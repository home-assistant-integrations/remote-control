/*
 * Downloads cover art (entity_picture) in its own task. entity_picture
 * carries its own token, so no Authorization header is sent.
 */

#include "ui_internal.hpp"

#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "cover";

namespace {

constexpr size_t MAX_COVER = 200 * 1024;

struct Request {
    char entity[96];
    char picture[384];
};

QueueHandle_t requests;
const config_t *cfg;

struct Body {
    uint8_t *data;
    size_t len;
    bool too_big;
};

esp_err_t on_http(esp_http_client_event_t *e)
{
    Body *b = static_cast<Body *>(e->user_data);
    if (e->event_id != HTTP_EVENT_ON_DATA || b->too_big) {
        return ESP_OK;
    }
    if (b->len + e->data_len > MAX_COVER) {
        b->too_big = true;
        return ESP_OK;
    }
    auto *p = static_cast<uint8_t *>(realloc(b->data, b->len + e->data_len));
    if (p == nullptr) {
        b->too_big = true;
        return ESP_OK;
    }
    memcpy(p + b->len, e->data, e->data_len);
    b->data = p;
    b->len += e->data_len;
    return ESP_OK;
}

void cover_task(void *)
{
    for (;;) {
        Request r;
        xQueueReceive(requests, &r, portMAX_DELAY);
        char url[512];
        if (!ui_cover_url(cfg->ha_url, r.picture, url, sizeof url)) {
            ESP_LOGW(TAG, "%s: cover art not from Home Assistant, not fetched", r.entity);
            continue;
        }
        Body body = {};
        esp_http_client_config_t hc = {};
        hc.url = url;
        hc.timeout_ms = 10000;
        hc.event_handler = on_http;
        hc.user_data = &body;
        esp_http_client_handle_t c = esp_http_client_init(&hc);
        int status = -1;
        if (c != nullptr && esp_http_client_perform(c) == ESP_OK) {
            status = esp_http_client_get_status_code(c);
        }
        if (c != nullptr) {
            esp_http_client_cleanup(c);
        }
        if (status != 200 || body.too_big || body.len == 0) {
            ESP_LOGW(TAG, "%s: no cover (HTTP %d, %u bytes%s)", r.entity, status,
                     (unsigned)body.len, body.too_big ? ", too big" : "");
            free(body.data);
            continue;
        }
        UiMsg msg = {};
        msg.kind = UiMsgKind::Cover;
        snprintf(msg.cover.entity, sizeof msg.cover.entity, "%s", r.entity);
        msg.cover.data = body.data;
        msg.cover.len = body.len;
        ui_post(msg);
    }
}

}  // namespace

void cover_start(const config_t *c)
{
    cfg = c;
    requests = xQueueCreate(4, sizeof(Request));
    xTaskCreate(cover_task, "cover", 6144, nullptr, 3, nullptr);
}

void cover_request(const char *entity_id, const char *picture)
{
    Request r = {};
    snprintf(r.entity, sizeof r.entity, "%s", entity_id);
    snprintf(r.picture, sizeof r.picture, "%s", picture);
    xQueueSend(requests, &r, 0);
}
