#pragma once

/*
 * Wi-Fi and the Home Assistant connection. Specified in
 * openspec/changes/add-remote-ui/specs/ha-connection/spec.md.
 *
 * Everything that happens is reported through one callback (ha_event_fn).
 * The callback runs in a Wi-Fi, WebSocket or HA task: it must only copy the
 * event into a queue.
 */

#include <stdbool.h>
#include <stdint.h>

#include "device_config.h"
#include "layout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HA_EV_WIFI_UP,           /* got an IP address */
    HA_EV_WIFI_DOWN,         /* lost the network, reconnecting */
    HA_EV_CONNECTED,         /* WebSocket authenticated */
    HA_EV_DISCONNECTED,      /* WebSocket lost, reconnecting */
    HA_EV_TOKEN_REJECTED,    /* auth_invalid: stopped, needs a new config */
    HA_EV_TIMEZONE,          /* tz holds the IANA name */
    HA_EV_LAYOUT,            /* layout: new remotes, receiver frees */
    HA_EV_DASHBOARD_MISSING, /* lovelace/config failed */
    HA_EV_MEDIA,             /* media: new state of a media player, receiver frees */
    HA_EV_CALL_RESULT,       /* result of ha_call() */
} ha_event_kind_t;

typedef struct {
    char entity_id[96];
    char state[24];      /* playing, paused, off, ... */
    char title[128];     /* media_title, may be empty */
    char app[64];        /* app_name, may be empty */
    char picture[384];   /* entity_picture path, may be empty */
} ha_media_t;

typedef struct {
    ha_event_kind_t kind;
    union {
        char tz[64];
        layout_t *layout;
        ha_media_t *media;
        struct {
            uint32_t request;
            bool ok;
            char message[96];
        } call;
    };
} ha_event_t;

typedef void (*ha_event_fn)(const ha_event_t *event);

/* Connects to Wi-Fi and then to Home Assistant, and keeps both connections
 * up. cfg must stay valid. Requires esp_netif_init() and the default event
 * loop. */
void ha_start(const config_t *cfg, ha_event_fn on_event);

/* Calls a service. Returns false at once if Home Assistant is not
 * connected; otherwise the result arrives later as HA_EV_CALL_RESULT with the
 * same request number. */
bool ha_call(uint32_t request, const call_t *call);

/* Current Wi-Fi signal strength in dBm, or 0 if not connected. */
int ha_wifi_rssi(void);

/* Frees an event's payload (layout or media), if it has one. */
void ha_event_free(ha_event_t *event);

#ifdef __cplusplus
}
#endif
