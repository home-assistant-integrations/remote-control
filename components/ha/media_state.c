/*
 * Media player state from subscribe_entities events. Home Assistant sends a
 * compressed format:
 *   {"a": {entity: {"s": state, "a": {attributes}, ...}}}  complete states
 *   {"c": {entity: {"+": {"s":..., "a": {changed}}, "-": {"a": [removed]}}}}
 *   {"r": [entity, ...]}                                    removed entities
 * Pure C (cJSON and libc), tested on the host.
 */

#include <stdio.h>
#include <string.h>

#include "media_state.h"

static void copy_attr(char *dst, size_t size, const cJSON *attrs, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(attrs, key);
    if (v == NULL) {
        return; /* unchanged */
    }
    const char *s = cJSON_GetStringValue(v);
    snprintf(dst, size, "%s", s != NULL ? s : "");
}

static void apply(ha_media_t *m, const cJSON *s, const cJSON *attrs, bool replace)
{
    if (replace) {
        m->state[0] = m->title[0] = m->app[0] = m->picture[0] = '\0';
    }
    const char *state = cJSON_GetStringValue(s);
    if (state != NULL) {
        snprintf(m->state, sizeof m->state, "%s", state);
    }
    copy_attr(m->title, sizeof m->title, attrs, "media_title");
    copy_attr(m->app, sizeof m->app, attrs, "app_name");
    copy_attr(m->picture, sizeof m->picture, attrs, "entity_picture");
}

static void remove_attrs(ha_media_t *m, const cJSON *removed)
{
    const cJSON *name;
    cJSON_ArrayForEach(name, removed) {
        const char *n = cJSON_GetStringValue(name);
        if (n == NULL) {
            continue;
        }
        if (strcmp(n, "media_title") == 0) {
            m->title[0] = '\0';
        } else if (strcmp(n, "app_name") == 0) {
            m->app[0] = '\0';
        } else if (strcmp(n, "entity_picture") == 0) {
            m->picture[0] = '\0';
        }
    }
}

static ha_media_t *find(ha_media_t *list, int count, const char *entity_id)
{
    for (int i = 0; entity_id != NULL && i < count; i++) {
        if (strcmp(list[i].entity_id, entity_id) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

/* Reports m if a field that is shown on screen changed. */
static void report(const ha_media_t *before, const ha_media_t *m, media_changed_fn changed,
                   void *ctx)
{
    if (strcmp(before->state, m->state) != 0 || strcmp(before->title, m->title) != 0 ||
        strcmp(before->app, m->app) != 0 || strcmp(before->picture, m->picture) != 0) {
        changed(m, ctx);
    }
}

void media_state_apply(ha_media_t *list, int count, const cJSON *event, media_changed_fn changed,
                       void *ctx)
{
    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(event, "a")) {
        ha_media_t *m = find(list, count, item->string);
        if (m != NULL) {
            apply(m, cJSON_GetObjectItemCaseSensitive(item, "s"),
                  cJSON_GetObjectItemCaseSensitive(item, "a"), true);
            /* A complete state is always reported: it may be the first one. */
            changed(m, ctx);
        }
    }
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(event, "c")) {
        ha_media_t *m = find(list, count, item->string);
        if (m != NULL) {
            ha_media_t before = *m;
            const cJSON *plus = cJSON_GetObjectItemCaseSensitive(item, "+");
            const cJSON *minus = cJSON_GetObjectItemCaseSensitive(item, "-");
            apply(m, cJSON_GetObjectItemCaseSensitive(plus, "s"),
                  cJSON_GetObjectItemCaseSensitive(plus, "a"), false);
            remove_attrs(m, cJSON_GetObjectItemCaseSensitive(minus, "a"));
            report(&before, m, changed, ctx);
        }
    }
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(event, "r")) {
        ha_media_t *m = find(list, count, cJSON_GetStringValue(item));
        if (m != NULL) {
            ha_media_t before = *m;
            snprintf(m->state, sizeof m->state, "unavailable");
            m->title[0] = m->app[0] = m->picture[0] = '\0';
            report(&before, m, changed, ctx);
        }
    }
}
