/* Hardware-independent UI logic. See ui_logic.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_logic.h"

bool ui_rect_contains(ui_rect_t r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* ---- geometry ---- */

static int row_height(const button_row_t *row)
{
    int h = 0;
    for (int i = 0; i < row->count; i++) {
        int bh = row->buttons[i].height > 0 ? row->buttons[i].height : UI_ROW_HEIGHT;
        if (bh > h) {
            h = bh;
        }
    }
    return h > 0 ? h : UI_ROW_HEIGHT;
}

static int element_height(const element_t *e)
{
    switch (e->kind) {
    case ELEMENT_HEADING:
        return UI_HEADING_HEIGHT;
    case ELEMENT_MEDIA:
        return UI_MEDIA_HEIGHT + (e->row.count > 0 ? UI_MEDIA_CONTROLS : 0);
    case ELEMENT_ROW:
    default:
        return row_height(&e->row);
    }
}

int ui_measure(const remote_t *remote)
{
    int total = 0;
    for (int i = 0; i < remote->count; i++) {
        total += element_height(&remote->elements[i]);
        if (i > 0) {
            total += UI_ELEMENT_GAP;
        }
    }
    return total;
}

static int scaled(int v, int pct)
{
    return (v * pct + 50) / 100;
}

/* Places the buttons of one row into the rectangle (x, y, w, h). */
static void place_row(const element_t *e, const button_row_t *row, int x, int y, int w, int h,
                      int pct, bool fill, ui_geometry_t *out)
{
    int n = row->count;
    if (n == 0) {
        return;
    }
    int gap = scaled(UI_BUTTON_GAP, pct);
    int widths[LAYOUT_MAX_BUTTONS];
    int total = gap * (n - 1);
    bool all_fixed = !fill;
    for (int i = 0; i < n && i < LAYOUT_MAX_BUTTONS; i++) {
        widths[i] = scaled(row->buttons[i].width, pct);
        if (widths[i] <= 0) {
            all_fixed = false;
        }
        total += widths[i];
    }
    if (!all_fixed || total > w) {
        /* fill: divide the width evenly */
        int each = (w - gap * (n - 1)) / n;
        for (int i = 0; i < n; i++) {
            widths[i] = each;
        }
        total = each * n + gap * (n - 1);
    }
    int bx = x + (w - total) / 2;
    for (int i = 0; i < n && out->button_count < LAYOUT_MAX_BUTTONS; i++) {
        const button_t *b = &row->buttons[i];
        int bh = b->height > 0 ? scaled(b->height, pct) : h;
        ui_placed_button_t *p = &out->buttons[out->button_count++];
        p->button = b;
        p->element = e;
        p->rect = (ui_rect_t){bx, y + (h - bh) / 2, widths[i], bh};
        bx += widths[i] + gap;
    }
}

void ui_place(const remote_t *remote, ui_rect_t area, ui_geometry_t *out)
{
    memset(out, 0, sizeof *out);
    int need = ui_measure(remote);
    int pct = 100;
    if (need > area.h && need > 0) {
        pct = area.h * 100 / need;
        if (pct < UI_MIN_SCALE_PCT) {
            pct = UI_MIN_SCALE_PCT;
        }
    }
    out->scale_pct = pct;

    int y = area.y;
    for (int i = 0; i < remote->count; i++) {
        const element_t *e = &remote->elements[i];
        int h = scaled(element_height(e), pct);
        if (y + h > area.y + area.h || out->element_count == UI_MAX_PLACED) {
            out->cut_off = remote->count - i;
            break;
        }
        ui_placed_element_t *pe = &out->elements[out->element_count++];
        pe->element = e;
        if (e->kind == ELEMENT_MEDIA) {
            int controls = e->row.count > 0 ? scaled(UI_MEDIA_CONTROLS, pct) : 0;
            pe->rect = (ui_rect_t){area.x, y, area.w, h - controls};
            place_row(e, &e->row, area.x, y + h - controls, area.w, controls, pct, true, out);
        } else {
            pe->rect = (ui_rect_t){area.x, y, area.w, h};
            if (e->kind == ELEMENT_ROW) {
                place_row(e, &e->row, area.x, y, area.w, h, pct, e->row.justify == JUSTIFY_FILL,
                          out);
            }
        }
        y += h + scaled(UI_ELEMENT_GAP, pct);
    }
}

const ui_placed_button_t *ui_hit_button(const ui_geometry_t *g, int x, int y)
{
    for (int i = 0; i < g->button_count; i++) {
        if (ui_rect_contains(g->buttons[i].rect, x, y)) {
            return &g->buttons[i];
        }
    }
    return NULL;
}

/* ---- gestures ---- */

ui_gesture_t ui_touch_update(ui_touch_t *t, bool pressed, int x, int y, uint32_t now_ms, int *gx,
                             int *gy)
{
    if (pressed && !t->down) {
        *t = (ui_touch_t){.down = true, .x0 = x, .y0 = y, .x = x, .y = y, .t0 = now_ms};
        return GESTURE_NONE;
    }
    if (!t->down) {
        return GESTURE_NONE;
    }
    if (pressed) {
        t->x = x;
        t->y = y;
    }
    int dx = t->x - t->x0, dy = t->y - t->y0;
    int adx = abs(dx), ady = abs(dy);
    bool still = adx < UI_TAP_SLOP_PX && ady < UI_TAP_SLOP_PX;
    *gx = t->x0;
    *gy = t->y0;

    if (pressed) {
        if (!t->hold_fired && still && now_ms - t->t0 >= UI_HOLD_MS) {
            t->hold_fired = true;
            return GESTURE_HOLD;
        }
        return GESTURE_NONE;
    }

    /* released */
    t->down = false;
    if (t->hold_fired) {
        return GESTURE_NONE;
    }
    if (adx >= UI_SWIPE_MIN_PX && adx > 2 * ady) {
        return dx < 0 ? GESTURE_SWIPE_LEFT : GESTURE_SWIPE_RIGHT;
    }
    if (still && now_ms - t->t0 < UI_HOLD_MS) {
        return GESTURE_TAP;
    }
    return GESTURE_NONE;
}

/* ---- status values ---- */

int ui_battery_percent(int millivolts)
{
    int pct = (millivolts - 3300) * 100 / (4150 - 3300);
    return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

const char *ui_battery_icon(int percent)
{
    if (percent < 10) {
        return "battery-alert-variant-outline";
    }
    if (percent < 30) {
        return "battery-20";
    }
    if (percent < 50) {
        return "battery-40";
    }
    if (percent < 70) {
        return "battery-60";
    }
    if (percent < 90) {
        return "battery-80";
    }
    return "battery";
}

int ui_wifi_level(int rssi)
{
    if (rssi == 0) {
        return 0;
    }
    if (rssi >= -55) {
        return 4;
    }
    if (rssi >= -67) {
        return 3;
    }
    if (rssi >= -78) {
        return 2;
    }
    return 1;
}

bool ui_time_valid(const struct tm *t)
{
    return t != NULL && t->tm_year + 1900 >= 2024;
}

void ui_format_time(const struct tm *t, char *buf, size_t size)
{
    if (!ui_time_valid(t)) {
        snprintf(buf, size, "--.--.---- --:--");
        return;
    }
    snprintf(buf, size, "%02d.%02d.%04d %02d:%02d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
             t->tm_hour, t->tm_min);
}

/* ---- media and paging ---- */

void ui_state_line(const char *title, const char *app, const char *state, char *buf, size_t size)
{
    bool has_title = title != NULL && title[0] != '\0';
    bool has_app = app != NULL && app[0] != '\0';
    if (has_title && has_app) {
        snprintf(buf, size, "%s \xc2\xb7 %s", title, app); /* U+00B7 middle dot */
    } else if (has_title || has_app) {
        snprintf(buf, size, "%s", has_title ? title : app);
    } else {
        snprintf(buf, size, "%s", state != NULL ? state : "");
    }
}

int ui_page_step(int current, int count, int delta)
{
    if (count <= 0) {
        return 0;
    }
    return ((current + delta) % count + count) % count;
}

bool ui_image_size(const uint8_t *d, size_t len, int *w, int *h)
{
    /* PNG: signature, then the IHDR chunk with width and height */
    static const uint8_t png[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (len >= 24 && memcmp(d, png, 8) == 0 && memcmp(d + 12, "IHDR", 4) == 0) {
        *w = (d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19];
        *h = (d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23];
        return *w > 0 && *h > 0;
    }
    /* JPEG: walk the segments to a start-of-frame marker */
    if (len >= 4 && d[0] == 0xFF && d[1] == 0xD8) {
        size_t p = 2;
        while (p + 9 < len) {
            if (d[p] != 0xFF) {
                return false;
            }
            uint8_t marker = d[p + 1];
            if (marker == 0xFF) {
                p++; /* fill byte */
                continue;
            }
            size_t seg = (d[p + 2] << 8) | d[p + 3];
            bool sof = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 &&
                       marker != 0xCC;
            if (sof) {
                *h = (d[p + 5] << 8) | d[p + 6];
                *w = (d[p + 7] << 8) | d[p + 8];
                return *w > 0 && *h > 0;
            }
            if (seg < 2) {
                return false;
            }
            p += 2 + seg;
        }
    }
    return false;
}
