/*
 * Keeps one Home Assistant session alive: authenticate, read the time zone
 * and the layout, subscribe to layout changes and media player states, and
 * run service calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ha_internal.h"
#include "media_state.h"

static const char *TAG = "ha";

#define BACKOFF_MIN_MS 1000
#define BACKOFF_MAX_MS 60000
#define MAX_MEDIA      16

typedef enum { CMD_CALL, CMD_RELOAD } cmd_kind_t;

typedef struct {
    cmd_kind_t kind;
    uint32_t request;
    call_t call;
} cmd_t;

static const config_t *s_cfg;
static ha_event_fn s_on_event;
static QueueHandle_t s_cmds;
static volatile bool s_connected;
static char s_ws_url[300];

static volatile int s_lovelace_sub; /* id of the lovelace_updated subscription */
static volatile int s_entities_sub; /* id of the subscribe_entities subscription */

/* Latest known state of the media players in the layout. */
static SemaphoreHandle_t s_media_lock;
static ha_media_t s_media[MAX_MEDIA];
static int s_media_count;

/* ---- helpers ---- */

static void post(ha_event_kind_t kind)
{
    ha_event_t ev = {.kind = kind};
    s_on_event(&ev);
}

static void post_call_result(uint32_t request, bool ok, const char *message)
{
    ha_event_t ev = {.kind = HA_EV_CALL_RESULT};
    ev.call.request = request;
    ev.call.ok = ok;
    snprintf(ev.call.message, sizeof ev.call.message, "%s", message != NULL ? message : "");
    s_on_event(&ev);
}

static char *dup_str(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d != NULL) {
        memcpy(d, s, n);
    }
    return d;
}

static void log_layout(const char *message)
{
    ESP_LOGW(TAG, "layout: %s", message);
}

void ha_event_free(ha_event_t *event)
{
    if (event->kind == HA_EV_LAYOUT && event->layout != NULL) {
        layout_free(event->layout);
        free(event->layout);
        event->layout = NULL;
    } else if (event->kind == HA_EV_MEDIA) {
        free(event->media);
        event->media = NULL;
    }
}

/* ---- media player state ---- */

static void post_media(const ha_media_t *m, void *ctx)
{
    ha_media_t *copy = malloc(sizeof *copy);
    if (copy == NULL) {
        return;
    }
    *copy = *m;
    ha_event_t ev = {.kind = HA_EV_MEDIA, .media = copy};
    s_on_event(&ev);
}

static ha_media_t *find_media(const char *entity_id)
{
    for (int i = 0; i < s_media_count; i++) {
        if (strcmp(s_media[i].entity_id, entity_id) == 0) {
            return &s_media[i];
        }
    }
    return NULL;
}

static void on_entities_event(const cJSON *event)
{
    xSemaphoreTake(s_media_lock, portMAX_DELAY);
    media_state_apply(s_media, s_media_count, event, post_media, NULL);
    xSemaphoreGive(s_media_lock);
}

/* Runs in the WebSocket task. */
static void on_ws_event(int subscription, const cJSON *event, void *ctx)
{
    if (subscription == s_entities_sub && s_entities_sub != 0) {
        on_entities_event(event);
    } else if (subscription == s_lovelace_sub && s_lovelace_sub != 0) {
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(event, "data");
        const char *path = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(data, "url_path"));
        if (path != NULL && strcmp(path, s_cfg->ha_dashboard) == 0) {
            ESP_LOGI(TAG, "dashboard %s changed, reloading", path);
            cmd_t cmd = {.kind = CMD_RELOAD};
            xQueueSend(s_cmds, &cmd, 0);
        }
    }
}

/* ---- session steps (ha task) ---- */

static void read_time_zone(ha_ws_t *ws)
{
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "get_config");
    cJSON *result = NULL;
    char err[96];
    if (!ha_ws_call(ws, cmd, &result, NULL, err, sizeof err)) {
        ESP_LOGW(TAG, "get_config failed: %s", err);
        return;
    }
    const char *tz = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(result, "time_zone"));
    if (tz != NULL) {
        ha_event_t ev = {.kind = HA_EV_TIMEZONE};
        snprintf(ev.tz, sizeof ev.tz, "%s", tz);
        s_on_event(&ev);
    }
    cJSON_Delete(result);
}

static void subscribe_media(ha_ws_t *ws, const layout_t *layout)
{
    char err[96];
    if (s_entities_sub != 0) {
        cJSON *cmd = cJSON_CreateObject();
        cJSON_AddStringToObject(cmd, "type", "unsubscribe_events");
        cJSON_AddNumberToObject(cmd, "subscription", s_entities_sub);
        s_entities_sub = 0;
        ha_ws_call(ws, cmd, NULL, NULL, err, sizeof err);
    }

    xSemaphoreTake(s_media_lock, portMAX_DELAY);
    s_media_count = 0;
    for (int r = 0; r < layout->count; r++) {
        const remote_t *rem = &layout->remotes[r];
        for (int e = 0; e < rem->count; e++) {
            const element_t *el = &rem->elements[e];
            if (el->kind == ELEMENT_MEDIA && el->entity != NULL && s_media_count < MAX_MEDIA &&
                find_media(el->entity) == NULL) {
                ha_media_t *m = &s_media[s_media_count++];
                memset(m, 0, sizeof *m);
                snprintf(m->entity_id, sizeof m->entity_id, "%s", el->entity);
            }
        }
    }
    cJSON *ids = cJSON_CreateArray();
    for (int i = 0; i < s_media_count; i++) {
        cJSON_AddItemToArray(ids, cJSON_CreateString(s_media[i].entity_id));
    }
    int count = s_media_count;
    xSemaphoreGive(s_media_lock);

    if (count == 0) {
        cJSON_Delete(ids);
        return;
    }
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "subscribe_entities");
    cJSON_AddItemToObject(cmd, "entity_ids", ids);
    /* Set the id before sending: the first event (the full states) can
     * arrive before ha_ws_call() returns. */
    s_entities_sub = ha_ws_next_id(ws);
    if (!ha_ws_call(ws, cmd, NULL, NULL, err, sizeof err)) {
        s_entities_sub = 0;
        ESP_LOGW(TAG, "subscribe_entities failed: %s", err);
    }
}

static void read_layout(ha_ws_t *ws)
{
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "lovelace/config");
    cJSON_AddStringToObject(cmd, "url_path", s_cfg->ha_dashboard);
    cJSON *result = NULL;
    char err[96];
    if (!ha_ws_call(ws, cmd, &result, NULL, err, sizeof err)) {
        ESP_LOGW(TAG, "lovelace/config %s failed: %s", s_cfg->ha_dashboard, err);
        post(HA_EV_DASHBOARD_MISSING);
        return;
    }
    layout_t *layout = calloc(1, sizeof *layout);
    bool ok = layout != NULL && layout_parse(result, layout, log_layout);
    cJSON_Delete(result);
    if (!ok) {
        free(layout);
        ESP_LOGE(TAG, "layout could not be built (out of memory)");
        return;
    }
    ESP_LOGI(TAG, "layout: %d remotes", layout->count);
    subscribe_media(ws, layout);
    ha_event_t ev = {.kind = HA_EV_LAYOUT, .layout = layout};
    s_on_event(&ev);
}

static void subscribe_lovelace(ha_ws_t *ws)
{
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "subscribe_events");
    cJSON_AddStringToObject(cmd, "event_type", "lovelace_updated");
    char err[96];
    s_lovelace_sub = ha_ws_next_id(ws);
    if (!ha_ws_call(ws, cmd, NULL, NULL, err, sizeof err)) {
        s_lovelace_sub = 0;
        ESP_LOGW(TAG, "subscribe lovelace_updated failed: %s", err);
    }
}

static void run_call(ha_ws_t *ws, cmd_t *c)
{
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "call_service");
    cJSON_AddStringToObject(cmd, "domain", c->call.domain);
    cJSON_AddStringToObject(cmd, "service", c->call.service);
    cJSON *data = cJSON_Parse(c->call.service_data_json);
    cJSON *target = cJSON_Parse(c->call.target_json);
    cJSON_AddItemToObject(cmd, "service_data", data != NULL ? data : cJSON_CreateObject());
    cJSON_AddItemToObject(cmd, "target", target != NULL ? target : cJSON_CreateObject());
    char err[96];
    bool ok = ha_ws_call(ws, cmd, NULL, NULL, err, sizeof err);
    if (!ok) {
        ESP_LOGW(TAG, "%s.%s failed: %s", c->call.domain, c->call.service, err);
    }
    post_call_result(c->request, ok, ok ? NULL : err);
}

/* Answers queued calls with "not connected" after the connection dropped. */
static void fail_queued_calls(void)
{
    cmd_t c;
    while (xQueueReceive(s_cmds, &c, 0)) {
        if (c.kind == CMD_CALL) {
            post_call_result(c.request, false, "Not connected");
            call_free(&c.call);
        }
    }
}

static void ha_task(void *arg)
{
    int backoff = BACKOFF_MIN_MS;
    for (;;) {
        wifi_wait_connected();
        ha_ws_t *ws = NULL;
        ESP_LOGI(TAG, "connecting to %s", s_ws_url);
        ha_ws_status_t st = ha_ws_connect(s_ws_url, s_cfg->ha_token, on_ws_event, NULL, &ws);
        if (st == HA_WS_AUTH_INVALID) {
            post(HA_EV_TOKEN_REJECTED);
            ESP_LOGE(TAG, "token rejected: not reconnecting until the configuration changes");
            vTaskSuspend(NULL);
        }
        if (st != HA_WS_OK) {
            post(HA_EV_DISCONNECTED);
            ESP_LOGW(TAG, "connection failed, retry in %d s", backoff / 1000);
            vTaskDelay(pdMS_TO_TICKS(backoff));
            backoff = backoff * 2 > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : backoff * 2;
            continue;
        }

        backoff = BACKOFF_MIN_MS;
        s_entities_sub = 0;
        s_lovelace_sub = 0;
        s_connected = true;
        post(HA_EV_CONNECTED);
        read_time_zone(ws);
        subscribe_lovelace(ws);
        read_layout(ws);

        while (ha_ws_alive(ws)) {
            cmd_t c;
            if (!xQueueReceive(s_cmds, &c, pdMS_TO_TICKS(1000))) {
                continue;
            }
            if (c.kind == CMD_RELOAD) {
                read_layout(ws);
            } else {
                run_call(ws, &c);
                call_free(&c.call);
            }
        }

        s_connected = false;
        s_entities_sub = 0;
        s_lovelace_sub = 0;
        ha_ws_close(ws);
        fail_queued_calls();
        post(HA_EV_DISCONNECTED);
        ESP_LOGW(TAG, "connection lost, reconnecting");
        vTaskDelay(pdMS_TO_TICKS(BACKOFF_MIN_MS));
    }
}

/* ---- public API ---- */

void ha_start(const config_t *cfg, ha_event_fn on_event)
{
    s_cfg = cfg;
    s_on_event = on_event;
    s_cmds = xQueueCreate(8, sizeof(cmd_t));
    s_media_lock = xSemaphoreCreateMutex();

    /* http://host:port -> ws://host:port/api/websocket */
    bool tls = strncmp(cfg->ha_url, "https://", 8) == 0;
    snprintf(s_ws_url, sizeof s_ws_url, "%s%s/api/websocket", tls ? "wss://" : "ws://",
             cfg->ha_url + (tls ? 8 : 7));

    wifi_start(cfg, on_event);
    xTaskCreate(ha_task, "ha", 6144, NULL, 5, NULL);
}

bool ha_call(uint32_t request, const call_t *call)
{
    if (!s_connected || call == NULL || call->domain == NULL) {
        return false;
    }
    cmd_t c = {.kind = CMD_CALL, .request = request};
    c.call.domain = dup_str(call->domain);
    c.call.service = dup_str(call->service);
    c.call.service_data_json = dup_str(call->service_data_json);
    c.call.target_json = dup_str(call->target_json);
    if (c.call.domain == NULL || c.call.service == NULL || c.call.service_data_json == NULL ||
        c.call.target_json == NULL || xQueueSend(s_cmds, &c, 0) != pdTRUE) {
        call_free(&c.call);
        return false;
    }
    return true;
}
