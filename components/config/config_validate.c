/*
 * Validation of the device configuration file. Pure C (cJSON and libc), so
 * it also builds on the host for unit tests (`make test-host`).
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "device_config.h"

/* ---- errors ---- */

static void add_error(config_errors_t *errs, const char *fmt, ...)
{
    if (errs->count < CONFIG_MAX_ERRORS) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(errs->msg[errs->count], CONFIG_ERROR_LEN, fmt, ap);
        va_end(ap);
    }
    errs->count++;
}

/* ---- helpers ---- */

/* Returns the number of code points, or -1 if s is not valid UTF-8. */
static long utf8_length(const char *s, size_t len)
{
    long count = 0;
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        size_t n;
        uint32_t cp;
        if (c < 0x80) {
            n = 1;
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            n = 2;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            n = 3;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            n = 4;
            cp = c & 0x07;
        } else {
            return -1;
        }
        if (i + n > len) {
            return -1;
        }
        for (size_t k = 1; k < n; k++) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0) != 0x80) {
                return -1;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        /* reject overlong forms, surrogates and values above U+10FFFF */
        if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
            return -1;
        }
        i += n;
        count++;
    }
    return count;
}

/* Reports keys of obj that are not in allowed (NULL-terminated). */
static void check_keys(const cJSON *obj, const char *prefix, const char *const *allowed,
                       config_errors_t *errs)
{
    const cJSON *item;
    cJSON_ArrayForEach(item, obj) {
        bool known = false;
        for (const char *const *a = allowed; *a != NULL; a++) {
            if (strcmp(item->string, *a) == 0) {
                known = true;
                break;
            }
        }
        if (!known) {
            add_error(errs, "%s%s: unknown key", prefix, item->string);
        }
    }
}

/* Returns the string member key of obj, or NULL after reporting why not. */
static const char *get_string(const cJSON *obj, const char *path, const char *key,
                              config_errors_t *errs)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        add_error(errs, "%s: missing", path);
        return NULL;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        add_error(errs, "%s: must be a string", path);
        return NULL;
    }
    return item->valuestring;
}

/* Returns the object member key of obj, or NULL after reporting why not. */
static const cJSON *get_object(const cJSON *obj, const char *key, config_errors_t *errs)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        add_error(errs, "%s: missing", key);
        return NULL;
    }
    if (!cJSON_IsObject(item)) {
        add_error(errs, "%s: must be an object", key);
        return NULL;
    }
    return item;
}

static bool is_host_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '.';
}

static bool is_base64url_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_';
}

/* ---- field rules ---- */

static void check_ssid(const char *v, config_errors_t *errs)
{
    size_t n = strlen(v);
    if (n < 1 || n > 32) {
        add_error(errs, "wifi.ssid: must be 1 to 32 bytes");
    }
}

static void check_password(const char *v, config_errors_t *errs)
{
    size_t bytes = strlen(v);
    long chars = utf8_length(v, bytes);
    /* WPA2 allows 8 to 63 characters; the Wi-Fi driver stores at most 63
     * bytes, so the upper bound is checked in bytes as well. */
    if (bytes != 0 && (chars < 8 || chars > 63 || bytes > 63)) {
        add_error(errs, "wifi.password: must be empty or 8 to 63 characters");
    }
}

/* Validates the URL and returns a normalized copy (malloc), or NULL. */
static char *check_url(const char *v, config_errors_t *errs)
{
    const char *p;
    if (strncmp(v, "http://", 7) == 0) {
        p = v + 7;
    } else if (strncmp(v, "https://", 8) == 0) {
        p = v + 8;
    } else {
        add_error(errs, "home_assistant.url: must start with http:// or https://");
        return NULL;
    }

    size_t len = strlen(v);
    if (len > 0 && v[len - 1] == '/') {
        len--; /* a single trailing slash is allowed and removed */
    }
    const char *end = v + len;

    /* host: a name/IPv4 address, or an IPv6 address in brackets */
    const char *h = p;
    if (h < end && *h == '[') {
        h++;
        while (h < end && *h != ']') {
            char c = *h;
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
                  c == ':' || c == '.')) {
                break;
            }
            h++;
        }
        if (h >= end || *h != ']' || h == p + 1) {
            add_error(errs, "home_assistant.url: must have a valid host");
            return NULL;
        }
        h++;
    } else {
        while (h < end && is_host_char(*h)) {
            h++;
        }
        if (h == p) {
            add_error(errs, "home_assistant.url: must have a valid host");
            return NULL;
        }
    }

    /* optional port */
    if (h < end && *h == ':') {
        const char *d = ++h;
        long port = 0;
        while (h < end && *h >= '0' && *h <= '9' && port <= 65535) {
            port = port * 10 + (*h - '0');
            h++;
        }
        if (h == d || port < 1 || port > 65535 || (h < end && *h >= '0' && *h <= '9')) {
            add_error(errs, "home_assistant.url: port must be a number from 1 to 65535");
            return NULL;
        }
    }

    if (h != end) {
        if (*h == '/' || *h == '?' || *h == '#') {
            add_error(errs, "home_assistant.url: must not have a path, query or fragment");
        } else {
            add_error(errs, "home_assistant.url: must have a valid host");
        }
        return NULL;
    }

    char *out = malloc(len + 1);
    if (out != NULL) {
        memcpy(out, v, len);
        out[len] = '\0';
    }
    return out;
}

static void check_token(const char *v, config_errors_t *errs)
{
    int parts = 1;
    size_t part_len = 0;
    bool ok = true;
    for (const char *c = v; *c != '\0'; c++) {
        if (*c == '.') {
            if (part_len == 0) {
                ok = false;
            }
            parts++;
            part_len = 0;
        } else if (is_base64url_char(*c)) {
            part_len++;
        } else {
            ok = false; /* whitespace or any other character */
        }
    }
    if (part_len == 0 || parts != 3) {
        ok = false;
    }
    if (!ok) {
        add_error(errs, "home_assistant.token: must be three base64url parts separated by dots");
    }
}

static void check_dashboard(const char *v, config_errors_t *errs)
{
    size_t n = strlen(v);
    bool ok = n >= 1 && n <= 64;
    for (size_t i = 0; ok && i < n; i++) {
        char c = v[i];
        ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    }
    if (!ok) {
        add_error(errs,
                  "home_assistant.dashboard: must be 1 to 64 characters from a-z, 0-9, - and _");
    }
}

/* ---- entry points ---- */

static char *dup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d != NULL) {
        memcpy(d, s, n);
    }
    return d;
}

bool config_validate(const char *json, size_t len, config_t *out, config_errors_t *errs)
{
    static const char *const top_keys[] = {"version", "wifi", "home_assistant", NULL};
    static const char *const wifi_keys[] = {"ssid", "password", NULL};
    static const char *const ha_keys[] = {"url", "token", "dashboard", NULL};

    memset(errs, 0, sizeof *errs);
    if (out != NULL) {
        memset(out, 0, sizeof *out);
    }

    if (len > CONFIG_MAX_SIZE) {
        add_error(errs, "file: larger than %d bytes", CONFIG_MAX_SIZE);
        return false;
    }
    if (json == NULL || utf8_length(json, len) < 0) {
        add_error(errs, "file: not valid UTF-8");
        return false;
    }

    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, len, &parse_end, false);
    if (root != NULL) {
        /* only whitespace may follow the JSON value */
        const char *e = json + len;
        for (const char *c = parse_end; c < e; c++) {
            if (*c != ' ' && *c != '\t' && *c != '\r' && *c != '\n') {
                cJSON_Delete(root);
                root = NULL;
                break;
            }
        }
    }
    if (root == NULL) {
        add_error(errs, "file: not valid JSON");
        return false;
    }
    if (!cJSON_IsObject(root)) {
        add_error(errs, "file: must be a JSON object");
        cJSON_Delete(root);
        return false;
    }

    check_keys(root, "", top_keys, errs);

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (version == NULL) {
        add_error(errs, "version: missing");
    } else if (!cJSON_IsNumber(version) || version->valuedouble != 1.0) {
        add_error(errs, "version: must be 1");
    }

    const char *ssid = NULL, *password = NULL, *url = NULL, *token = NULL, *dashboard = NULL;
    char *norm_url = NULL;

    const cJSON *wifi = get_object(root, "wifi", errs);
    if (wifi != NULL) {
        check_keys(wifi, "wifi.", wifi_keys, errs);
        if ((ssid = get_string(wifi, "wifi.ssid", "ssid", errs)) != NULL) {
            check_ssid(ssid, errs);
        }
        if ((password = get_string(wifi, "wifi.password", "password", errs)) != NULL) {
            check_password(password, errs);
        }
    }

    const cJSON *ha = get_object(root, "home_assistant", errs);
    if (ha != NULL) {
        check_keys(ha, "home_assistant.", ha_keys, errs);
        if ((url = get_string(ha, "home_assistant.url", "url", errs)) != NULL) {
            norm_url = check_url(url, errs);
        }
        if ((token = get_string(ha, "home_assistant.token", "token", errs)) != NULL) {
            check_token(token, errs);
        }
        if ((dashboard = get_string(ha, "home_assistant.dashboard", "dashboard", errs)) != NULL) {
            check_dashboard(dashboard, errs);
        }
    }

    bool ok = errs->count == 0;
    if (ok && out != NULL) {
        out->wifi_ssid = dup(ssid);
        out->wifi_password = dup(password);
        out->ha_url = norm_url;
        norm_url = NULL;
        out->ha_token = dup(token);
        out->ha_dashboard = dup(dashboard);
        if (!out->wifi_ssid || !out->wifi_password || !out->ha_url || !out->ha_token ||
            !out->ha_dashboard) {
            config_free(out);
            add_error(errs, "file: out of memory");
            ok = false;
        }
    }
    free(norm_url);
    cJSON_Delete(root);
    return ok;
}

void config_free(config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    free(cfg->wifi_ssid);
    if (cfg->wifi_password != NULL) {
        memset(cfg->wifi_password, 0, strlen(cfg->wifi_password));
        free(cfg->wifi_password);
    }
    free(cfg->ha_url);
    if (cfg->ha_token != NULL) {
        memset(cfg->ha_token, 0, strlen(cfg->ha_token));
        free(cfg->ha_token);
    }
    free(cfg->ha_dashboard);
    memset(cfg, 0, sizeof *cfg);
}
