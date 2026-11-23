/*
 * One authenticated WebSocket session with Home Assistant. Based on the
 * verified example in docs/dev/home-assistant-api.md, extended with
 * subscription events and explicit success/failure of commands.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "ha_internal.h"

static const char *TAG = "ha_ws";

#define EV_AUTH_OK      BIT0
#define EV_AUTH_INVALID BIT1
#define EV_RESULT       BIT2
#define EV_CLOSED       BIT3

#define CALL_TIMEOUT_MS 10000

struct ha_ws {
    esp_websocket_client_handle_t client;
    EventGroupHandle_t events;
    const char *token;
    ha_ws_event_fn on_event;
    void *ctx;
    char *frame;      /* text frame being reassembled */
    int next_id;      /* last id sent */
    int wait_id;      /* id whose result ha_ws_call() waits for */
    bool success;     /* of that result */
    cJSON *result;    /* "result" of that command */
    char error[96];   /* error message of that command */
};

static const char *str(const cJSON *obj, const char *key)
{
    const char *s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
    return s != NULL ? s : "";
}

static void send_json(ha_ws_t *ws, const cJSON *msg)
{
    char *text = cJSON_PrintUnformatted(msg);
    if (text != NULL) {
        esp_websocket_client_send_text(ws->client, text, strlen(text), pdMS_TO_TICKS(5000));
        cJSON_free(text);
    }
}

static void on_message(ha_ws_t *ws, cJSON *msg)
{
    const char *type = str(msg, "type");

    if (strcmp(type, "event") == 0) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
        if (ws->on_event != NULL && cJSON_IsNumber(id)) {
            ws->on_event(id->valueint, cJSON_GetObjectItemCaseSensitive(msg, "event"), ws->ctx);
        }
    } else if (strcmp(type, "result") == 0) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
        if (!cJSON_IsNumber(id) || id->valueint != ws->wait_id) {
            return;
        }
        ws->success = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(msg, "success"));
        if (ws->success) {
            ws->result = cJSON_DetachItemFromObjectCaseSensitive(msg, "result");
        } else {
            const cJSON *err = cJSON_GetObjectItemCaseSensitive(msg, "error");
            snprintf(ws->error, sizeof ws->error, "%s", str(err, "message"));
        }
        xEventGroupSetBits(ws->events, EV_RESULT);
    } else if (strcmp(type, "auth_required") == 0) {
        /* The auth message must be the first one we send and has no "id". */
        cJSON *auth = cJSON_CreateObject();
        cJSON_AddStringToObject(auth, "type", "auth");
        cJSON_AddStringToObject(auth, "access_token", ws->token);
        send_json(ws, auth);
        cJSON_Delete(auth);
    } else if (strcmp(type, "auth_ok") == 0) {
        xEventGroupSetBits(ws->events, EV_AUTH_OK);
    } else if (strcmp(type, "auth_invalid") == 0) {
        ESP_LOGE(TAG, "auth_invalid: %s", str(msg, "message"));
        xEventGroupSetBits(ws->events, EV_AUTH_INVALID);
    }
}

/* Runs in the WebSocket client's task. */
static void on_ws_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ha_ws_t *ws = arg;
    const esp_websocket_event_data_t *d = event_data;

    if (event_id == WEBSOCKET_EVENT_DISCONNECTED || event_id == WEBSOCKET_EVENT_ERROR ||
        event_id == WEBSOCKET_EVENT_CLOSED) {
        xEventGroupSetBits(ws->events, EV_CLOSED);
        return;
    }
    if (event_id != WEBSOCKET_EVENT_DATA || d->op_code != 0x01) {
        return; /* only text frames carry HA messages */
    }
    /* A frame larger than buffer_size arrives as several events. */
    if (d->payload_offset == 0) {
        free(ws->frame);
        ws->frame = malloc(d->payload_len + 1);
    }
    if (ws->frame == NULL) {
        return; /* out of memory: drop this frame */
    }
    memcpy(ws->frame + d->payload_offset, d->data_ptr, d->data_len);
    if (d->payload_offset + d->data_len < d->payload_len) {
        return;
    }
    ws->frame[d->payload_len] = '\0';
    cJSON *msg = cJSON_Parse(ws->frame);
    free(ws->frame);
    ws->frame = NULL;
    if (msg != NULL) {
        on_message(ws, msg);
        cJSON_Delete(msg);
    }
}

ha_ws_status_t ha_ws_connect(const char *url, const char *token, ha_ws_event_fn on_event,
                             void *ctx, ha_ws_t **out)
{
    *out = NULL;
    ha_ws_t *ws = calloc(1, sizeof *ws);
    if (ws == NULL) {
        return HA_WS_FAILED;
    }
    ws->events = xEventGroupCreate();
    ws->token = token;
    ws->on_event = on_event;
    ws->ctx = ctx;

    esp_websocket_client_config_t cfg = {
        .uri = url,
        .buffer_size = 4096,
        .task_stack = 8 * 1024, /* cJSON parses in this task */
        .disable_auto_reconnect = true,
        .network_timeout_ms = 10000,
        .ping_interval_sec = 30,
        .pingpong_timeout_sec = 90,
    };
    ws->client = esp_websocket_client_init(&cfg);
    if (ws->events == NULL || ws->client == NULL) {
        ha_ws_close(ws);
        return HA_WS_FAILED;
    }
    esp_websocket_register_events(ws->client, WEBSOCKET_EVENT_ANY, on_ws_event, ws);
    esp_websocket_client_start(ws->client);

    EventBits_t bits = xEventGroupWaitBits(ws->events, EV_AUTH_OK | EV_AUTH_INVALID | EV_CLOSED,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & EV_AUTH_OK) {
        *out = ws;
        return HA_WS_OK;
    }
    ha_ws_close(ws);
    return (bits & EV_AUTH_INVALID) ? HA_WS_AUTH_INVALID : HA_WS_FAILED;
}

bool ha_ws_call(ha_ws_t *ws, cJSON *cmd, cJSON **result, int *id, char *error, size_t error_size)
{
    if (result != NULL) {
        *result = NULL;
    }
    ws->wait_id = ++ws->next_id; /* ids must increase on one connection */
    if (id != NULL) {
        *id = ws->wait_id;
    }
    ws->success = false;
    ws->result = NULL;
    ws->error[0] = '\0';
    xEventGroupClearBits(ws->events, EV_RESULT);

    cJSON_AddNumberToObject(cmd, "id", ws->wait_id);
    send_json(ws, cmd);
    cJSON_Delete(cmd);

    EventBits_t bits = xEventGroupWaitBits(ws->events, EV_RESULT | EV_CLOSED, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(CALL_TIMEOUT_MS));
    ws->wait_id = 0;
    bool ok = (bits & EV_RESULT) && ws->success;
    if (!(bits & EV_RESULT)) {
        snprintf(ws->error, sizeof ws->error, "%s",
                 (bits & EV_CLOSED) ? "connection lost" : "no answer from Home Assistant");
    }
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", ws->error);
    }
    if (ok && result != NULL) {
        *result = ws->result;
    } else {
        cJSON_Delete(ws->result);
    }
    ws->result = NULL;
    return ok;
}

int ha_ws_next_id(const ha_ws_t *ws)
{
    return ws->next_id + 1;
}

bool ha_ws_alive(ha_ws_t *ws)
{
    return ws != NULL && !(xEventGroupGetBits(ws->events) & EV_CLOSED) &&
           esp_websocket_client_is_connected(ws->client);
}

void ha_ws_close(ha_ws_t *ws)
{
    if (ws == NULL) {
        return;
    }
    if (ws->client != NULL) {
        if (esp_websocket_client_is_connected(ws->client)) {
            esp_websocket_client_close(ws->client, pdMS_TO_TICKS(2000));
        }
        esp_websocket_client_destroy(ws->client);
    }
    if (ws->events != NULL) {
        vEventGroupDelete(ws->events);
    }
    free(ws->frame);
    cJSON_Delete(ws->result);
    free(ws);
}
