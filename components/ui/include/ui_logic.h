#pragma once

/*
 * The parts of the UI that don't touch the hardware: page geometry, gesture
 * recognition, status values and text formatting. Pure C, tested on the
 * host (test/host/test_ui_logic.c).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- screen regions (portrait, 540 x 960) ---- */

#define UI_WIDTH        540
#define UI_HEIGHT       960
#define UI_BAR_HEIGHT   48
#define UI_TITLE_TOP    UI_BAR_HEIGHT
#define UI_TITLE_HEIGHT 56
#define UI_CONTENT_TOP  (UI_TITLE_TOP + UI_TITLE_HEIGHT + 8)
#define UI_MARGIN       16
#define UI_TOAST_HEIGHT 56
#define UI_CONTENT_BOTTOM (UI_HEIGHT - 8)

typedef struct {
    int x, y, w, h;
} ui_rect_t;

bool ui_rect_contains(ui_rect_t r, int x, int y);

/* ---- geometry (measure and place) ---- */

#define UI_HEADING_HEIGHT   40
#define UI_MEDIA_HEIGHT     160
#define UI_MEDIA_CONTROLS   64
#define UI_ROW_HEIGHT       56 /* default button height */
#define UI_ELEMENT_GAP      8
#define UI_BUTTON_GAP       12
#define UI_MIN_SCALE_PCT    60
#define UI_MAX_PLACED       48 /* elements per page */

typedef struct {
    const element_t *element;
    ui_rect_t rect; /* whole element; for MEDIA without the control row */
} ui_placed_element_t;

typedef struct {
    const button_t *button;
    const element_t *element;
    ui_rect_t rect;
} ui_placed_button_t;

typedef struct {
    int scale_pct; /* 60..100 */
    int element_count;
    ui_placed_element_t elements[UI_MAX_PLACED];
    int button_count;
    ui_placed_button_t buttons[LAYOUT_MAX_BUTTONS];
    int cut_off; /* elements that did not fit, even at the minimum scale */
} ui_geometry_t;

/* Natural height of a remote's content in pixels at scale 100. */
int ui_measure(const remote_t *remote);

/* Places a remote's elements and buttons into area. */
void ui_place(const remote_t *remote, ui_rect_t area, ui_geometry_t *out);

/* Button under (x, y), or NULL. */
const ui_placed_button_t *ui_hit_button(const ui_geometry_t *g, int x, int y);

/* ---- gestures ---- */

#define UI_HOLD_MS        500
#define UI_TAP_SLOP_PX    20
#define UI_SWIPE_MIN_PX   100

typedef enum {
    GESTURE_NONE,
    GESTURE_TAP,
    GESTURE_HOLD,        /* fired while still touching */
    GESTURE_SWIPE_LEFT,  /* finger moved left: next page */
    GESTURE_SWIPE_RIGHT, /* finger moved right: previous page */
} ui_gesture_t;

typedef struct {
    bool down;
    bool hold_fired;
    int x0, y0, x, y;
    uint32_t t0;
} ui_touch_t;

/* Feeds one touch sample (pressed or not, position, time in ms). Returns a
 * gesture when one is recognized. *gx, *gy receive the start point. */
ui_gesture_t ui_touch_update(ui_touch_t *t, bool pressed, int x, int y, uint32_t now_ms, int *gx,
                             int *gy);

/* ---- status values ---- */

/* 0..100 from the battery voltage, linear between 3300 and 4150 mV. */
int ui_battery_percent(int millivolts);
/* MDI icon name for a battery level. */
const char *ui_battery_icon(int percent);
/* 1..4 from RSSI; 0 if not connected (rssi == 0). */
int ui_wifi_level(int rssi);

/* "DD.MM.YYYY HH:MM", or "--.--.---- --:--" if the time is not valid. */
void ui_format_time(const struct tm *t, char *buf, size_t size);
/* True if t is a plausible current time (after 2024). */
bool ui_time_valid(const struct tm *t);

/* ---- media and paging ---- */

/* "title · app", or whichever is present, or the state. */
void ui_state_line(const char *title, const char *app, const char *state, char *buf,
                   size_t size);

/* Next page index with wrap-around; delta is +1 or -1. */
int ui_page_step(int current, int count, int delta);

/* URL to fetch cover art from, or false if picture is not on Home
 * Assistant. Accepts a path ("/api/...") and absolute URLs that start with
 * ha_url followed by "/". Refuses other hosts, "//host", "\\" and "@". */
bool ui_cover_url(const char *ha_url, const char *picture, char *url, size_t size);

/* Width and height of a JPEG or PNG image, from its header. */
bool ui_image_size(const uint8_t *data, size_t len, int *w, int *h);

#ifdef __cplusplus
}
#endif
