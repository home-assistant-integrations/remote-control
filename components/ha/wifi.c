/*
 * Wi-Fi station with reconnect backoff (1 s doubling to 60 s).
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "ha_internal.h"

static const char *TAG = "wifi";

#define BIT_CONNECTED BIT0
#define BACKOFF_MIN_MS 1000
#define BACKOFF_MAX_MS 60000

static EventGroupHandle_t s_events;
static esp_timer_handle_t s_retry_timer;
static ha_event_fn s_on_event;
static int s_backoff_ms = BACKOFF_MIN_MS;

static void post(ha_event_kind_t kind)
{
    ha_event_t ev = {.kind = kind};
    s_on_event(&ev);
}

static void retry(void *arg)
{
    esp_wifi_connect();
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        bool was_connected = xEventGroupGetBits(s_events) & BIT_CONNECTED;
        xEventGroupClearBits(s_events, BIT_CONNECTED);
        if (was_connected) {
            post(HA_EV_WIFI_DOWN);
        }
        const wifi_event_sta_disconnected_t *d = data;
        ESP_LOGW(TAG, "disconnected (reason %d), retry in %d s", d->reason, s_backoff_ms / 1000);
        esp_timer_stop(s_retry_timer);
        esp_timer_start_once(s_retry_timer, (uint64_t)s_backoff_ms * 1000);
        s_backoff_ms = s_backoff_ms * 2 > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : s_backoff_ms * 2;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "connected, IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_backoff_ms = BACKOFF_MIN_MS;
        xEventGroupSetBits(s_events, BIT_CONNECTED);
        post(HA_EV_WIFI_UP);
    }
}

void wifi_start(const config_t *cfg, ha_event_fn on_event)
{
    s_on_event = on_event;
    s_events = xEventGroupCreate();
    const esp_timer_create_args_t targs = {.callback = retry, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_retry_timer));

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, cfg->wifi_ssid, sizeof wc.sta.ssid);
    strlcpy((char *)wc.sta.password, cfg->wifi_password, sizeof wc.sta.password);
    wc.sta.threshold.authmode = cfg->wifi_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wc.sta.pmf_cfg.capable = true;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Modem sleep keeps the WebSocket open while saving power. */
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    ESP_LOGI(TAG, "connecting to %s", cfg->wifi_ssid);
}

void wifi_wait_connected(void)
{
    xEventGroupWaitBits(s_events, BIT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
}

bool wifi_is_connected(void)
{
    return s_events != NULL && (xEventGroupGetBits(s_events) & BIT_CONNECTED);
}

int ha_wifi_rssi(void)
{
    wifi_ap_record_t ap;
    if (!wifi_is_connected() || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return 0;
    }
    return ap.rssi;
}
