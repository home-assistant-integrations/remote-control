#pragma once

/*
 * Device configuration: format, validation and storage.
 *
 * The format and the rules are specified in
 * openspec/specs/device-configuration/spec.md and documented for users in
 * docs/user/configuration.md.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Largest accepted configuration file, in bytes. */
#define CONFIG_MAX_SIZE 4096

/* Errors kept per validation; further errors are counted but not stored. */
#define CONFIG_MAX_ERRORS 16
#define CONFIG_ERROR_LEN 96

/* A validated configuration. All strings are heap-allocated and
 * NUL-terminated; free them with config_free(). */
typedef struct {
    char *wifi_ssid;
    char *wifi_password; /* "" for an open network */
    char *ha_url;        /* normalized: no trailing slash */
    char *ha_token;
    char *ha_dashboard;
} config_t;

/* Validation errors, each in the form "<dotted.field>: <rule>". Messages
 * never contain field values, so they are safe to log and to return over
 * HTTP. */
typedef struct {
    int count; /* may exceed CONFIG_MAX_ERRORS */
    char msg[CONFIG_MAX_ERRORS][CONFIG_ERROR_LEN];
} config_errors_t;

/*
 * Checks a configuration file against every rule and collects all errors.
 * On success returns true and, if out is not NULL, fills it. On failure
 * returns false and leaves out zeroed. errs must not be NULL.
 */
bool config_validate(const char *json, size_t len, config_t *out, config_errors_t *errs);

void config_free(config_t *cfg);

/* --- Storage (ESP-IDF only) ------------------------------------------- */

#ifdef ESP_PLATFORM
#include "esp_err.h"

/* Mounts the "config" partition with LittleFS, formatting it if it does not
 * mount. */
esp_err_t config_fs_init(void);

/* Atomically replaces config.json with the given, already validated file. */
esp_err_t config_store(const char *json, size_t len);

/* Loads and validates config.json. Returns ESP_ERR_NOT_FOUND if there is
 * none, ESP_ERR_INVALID_STATE if it fails validation (errors are logged
 * without values), ESP_OK on success. */
esp_err_t config_load(config_t *out);
#endif

#ifdef __cplusplus
}
#endif
