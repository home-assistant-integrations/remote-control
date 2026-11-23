#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "ha.h"

/* ---- Wi-Fi (wifi.c) ---- */

void wifi_start(const config_t *cfg, ha_event_fn on_event);
/* Blocks until Wi-Fi has an IP address. */
void wifi_wait_connected(void);
bool wifi_is_connected(void);

/* ---- WebSocket session (ha_ws.c) ---- */

typedef struct ha_ws ha_ws_t;

/* Called in the WebSocket task for every "event" message. */
typedef void (*ha_ws_event_fn)(int subscription, const cJSON *event, void *ctx);

typedef enum {
    HA_WS_OK,
    HA_WS_AUTH_INVALID,
    HA_WS_FAILED,
} ha_ws_status_t;

ha_ws_status_t ha_ws_connect(const char *url, const char *token, ha_ws_event_fn on_event,
                             void *ctx, ha_ws_t **out);

/* Sends cmd (takes ownership, adds "id") and waits for its result.
 * Returns true on success; *result receives the "result" member (may be NULL,
 * free with cJSON_Delete). On failure, error receives the error message.
 * *id receives the command id, which is the subscription id for
 * subscribe_* commands. */
bool ha_ws_call(ha_ws_t *ws, cJSON *cmd, cJSON **result, int *id, char *error, size_t error_size);

/* The id the next ha_ws_call() will use. Subscribers store it before the
 * call, because the first event can arrive before the call returns. */
int ha_ws_next_id(const ha_ws_t *ws);

bool ha_ws_alive(ha_ws_t *ws);
void ha_ws_close(ha_ws_t *ws);
