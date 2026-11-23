#pragma once

/* E-paper screens. C interface over M5Unified (C++). All functions are
 * thread-safe and return after the panel has finished refreshing. */

#include "device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes M5Unified: display, and the power hold (GPIO 2) that keeps the
 * M5Paper on when it runs on battery. Call once, first thing in app_main. */
void screen_init(void);

/* A full-screen message, for example "Configuration saved, restarting". */
void screen_show_message(const char *text);

/* Setup mode: network name, password, URL and the two QR codes. */
void screen_show_setup(const char *ssid, const char *password, const char *url);

#ifdef __cplusplus
}
#endif
