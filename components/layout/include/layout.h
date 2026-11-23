#pragma once

/*
 * Turns a Home Assistant dashboard configuration (`lovelace/config`) into
 * remotes, elements and buttons. Pure C (cJSON and libc), tested on the host.
 * Specified in openspec/changes/add-remote-ui/specs/remote-layout/spec.md.
 */

#include <stdbool.h>

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAYOUT_MAX_REMOTES 16
#define LAYOUT_MAX_BUTTONS 64 /* per remote, including media controls */

/* A ready-to-send service call. domain == NULL means "no call". The JSON
 * strings are never NULL when domain is set ("{}" if empty). */
typedef struct {
    char *domain;
    char *service;
    char *service_data_json;
    char *target_json;
} call_t;

typedef enum {
    BUTTON_PLAIN,
    BUTTON_PLAY_PAUSE, /* media control: icon depends on the player state */
} button_role_t;

typedef struct {
    char *name;      /* may be NULL */
    char *icon;      /* "mdi:..." or NULL */
    bool show_icon;
    bool show_name;
    int width;       /* CSS pixels, 0 = automatic */
    int height;      /* CSS pixels, 0 = automatic */
    button_role_t role;
    call_t tap;
    call_t hold;
} button_t;

typedef enum {
    JUSTIFY_CENTER, /* keep the given widths, centered */
    JUSTIFY_FILL,   /* divide the row width evenly */
} justify_t;

typedef struct {
    justify_t justify;
    int count;
    button_t *buttons;
} button_row_t;

typedef enum {
    ELEMENT_HEADING,
    ELEMENT_MEDIA,
    ELEMENT_ROW,
} element_kind_t;

typedef struct {
    element_kind_t kind;
    char *name;   /* heading text, media tile name */
    char *icon;   /* heading icon, media tile icon */
    char *entity; /* media tile: the media_player entity */
    button_row_t row; /* ROW: the buttons; MEDIA: the control row */
} element_t;

typedef struct {
    char *title;
    int count;
    element_t *elements;
    int button_count;
} remote_t;

typedef struct {
    int count;
    remote_t remotes[LAYOUT_MAX_REMOTES];
} layout_t;

/* Receives log messages about skipped cards and limits. May be NULL. */
typedef void (*layout_log_fn)(const char *message);

/*
 * Parses the "result" of lovelace/config ({"views":[...]}). Returns false
 * only on out-of-memory or if config is not an object; unsupported content
 * is skipped and reported through log. Free with layout_free().
 */
bool layout_parse(const cJSON *config, layout_t *out, layout_log_fn log);
void layout_free(layout_t *layout);

/*
 * Maps one dashboard action (tap_action, hold_action) to a service call,
 * following docs/dev/home-assistant-api.md. entity is the card's or button's
 * "entity" (used by "toggle"), or NULL. Returns false if the action has no
 * service call (none, missing, UI-only); out is then all NULL.
 */
bool action_to_call(const cJSON *action, const char *entity, call_t *out);
void call_free(call_t *call);

#ifdef __cplusplus
}
#endif
