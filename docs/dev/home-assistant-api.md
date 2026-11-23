# Home Assistant API for Remote Controls

This document describes how `remote-control` reads remote control definitions
and states from Home Assistant (HA), and how it triggers the actions behind the
buttons.

A remote control is a Lovelace dashboard (typically built with Bubble Card)
whose cards hold one action per button. The firmware reads the dashboard
configuration, renders the buttons, and turns each button's action into a REST
call.

All requests and responses below were checked against HA 2026.11.2.

## Conventions

Placeholders used below:

| Placeholder | Meaning | Example |
| --- | --- | --- |
| `<ha>` | HA base URL: `scheme://host[:port]`, no trailing slash | `http://homeassistant.local:8123` |
| `<ha-ws>` | WebSocket URL: `<ha>` with `http` → `ws` (`https` → `wss`), plus `/api/websocket` | `ws://homeassistant.local:8123/api/websocket` |
| `<token>` | Long-lived access token (HA profile → Security) | `eyJhbGciOi…` |
| `<dashboard>` | URL path of the remote's dashboard | `media-control` |
| `<media_player>` | Media player entity of the device | `media_player.living_room_tv` |
| `<button>` | Button entity of the device | `button.living_room_tv_select` |

The default port is 8123. Behind a reverse proxy HA often answers on port 80
or 443 instead, and 8123 refuses the connection. Check which port works with
`curl <ha>/api/` (answers `401: Unauthorized` without a token).

## Authentication

REST and WebSocket use the same long-lived token, but send it differently.

The token is a JWT of about 180 characters with two dots in it. It must be
sent complete, on one line, without spaces or line breaks. A truncated token
fails the same way as a wrong one.

### REST

Every REST call sends these headers:

```text
Authorization: Bearer <token>
Content-Type: application/json
```

`Content-Type` is only needed for requests with a body. Each request is
authenticated on its own.

### WebSocket

The WebSocket API does not read the `Authorization` header. The token goes into
the first message, and only after HA asks for it:

```text
← {"type":"auth_required","ha_version":"…"}
→ {"type":"auth","access_token":"<token>"}
← {"type":"auth_ok","ha_version":"…"}
→ {"id":1,"type":"…"}
```

Rules:

- Authentication applies to one connection only. Every new connection must
  authenticate again. A command sent on a fresh connection without `auth` is
  rejected and the connection is closed.
- The `auth` message must be the first message the client sends.
- The `auth` message has no `id`. HA rejects it if it has one.
- Commands start after `auth_ok`. Each command has an `id`, and `id`s must
  increase within one connection.
- After `auth_invalid`, HA closes the connection.

### Errors

| Situation | Response |
| --- | --- |
| REST, token missing or wrong | HTTP 401, plain-text body `401: Unauthorized` (not JSON) |
| REST, endpoint needs an admin (`/api/template`) and the token's user is not one | HTTP 401 |
| WebSocket, token wrong, revoked or truncated | `{"type":"auth_invalid","message":"Invalid access token or password"}` |
| WebSocket, `auth` message has an `id` | `{"type":"auth_invalid","message":"Auth message incorrectly formatted: not a valid option at 'id'. Got 1"}` |
| WebSocket, command sent before `auth` (for example on a new connection) | `{"type":"auth_invalid","message":"Auth message incorrectly formatted: not a valid option at 'id'. Got 1\nnot a valid value at 'type'. Got '…'"}` |
| Wrong port | Connection refused, no HTTP status |

### Testing with Bruno

The Bruno collection in [`test/bruno`](../../test/bruno) has one folder per
use case of this document. To use it:

1. Create `test/bruno/.env` with the line `HA_TOKEN=<token>`. The file is
   ignored by git.
2. Open the collection in Bruno and select the environment *Home Assistant*.
   Adjust `ha`, `haWs` and the entity variables there if needed.

All HTTP requests inherit bearer authentication from the collection. The
requests in *Actions* control the real device.

Every Bruno WebSocket request opens its own connection. A request that only
sends `{"id":1,"type":"lovelace/dashboards/list"}` therefore always fails, even
if another request authenticated before it. Each WebSocket request in the
collection sends the `auth` message first and the command second. Bruno sends
all messages of a request in order when it connects.

## C examples

The examples use ESP-IDF (`esp_http_client`, `esp_websocket_client`) and cJSON,
which ships with ESP-IDF as the `json` component. They were compiled with
`-Wall -Wextra` against ESP-IDF 4.3 headers. The `esp_websocket_client` API
used here is the same in the ESP-IDF 5.x registry component. Wi-Fi setup is
left out: all calls need a connected station.

Every example uses this configuration:

```c
/* ha_config.h */
#pragma once

/* scheme://host[:port], no trailing slash */
#define HA_URL    "http://homeassistant.local:8123"
/* HA_URL with http -> ws (https -> wss), plus /api/websocket */
#define HA_WS_URL "ws://homeassistant.local:8123/api/websocket"
/* Long-lived access token: the complete JWT on one line */
#define HA_TOKEN  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOi..."
```

Do not commit a real token. Load it from NVS or a file that is not in version
control.

### REST helper

`ha_rest()` sends one request and collects the response body. All REST
examples below use it.

```c
/* ha_rest.h */
#pragma once
#include <stdbool.h>
#include "esp_http_client.h"
#include "cJSON.h"

typedef struct {
    char *data; /* response body, NUL-terminated, malloc'ed: free() it */
    int len;    /* length without the NUL (cover art is binary) */
} ha_body_t;

int ha_rest(esp_http_client_method_t method, const char *path,
            const char *json, bool auth, ha_body_t *out);
int ha_call_service(const char *domain, const char *service, const cJSON *body);
```

```c
/* ha_rest.c */
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "ha_config.h"
#include "ha_rest.h"

static const char *TAG = "ha_rest";

static esp_err_t collect_body(esp_http_client_event_t *e)
{
    ha_body_t *b = e->user_data;
    if (e->event_id != HTTP_EVENT_ON_DATA || b == NULL) {
        return ESP_OK;
    }
    char *p = realloc(b->data, b->len + e->data_len + 1);
    if (p == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(p + b->len, e->data, e->data_len);
    b->data = p;
    b->len += e->data_len;
    b->data[b->len] = '\0';
    return ESP_OK;
}

/* Sends one request to HA and returns the HTTP status, or -1 if HA could not
 * be reached.
 *   path  "/api/..." (may contain a query string)
 *   json  request body, or NULL
 *   auth  false for URLs that carry their own secret (cover art, webhooks)
 *   out   receives the response body, or NULL to discard it.
 *         Free out->data in every case. */
int ha_rest(esp_http_client_method_t method, const char *path,
            const char *json, bool auth, ha_body_t *out)
{
    char url[512];
    if (snprintf(url, sizeof url, "%s%s", HA_URL, path) >= (int)sizeof url) {
        return -1;
    }
    if (out != NULL) {
        *out = (ha_body_t){0};
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = method,
        .timeout_ms = 10000,
        .event_handler = collect_body,
        .user_data = out,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) {
        return -1;
    }
    if (auth) {
        /* Exactly one space after "Bearer", nothing after the token. */
        esp_http_client_set_header(c, "Authorization", "Bearer " HA_TOKEN);
    }
    if (json != NULL) {
        esp_http_client_set_header(c, "Content-Type", "application/json");
        esp_http_client_set_post_field(c, json, strlen(json));
    }

    int status = -1;
    esp_err_t err = esp_http_client_perform(c);
    if (err == ESP_OK) {
        status = esp_http_client_get_status_code(c);
    } else {
        ESP_LOGE(TAG, "%s: %s", path, esp_err_to_name(err));
    }
    esp_http_client_cleanup(c);

    if (status == 401) {
        /* Body is the plain text "401: Unauthorized", not JSON. */
        ESP_LOGE(TAG, "%s: 401, token wrong/revoked or endpoint needs admin", path);
    }
    return status;
}

/* POST /api/services/<domain>/<service> with a flat JSON body.
 * Returns the HTTP status: 200 = done, 400 = unknown service or bad data. */
int ha_call_service(const char *domain, const char *service, const cJSON *body)
{
    char path[160];
    snprintf(path, sizeof path, "/api/services/%s/%s", domain, service);
    char *json = cJSON_PrintUnformatted(body);
    if (json == NULL) {
        return -1;
    }
    int status = ha_rest(HTTP_METHOD_POST, path, json, true, NULL);
    cJSON_free(json);
    return status;
}
```

### WebSocket helper

`ha_ws_connect()` opens a connection and authenticates as described in
[WebSocket](#websocket). `ha_ws_call()` sends one command and waits for its
result.

Two details of `esp_websocket_client` matter here:

- A frame larger than `buffer_size` arrives as several `WEBSOCKET_EVENT_DATA`
  events. `payload_offset` and `payload_len` tell where each piece belongs.
  A dashboard configuration easily exceeds the buffer, so the handler
  reassembles the frame before parsing it. These lengths come from the
  network (over `ws://` anyone in between can change them), so the handler
  caps the frame size and checks every piece against the allocation.
- The event handler runs in the client's own task. Sending from the handler is
  safe, but `esp_websocket_client_close()` and `_destroy()` must not be called
  from it.

```c
/* ha_ws.h */
#pragma once
#include "cJSON.h"

typedef struct ha_ws ha_ws_t;

ha_ws_t *ha_ws_connect(void);
cJSON *ha_ws_call(ha_ws_t *ws, cJSON *cmd);
void ha_ws_close(ha_ws_t *ws);
```

```c
/* ha_ws.c */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "ha_config.h"
#include "ha_ws.h"

static const char *TAG = "ha_ws";

#define EV_AUTH_OK BIT0
#define EV_RESULT  BIT1
#define EV_FAILED  BIT2

/* Largest message accepted. A dashboard configuration is a few KB. */
#define HA_WS_MAX_FRAME (256 * 1024)

struct ha_ws {
    esp_websocket_client_handle_t client;
    EventGroupHandle_t events;
    char *frame;   /* text frame being reassembled */
    int frame_size; /* its payload length (allocation is one more) */
    int next_id;   /* last id sent on this connection */
    int wait_id;   /* id whose result ha_ws_call() waits for */
    cJSON *result; /* that result, NULL if the command failed */
};

static const char *str(const cJSON *obj, const char *key)
{
    const char *s = cJSON_GetStringValue(cJSON_GetObjectItem(obj, key));
    return s != NULL ? s : "";
}

static void send_json(ha_ws_t *ws, const cJSON *msg)
{
    char *text = cJSON_PrintUnformatted(msg);
    if (text != NULL) {
        esp_websocket_client_send_text(ws->client, text, strlen(text), portMAX_DELAY);
        cJSON_free(text);
    }
}

static void on_message(ha_ws_t *ws, cJSON *msg)
{
    const char *type = str(msg, "type");

    if (strcmp(type, "auth_required") == 0) {
        /* HA's first message on every connection. The reply must be the
         * first message we send, and it must not have an "id". */
        cJSON *auth = cJSON_CreateObject();
        cJSON_AddStringToObject(auth, "type", "auth");
        cJSON_AddStringToObject(auth, "access_token", HA_TOKEN);
        send_json(ws, auth);
        cJSON_Delete(auth);
    } else if (strcmp(type, "auth_ok") == 0) {
        xEventGroupSetBits(ws->events, EV_AUTH_OK);
    } else if (strcmp(type, "auth_invalid") == 0) {
        ESP_LOGE(TAG, "auth_invalid: %s", str(msg, "message"));
        xEventGroupSetBits(ws->events, EV_FAILED);
    } else if (strcmp(type, "result") == 0) {
        const cJSON *id = cJSON_GetObjectItem(msg, "id");
        if (!cJSON_IsNumber(id) || id->valueint != ws->wait_id) {
            return;
        }
        if (cJSON_IsTrue(cJSON_GetObjectItem(msg, "success"))) {
            ws->result = cJSON_DetachItemFromObject(msg, "result");
        } else {
            const cJSON *err = cJSON_GetObjectItem(msg, "error");
            ESP_LOGE(TAG, "command %d: %s: %s", ws->wait_id, str(err, "code"), str(err, "message"));
        }
        xEventGroupSetBits(ws->events, EV_RESULT);
    }
}

/* Runs in the WebSocket client's task. */
static void on_ws_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ha_ws_t *ws = arg;
    const esp_websocket_event_data_t *d = event_data;

    if (event_id == WEBSOCKET_EVENT_DISCONNECTED || event_id == WEBSOCKET_EVENT_ERROR) {
        xEventGroupSetBits(ws->events, EV_FAILED);
        return;
    }
    if (event_id != WEBSOCKET_EVENT_DATA || d->op_code != 0x01) {
        return; /* only text frames carry HA messages */
    }

    /* A frame larger than buffer_size arrives as several events. The
     * lengths come from the network: check them before every copy. */
    if (d->payload_offset == 0) {
        free(ws->frame);
        ws->frame = NULL;
        if (d->payload_len < 0 || d->payload_len > HA_WS_MAX_FRAME) {
            xEventGroupSetBits(ws->events, EV_FAILED);
            return;
        }
        ws->frame = malloc((size_t)d->payload_len + 1);
        ws->frame_size = d->payload_len;
    }
    if (ws->frame == NULL || d->payload_len != ws->frame_size || d->payload_offset < 0 ||
        d->data_len < 0 || d->payload_offset > ws->frame_size - d->data_len) {
        free(ws->frame);
        ws->frame = NULL;
        xEventGroupSetBits(ws->events, EV_FAILED);
        return;
    }
    memcpy(ws->frame + d->payload_offset, d->data_ptr, d->data_len);
    if (d->payload_offset + d->data_len < d->payload_len) {
        return; /* more of this frame follows */
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

/* Opens a connection and authenticates. Returns NULL on failure (the log
 * says why: auth_invalid, timeout, or no connection). */
ha_ws_t *ha_ws_connect(void)
{
    ha_ws_t *ws = calloc(1, sizeof *ws);
    if (ws == NULL) {
        return NULL;
    }
    ws->events = xEventGroupCreate();

    esp_websocket_client_config_t cfg = {
        .uri = HA_WS_URL,
        .buffer_size = 2048,
        .task_stack = 8 * 1024, /* cJSON parses in this task */
        .disable_auto_reconnect = true,
    };
    ws->client = esp_websocket_client_init(&cfg);
    if (ws->events == NULL || ws->client == NULL) {
        ha_ws_close(ws);
        return NULL;
    }
    esp_websocket_register_events(ws->client, WEBSOCKET_EVENT_ANY, on_ws_event, ws);
    esp_websocket_client_start(ws->client);

    EventBits_t bits = xEventGroupWaitBits(ws->events, EV_AUTH_OK | EV_FAILED,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    if ((bits & EV_AUTH_OK) == 0) {
        ESP_LOGE(TAG, "not authenticated");
        ha_ws_close(ws);
        return NULL;
    }
    return ws;
}

/* Sends a command and waits for its result. Takes ownership of cmd and adds
 * its "id". Returns the "result" member (free with cJSON_Delete), or NULL. */
cJSON *ha_ws_call(ha_ws_t *ws, cJSON *cmd)
{
    ws->wait_id = ++ws->next_id; /* ids must increase on one connection */
    ws->result = NULL;
    xEventGroupClearBits(ws->events, EV_RESULT);

    cJSON_AddNumberToObject(cmd, "id", ws->wait_id);
    send_json(ws, cmd);
    cJSON_Delete(cmd);

    EventBits_t bits = xEventGroupWaitBits(ws->events, EV_RESULT | EV_FAILED,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    ws->wait_id = 0;
    if ((bits & EV_RESULT) == 0) {
        return NULL;
    }
    cJSON *result = ws->result;
    ws->result = NULL;
    return result;
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
```

## Reading

### Layout (dashboard configuration)

HA has no REST endpoint for dashboards. The configuration is only available
over the WebSocket API at `<ha-ws>`. After [authentication](#websocket):

```text
→ {"id":1,"type":"lovelace/dashboards/list"}
← {"id":1,"type":"result","success":true,"result":[{"url_path":"…","title":"…",…}]}
→ {"id":2,"type":"lovelace/config","url_path":"<dashboard>"}
← {"id":2,"type":"result","success":true,"result":{"views":[…]}}
```

- `lovelace/dashboards/list` is optional. Use it to let the user pick a
  remote instead of configuring `<dashboard>` by hand.
- A failed command returns `"success":false` and an `error` object with `code`
  and `message`.
- The connection can be closed once the configuration has been read. Keep it
  open only if the firmware should follow layout changes or state updates
  (`subscribe_entities`) live.

List the dashboards:

```c
void example_list_dashboards(void)
{
    ha_ws_t *ws = ha_ws_connect();
    if (ws == NULL) {
        return;
    }
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "lovelace/dashboards/list");
    cJSON *list = ha_ws_call(ws, cmd);
    ha_ws_close(ws);

    const cJSON *d;
    cJSON_ArrayForEach(d, list) {
        ESP_LOGI(TAG, "%s: %s",
                 cJSON_GetStringValue(cJSON_GetObjectItem(d, "url_path")),
                 cJSON_GetStringValue(cJSON_GetObjectItem(d, "title")));
    }
    cJSON_Delete(list);
}
```

Read the configuration of one dashboard:

```c
cJSON *example_read_layout(const char *dashboard)
{
    ha_ws_t *ws = ha_ws_connect();
    if (ws == NULL) {
        return NULL;
    }
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "type", "lovelace/config");
    cJSON_AddStringToObject(cmd, "url_path", dashboard);
    cJSON *config = ha_ws_call(ws, cmd); /* {"views":[...]} or NULL */
    ha_ws_close(ws);
    return config;
}
```

Both commands can also run on one connection: call `ha_ws_call()` twice before
`ha_ws_close()`.

### Buttons in the layout

Bubble Card nests buttons deeply. In a `sub-buttons` card they sit under
`sub_button.bottom[].group[]`, inside `views[].sections[].cards[]`. Instead of
following each card type, search the whole configuration for objects that have
a `tap_action`, `hold_action` or `double_tap_action`:

```c
typedef void (*button_fn)(const cJSON *button, void *ctx);

static void find_buttons(const cJSON *node, button_fn fn, void *ctx)
{
    if (cJSON_IsObject(node) && (cJSON_HasObjectItem(node, "tap_action") ||
                                 cJSON_HasObjectItem(node, "hold_action") ||
                                 cJSON_HasObjectItem(node, "double_tap_action"))) {
        fn(node, ctx);
    }
    const cJSON *child;
    cJSON_ArrayForEach(child, node) {
        if ((cJSON_IsObject(child) || cJSON_IsArray(child)) &&
            (child->string == NULL || strstr(child->string, "_action") == NULL)) {
            find_buttons(child, fn, ctx);
        }
    }
}

static void log_button(const cJSON *button, void *ctx)
{
    const cJSON *tap = cJSON_GetObjectItem(button, "tap_action");
    const char *svc = cJSON_GetStringValue(cJSON_GetObjectItem(tap, "perform_action"));
    ESP_LOGI(TAG, "button %s -> %s",
             cJSON_GetStringValue(cJSON_GetObjectItem(button, "name")),
             svc != NULL ? svc : "-");
}

void example_find_buttons(const cJSON *config)
{
    find_buttons(config, log_button, NULL);
}
```

### State

| Purpose | Request | Body |
| --- | --- | --- |
| State of one entity (state, attributes) | `GET <ha>/api/states/<entity_id>` | |
| All states | `GET <ha>/api/states` | |
| Filtered entity list, without WebSocket | `POST <ha>/api/template` | see below |
| Cover art of a media player | `GET <ha><entity_picture>` | |

An unknown entity returns HTTP 404 with `{"message":"Entity not found."}`.

Useful media player attributes: `media_title`, `media_artist`, `app_name`,
`volume_level`, `is_volume_muted`, `entity_picture`, and `supported_features`
(a bit mask that shows which controls the device supports). Most of them are
missing while the device is off.

```c
void example_read_state(void)
{
    ha_body_t body;
    int status = ha_rest(HTTP_METHOD_GET, "/api/states/media_player.living_room_tv",
                         NULL, true, &body);
    if (status == 200) {
        cJSON *s = cJSON_Parse(body.data);
        const cJSON *attr = cJSON_GetObjectItem(s, "attributes");
        const char *state = cJSON_GetStringValue(cJSON_GetObjectItem(s, "state"));
        /* Attributes are missing while the device is off. */
        const cJSON *vol = cJSON_GetObjectItem(attr, "volume_level");
        const char *title = cJSON_GetStringValue(cJSON_GetObjectItem(attr, "media_title"));
        ESP_LOGI(TAG, "state=%s volume=%.2f title=%s", state ? state : "?",
                 cJSON_IsNumber(vol) ? vol->valuedouble : -1.0, title ? title : "");
        cJSON_Delete(s);
    } else if (status == 404) {
        ESP_LOGW(TAG, "no such entity"); /* body: {"message":"Entity not found."} */
    }
    free(body.data);
}
```

### Filtered entity list

The template endpoint evaluates Jinja on the server and returns plain text.
It is the only REST way to get a filtered list, for example all button
entities of one device:

```json
{"template":"{{ states.button | selectattr('entity_id','search','living_room_tv') | map(attribute='entity_id') | list | tojson }}"}
```

End the template with `| tojson`. Without it, HA renders a list as
`['button.a', 'button.b']`, with single quotes, which is not valid JSON.

`/api/template` needs an admin user. With a non-admin token it returns HTTP
401, although the token is valid.

```c
void example_template(void)
{
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "template",
        "{{ states.button | selectattr('entity_id','search','living_room_tv')"
        " | map(attribute='entity_id') | list | tojson }}");
    char *json = cJSON_PrintUnformatted(req); /* escapes the quotes */
    cJSON_Delete(req);

    ha_body_t body;
    int status = ha_rest(HTTP_METHOD_POST, "/api/template", json, true, &body);
    cJSON_free(json);
    if (status == 200) {
        cJSON *list = cJSON_Parse(body.data);
        const cJSON *e;
        cJSON_ArrayForEach(e, list) {
            ESP_LOGI(TAG, "%s", cJSON_GetStringValue(e));
        }
        cJSON_Delete(list);
    }
    free(body.data);
}
```

### Cover art

A media player's `entity_picture` attribute is a path like
`/api/media_player_proxy/<media_player>?token=…&cache=…`. Its access token is
in the query string, so it can be fetched without the `Authorization` header.
The token changes, so read the attribute again before fetching the image. Some
integrations put an external URL there instead of a path, and some leave it
empty even while playing.

```c
/* attributes: the "attributes" object of the media player's state */
void example_cover_art(const cJSON *attributes)
{
    const char *pic = cJSON_GetStringValue(cJSON_GetObjectItem(attributes, "entity_picture"));
    if (pic == NULL || pic[0] != '/') {
        return; /* no picture, or an external URL that ha_rest() can't fetch */
    }
    ha_body_t img;
    if (ha_rest(HTTP_METHOD_GET, pic, NULL, false, &img) == 200) {
        ESP_LOGI(TAG, "cover art: %d bytes", img.len); /* JPEG or PNG */
    }
    free(img.data);
}
```

## Triggering actions

### Mapping dashboard actions to REST calls

Each button has a `tap_action` and, optionally, a `hold_action` (Bubble Card
also has `double_tap_action`). An action of type `perform-action` maps
mechanically to a REST call:

```yaml
tap_action:
  action: perform-action
  perform_action: <domain>.<service>
  target:
    entity_id: <entity_id>
  data:            # optional
    <key>: <value>
```

becomes

```text
POST <ha>/api/services/<domain>/<service>
{"entity_id":"<entity_id>", "<key>":"<value>"}
```

Rules:

- Merge `target` and `data` into one flat JSON body.
- `target` may also hold `device_id` or `area_id`. Pass them through the same
  way.
- `call-service` is the legacy name of `perform-action`. It uses `service`
  instead of `perform_action`, and `service_data` instead of `data`. Handle
  both.
- `toggle` maps to `POST <ha>/api/services/homeassistant/toggle` with the
  card's or button's `entity`.
- `action: none` or a missing action means the gesture does nothing.
- `more-info`, `navigate` and `url` are UI actions of the HA frontend and have
  no REST equivalent. Ignore them or handle them locally.

A successful call returns HTTP 200 with a JSON array of the states that
changed (it may be empty). Any other status is a failure: 400 (plain-text body
`400: Bad Request`) for an unknown service or invalid data, 401 for a bad
token.

Example:

```sh
curl -X POST "<ha>/api/services/button/press" \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"entity_id":"<button>"}'
```

In C, `ha_run_action()` applies these rules to one action object from the
layout:

```c
/* Runs a tap_action, hold_action or double_tap_action. entity is the
 * card's or button's "entity" (used by "toggle"), or NULL.
 * Returns the HTTP status, or 0 if the action has no REST equivalent. */
int ha_run_action(const cJSON *action, const char *entity)
{
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(action, "action"));
    if (type == NULL || strcmp(type, "none") == 0) {
        return 0;
    }

    if (strcmp(type, "toggle") == 0) {
        if (entity == NULL) {
            return 0;
        }
        cJSON *body = cJSON_CreateObject();
        cJSON_AddStringToObject(body, "entity_id", entity);
        int status = ha_call_service("homeassistant", "toggle", body);
        cJSON_Delete(body);
        return status;
    }

    const char *svc;
    const cJSON *data;
    if (strcmp(type, "perform-action") == 0) {
        svc = cJSON_GetStringValue(cJSON_GetObjectItem(action, "perform_action"));
        data = cJSON_GetObjectItem(action, "data");
    } else if (strcmp(type, "call-service") == 0) { /* legacy name */
        svc = cJSON_GetStringValue(cJSON_GetObjectItem(action, "service"));
        data = cJSON_GetObjectItem(action, "service_data");
    } else {
        return 0; /* more-info, navigate, url: frontend only */
    }

    /* "button.press" -> domain "button", service "press" */
    const char *dot = svc != NULL ? strchr(svc, '.') : NULL;
    char domain[64];
    if (dot == NULL || (size_t)(dot - svc) >= sizeof domain) {
        return 0;
    }
    memcpy(domain, svc, dot - svc);
    domain[dot - svc] = '\0';

    /* One flat body: the members of target and data. */
    cJSON *body = cJSON_CreateObject();
    const cJSON *parts[] = {cJSON_GetObjectItem(action, "target"), data};
    for (int i = 0; i < 2; i++) {
        const cJSON *kv;
        cJSON_ArrayForEach(kv, parts[i]) {
            cJSON_AddItemToObject(body, kv->string, cJSON_Duplicate(kv, true));
        }
    }
    int status = ha_call_service(domain, dot + 1, body);
    cJSON_Delete(body);
    return status;
}
```

For a button found by `find_buttons()`, a short press runs
`ha_run_action(cJSON_GetObjectItem(button, "tap_action"), entity)`, and a long
press does the same with `hold_action`.

### Typical remote actions

| Button | Request | Body |
| --- | --- | --- |
| D-pad, OK, back, home, menu, … | `POST <ha>/api/services/button/press` | `{"entity_id":"<button>"}` |
| Volume down / up | `POST <ha>/api/services/media_player/volume_down` / `volume_up` | `{"entity_id":"<media_player>"}` |
| Set volume | `POST <ha>/api/services/media_player/volume_set` | `{"entity_id":"<media_player>","volume_level":0.4}` |
| Mute | `POST <ha>/api/services/media_player/volume_mute` | `{"entity_id":"<media_player>","is_volume_muted":true}` |
| Play / pause | `POST <ha>/api/services/media_player/media_play_pause` | `{"entity_id":"<media_player>"}` |
| Previous / next track | `POST <ha>/api/services/media_player/media_previous_track` / `media_next_track` | `{"entity_id":"<media_player>"}` |
| Power | `POST <ha>/api/services/media_player/toggle` | `{"entity_id":"<media_player>"}` |

Many integrations expose a short and a long press as two button entities
(for example `…_back` and `…_back_long_press`). In the dashboard these appear
as the `tap_action` and the `hold_action` of the same button.

Calling a service directly, without a dashboard action:

```c
void example_remote_actions(void)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "entity_id", "button.living_room_tv_select");
    ha_call_service("button", "press", body);
    cJSON_Delete(body);

    body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "entity_id", "media_player.living_room_tv");
    cJSON_AddNumberToObject(body, "volume_level", 0.4);
    ha_call_service("media_player", "volume_set", body);
    cJSON_Delete(body);
}
```

### Controls that are not in the configuration

A Bubble Card `media-player` card draws its transport and power buttons
itself. They do not appear in the dashboard configuration. When the firmware
finds such a card, it must add the play/pause, previous, next and power
actions from the table above for the card's `entity` on its own. Fields under
`hide:` (for example `volume_button: true`) tell which of them the user hid.

### Calling services over WebSocket (used by the firmware)

The firmware keeps one WebSocket connection open anyway, for
`subscribe_entities` and `lovelace_updated`. It therefore sends button
presses as `call_service` messages on that connection instead of REST calls.
That avoids a new TCP connection per press, and with HTTPS also a TLS
handshake.

The mapping rules above stay the same. Only the message differs: `domain` and
`service` are separate fields, and `target` and `data` are kept apart as
`target` and `service_data`:

```text
→ {"id":7,"type":"call_service","domain":"button","service":"press",
   "service_data":{},"target":{"entity_id":"button.wohnzimmer_apple_tv_wohnzimmer_up"}}
← {"id":7,"type":"result","success":true,"result":{…}}
```

A failed call returns `success:false` with an error code and message, for
example for an unknown service:

```text
← {"id":3,"type":"result","success":false,"error":{"code":"not_found",
   "message":"Service nonexistent.foo not found.",…}}
```

The firmware shows `error.message` on the screen for 3 seconds. While the
connection is down it does not queue presses: it reports "Not connected" at
once.

## Triggering by plain URL (webhooks)

The REST API never triggers an action through `GET`, and it always needs the
`Authorization` header. If a client can only open a URL, create one
automation per button with a webhook trigger:

```yaml
triggers:
  - trigger: webhook
    webhook_id: <random-id>
    allowed_methods: [GET, POST]
    local_only: true
actions:
  - action: button.press
    target:
      entity_id: <button>
```

The button is then triggered by `GET <ha>/api/webhook/<random-id>`, with no
token. HA answers HTTP 200 even for an unknown webhook ID, so a wrong ID is
not reported. Check the automation's trace in HA when testing.

```c
void example_webhook(void)
{
    int status = ha_rest(HTTP_METHOD_GET, "/api/webhook/<random-id>", NULL, false, NULL);
    ESP_LOGI(TAG, "webhook: %d", status); /* 200 even if the id is unknown */
}
```

**Security:** the webhook ID is the only secret. Anyone on the local network
who knows it can trigger the button. Use long random IDs, keep
`local_only: true`, and never use webhooks for locks, alarms or other
security-relevant actions.

## Security notes

- A long-lived token grants the full rights of its user, not only access to
  the remote. Create a dedicated non-admin HA user for the device and issue
  the token for that user. `POST /api/template` is restricted to admin users,
  so with a non-admin token, filter the result of `GET /api/states` on the
  device instead. Check each endpoint with the device's token first.
- The token is stored on the device. Anyone who has the device can read it.
  Revoke it in HA if the device is lost.
- Over plain `http://` and `ws://`, the token travels unencrypted on the local
  network. Use HTTPS/WSS where the HA setup offers it.
