/*
 * Dashboard actions to service calls. The rules are those of
 * docs/dev/home-assistant-api.md ("Mapping dashboard actions to REST
 * calls"); the result is sent as a WebSocket call_service message.
 */

#include <stdlib.h>
#include <string.h>

#include "layout.h"

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d != NULL) {
        memcpy(d, s, n);
    }
    return d;
}

static char *print_or_empty(const cJSON *obj)
{
    if (cJSON_IsObject(obj)) {
        char *s = cJSON_PrintUnformatted(obj);
        if (s != NULL) {
            /* cJSON may use its own allocator; copy into a malloc'ed string */
            char *d = dup_str(s);
            cJSON_free(s);
            return d;
        }
        return NULL;
    }
    return dup_str("{}");
}

void call_free(call_t *call)
{
    if (call == NULL) {
        return;
    }
    free(call->domain);
    free(call->service);
    free(call->service_data_json);
    free(call->target_json);
    memset(call, 0, sizeof *call);
}

static bool fill(call_t *out, const char *domain, size_t domain_len, const char *service,
                 const cJSON *data, const cJSON *target)
{
    out->domain = malloc(domain_len + 1);
    if (out->domain != NULL) {
        memcpy(out->domain, domain, domain_len);
        out->domain[domain_len] = '\0';
    }
    out->service = dup_str(service);
    out->service_data_json = print_or_empty(data);
    out->target_json = print_or_empty(target);
    if (!out->domain || !out->service || !out->service_data_json || !out->target_json) {
        call_free(out);
        return false;
    }
    return true;
}

bool action_to_call(const cJSON *action, const char *entity, call_t *out)
{
    memset(out, 0, sizeof *out);
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(action, "action"));
    if (type == NULL || strcmp(type, "none") == 0) {
        return false;
    }

    if (strcmp(type, "toggle") == 0) {
        if (entity == NULL || entity[0] == '\0') {
            return false;
        }
        cJSON *target = cJSON_CreateObject();
        cJSON_AddStringToObject(target, "entity_id", entity);
        bool ok = fill(out, "homeassistant", strlen("homeassistant"), "toggle", NULL, target);
        cJSON_Delete(target);
        return ok;
    }

    const char *svc;
    const cJSON *data;
    if (strcmp(type, "perform-action") == 0) {
        svc = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(action, "perform_action"));
        data = cJSON_GetObjectItemCaseSensitive(action, "data");
    } else if (strcmp(type, "call-service") == 0) { /* legacy name */
        svc = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(action, "service"));
        data = cJSON_GetObjectItemCaseSensitive(action, "service_data");
    } else {
        return false; /* more-info, navigate, url, ...: frontend only */
    }

    const char *dot = svc != NULL ? strchr(svc, '.') : NULL;
    if (dot == NULL || dot == svc || dot[1] == '\0') {
        return false;
    }
    return fill(out, svc, (size_t)(dot - svc), dot + 1, data,
                cJSON_GetObjectItemCaseSensitive(action, "target"));
}
