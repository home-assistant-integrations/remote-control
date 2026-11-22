#include "esp_check.h"
#include "esp_log.h"
#include "setup_internal.h"

static const char *TAG = "setup";

esp_err_t setup_mode_start(const setup_callbacks_t *callbacks)
{
    static char ssid[SETUP_SSID_SIZE];
    static char password[SETUP_PASSWORD_LEN + 1];

    setup_make_ssid(ssid, sizeof ssid);
    setup_make_password(password, sizeof password);

    ESP_RETURN_ON_ERROR(setup_wifi_start(ssid, password), TAG, "access point");
    ESP_RETURN_ON_ERROR(setup_dns_start(), TAG, "DNS");
    ESP_RETURN_ON_ERROR(setup_http_start(callbacks), TAG, "web server");

    if (callbacks != NULL && callbacks->on_ready != NULL) {
        callbacks->on_ready(ssid, password, SETUP_URL);
    }
    return ESP_OK;
}
