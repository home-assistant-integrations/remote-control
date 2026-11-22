/*
 * Access point for setup mode, and its network name and password.
 */

#include <stdio.h>
#include <string.h>

#include "bootloader_random.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "setup_internal.h"

static const char *TAG = "setup";

/* No look-alike characters: 0 O 1 l I are left out. */
static const char ALPHABET[] = "abcdefghijkmnopqrstuvwxyz"
                               "ABCDEFGHJKLMNPQRSTUVWXYZ"
                               "23456789";

void setup_make_ssid(char *ssid, size_t size)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(ssid, size, "RemoteControl-%02X%02X", mac[4], mac[5]);
}

void setup_make_password(char *password, size_t size)
{
    const size_t n = sizeof ALPHABET - 1;
    /* Largest multiple of n below 256, to pick characters without bias. */
    const uint32_t limit = 256 - (256 % n);

    /* The radio is not running yet, so the hardware RNG needs this mode to
     * deliver true random numbers. */
    bootloader_random_enable();
    size_t i = 0;
    while (i + 1 < size && i < SETUP_PASSWORD_LEN) {
        uint32_t r = esp_random() & 0xFF;
        if (r < limit) {
            password[i++] = ALPHABET[r % n];
        }
    }
    bootloader_random_disable();
    password[i] = '\0';
}

esp_err_t setup_wifi_start(const char *ssid, const char *password)
{
    esp_netif_create_default_wifi_ap(); /* 192.168.4.1 with a DHCP server */

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "esp_wifi_init");

    wifi_config_t cfg = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.required = false},
        },
    };
    strlcpy((char *)cfg.ap.ssid, ssid, sizeof cfg.ap.ssid);
    cfg.ap.ssid_len = strlen(ssid);
    strlcpy((char *)cfg.ap.password, password, sizeof cfg.ap.password);

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "esp_wifi_set_mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &cfg), TAG, "esp_wifi_set_config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start");

    ESP_LOGI(TAG, "access point %s started", ssid); /* the password stays off the log */
    return ESP_OK;
}
