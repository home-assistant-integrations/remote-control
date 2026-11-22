#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "setup_mode.h"

#define SETUP_SSID_SIZE    33
#define SETUP_PASSWORD_LEN 12

void setup_make_ssid(char *ssid, size_t size);
void setup_make_password(char *password, size_t size);
esp_err_t setup_wifi_start(const char *ssid, const char *password);
esp_err_t setup_dns_start(void);
esp_err_t setup_http_start(const setup_callbacks_t *callbacks);
