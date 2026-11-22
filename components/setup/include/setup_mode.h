#pragma once

/*
 * Setup mode: a WPA2 access point, a captive-portal DNS server and a web
 * page for uploading the configuration file. Specified in
 * openspec/specs/setup-mode/spec.md.
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SETUP_URL "http://192.168.4.1/"

typedef struct {
    /* Called once the access point runs, with what the screen must show. */
    void (*on_ready)(const char *ssid, const char *password, const char *url);
    /* Called after a valid configuration was stored, just before the
     * restart. */
    void (*on_saved)(void);
} setup_callbacks_t;

/*
 * Starts setup mode and returns. The device restarts by itself after a
 * valid configuration has been uploaded. Requires nvs_flash_init(),
 * esp_netif_init(), esp_event_loop_create_default() and config_fs_init().
 */
esp_err_t setup_mode_start(const setup_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif
