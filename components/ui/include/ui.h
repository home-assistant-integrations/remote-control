#pragma once

/*
 * The remote control UI: status bar, remotes, paging and the setup dialog.
 * Specified in openspec/changes/add-remote-ui/specs/remote-ui/spec.md and
 * status-bar/spec.md.
 */

#include <stdbool.h>

#include "device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the UI task and the Home Assistant connection. cfg must stay
 * valid. Call after screen_init(), nvs_flash_init(), esp_netif_init() and
 * esp_event_loop_create_default(). */
void ui_start(const config_t *cfg);

/* True once if the setup dialog asked for setup mode before the last
 * restart. Clears the request. */
bool ui_take_setup_request(void);

#ifdef __cplusplus
}
#endif
