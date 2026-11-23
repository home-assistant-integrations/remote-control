/*
 * The UI task: owns the display, touch, buttons and the UI state. Other
 * tasks only post messages (ui_post).
 */

#include "ui_internal.hpp"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs.h"
#include "ui.h"

static const char *TAG = "ui";

UiState ui;

namespace {

QueueHandle_t queue;

constexpr uint32_t TOAST_MS = 3000;
constexpr uint32_t DIALOG_MS = 15000;
constexpr uint32_t MEDIA_MIN_INTERVAL_MS = 5000;
constexpr int GHOST_PARTIALS = 20;
constexpr uint32_t GHOST_MS = 10 * 60 * 1000;

const char *NVS_NS = "rc";

uint32_t now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ---- NVS ---- */

void nvs_put_i32(const char *key, int32_t v)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, key, v);
        nvs_commit(h);
        nvs_close(h);
    }
}

int32_t nvs_get_i32_or(const char *key, int32_t def)
{
    nvs_handle_t h;
    int32_t v = def;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, key, &v);
        nvs_close(h);
    }
    return v;
}

/* ---- HA events from other tasks ---- */

void on_ha_event(const ha_event_t *event)
{
    UiMsg msg = {};
    msg.kind = UiMsgKind::Ha;
    msg.ha = *event;
    ui_post(msg);
}

/* ---- state changes ---- */

void place_current()
{
    const remote_t *r = current_remote();
    if (r == nullptr) {
        memset(&ui.geo, 0, sizeof ui.geo);
        return;
    }
    ui_place(r, rect_content(), &ui.geo);
    if (ui.geo.cut_off > 0) {
        ESP_LOGW(TAG, "%s: %d elements do not fit and are not shown", r->title, ui.geo.cut_off);
    }
}

void set_page(int page)
{
    if (ui.layout == nullptr || page == ui.page) {
        return;
    }
    ui.page = page;
    ui.pressed = nullptr;
    nvs_put_i32("page", page);
    place_current();
    render_full();
}

void show_toast(const char *text)
{
    snprintf(ui.toast_text, sizeof ui.toast_text, "%s", text);
    ui.toast = true;
    ui.toast_until_ms = now_ms() + TOAST_MS;
    render_toast(ui.toast_text);
}

void take_layout(layout_t *layout)
{
    if (ui.layout != nullptr) {
        layout_free(ui.layout);
        free(ui.layout);
    }
    ui.layout = layout;
    ui.pressed = nullptr;
    if (ui.saved_page >= 0) {
        ui.page = ui.saved_page < layout->count ? ui.saved_page : 0;
        ui.saved_page = -1;
    } else if (ui.page >= layout->count) {
        ui.page = 0; /* the page is gone: back to the first one */
    }

    /* Keep known media states; add the players of the new layout. Static:
     * 16 MediaView are about 11 KB, more than the UI task's stack. */
    static MediaView kept[UI_MAX_MEDIA];
    int kept_count = ui.media_count;
    memcpy(kept, ui.media, sizeof kept);
    ui.media_count = 0;
    for (int r = 0; r < layout->count; r++) {
        for (int e = 0; e < layout->remotes[r].count; e++) {
            const element_t *el = &layout->remotes[r].elements[e];
            if (el->kind != ELEMENT_MEDIA || el->entity == nullptr ||
                media_for(el->entity) != nullptr || ui.media_count == UI_MAX_MEDIA) {
                continue;
            }
            MediaView *m = &ui.media[ui.media_count++];
            memset(m, 0, sizeof *m);
            for (int k = 0; k < kept_count; k++) {
                if (strcmp(kept[k].state.entity_id, el->entity) == 0) {
                    *m = kept[k];
                    kept[k].cover = nullptr; /* moved */
                }
            }
            snprintf(m->state.entity_id, sizeof m->state.entity_id, "%s", el->entity);
        }
    }
    for (int k = 0; k < kept_count; k++) {
        free(kept[k].cover);
    }

    ui.status = Status::Connected;
    place_current();
    render_full();
}

void take_media(const ha_media_t *state)
{
    MediaView *m = media_for(state->entity_id);
    if (m == nullptr) {
        return;
    }
    bool new_picture = strcmp(m->state.picture, state->picture) != 0;
    m->state = *state;
    if (new_picture) {
        free(m->cover);
        m->cover = nullptr;
        m->cover_len = 0;
        if (state->picture[0] != '\0') {
            cover_request(state->entity_id, state->picture);
        }
    }
    ui.media_dirty = true;
}

void handle_ha(ha_event_t *ev)
{
    switch (ev->kind) {
    case HA_EV_WIFI_UP:
        ui.wifi_up = true;
        ui.wifi_level = ui_wifi_level(ha_wifi_rssi());
        status_on_wifi_up();
        if (ui.status == Status::WifiConnecting) {
            ui.status = Status::HaConnecting;
            render_full();
        } else {
            status_refresh();
        }
        break;
    case HA_EV_WIFI_DOWN:
        ui.wifi_up = false;
        ui.ha_up = false;
        if (ui.status == Status::HaConnecting) {
            /* no remote shown yet: show what we are waiting for */
            ui.status = Status::WifiConnecting;
            render_full();
        } else {
            status_refresh(); /* a remote stays on screen */
        }
        break;
    case HA_EV_CONNECTED:
        ui.ha_up = true;
        status_refresh();
        break;
    case HA_EV_DISCONNECTED:
        ui.ha_up = false;
        if (ui.pressed != nullptr) {
            render_button(ui.pressed, false);
            ui.pressed = nullptr;
        }
        status_refresh();
        break;
    case HA_EV_TOKEN_REJECTED:
        ui.ha_up = false;
        ui.status = Status::TokenRejected;
        render_full();
        break;
    case HA_EV_TIMEZONE:
        status_set_time_zone(ev->tz);
        status_refresh();
        break;
    case HA_EV_LAYOUT:
        take_layout(ev->layout);
        ev->layout = nullptr; /* now owned by the UI */
        break;
    case HA_EV_DASHBOARD_MISSING:
        ui.status = Status::DashboardMissing;
        render_full();
        break;
    case HA_EV_MEDIA:
        take_media(ev->media);
        break;
    case HA_EV_CALL_RESULT:
        if (ui.pressed != nullptr && ev->call.request == ui.pressed_request) {
            render_button(ui.pressed, false);
            ui.pressed = nullptr;
        }
        if (!ev->call.ok) {
            show_toast(ev->call.message[0] ? ev->call.message : "The action failed");
        }
        break;
    }
    ha_event_free(ev);
}

void handle_msg(UiMsg &msg)
{
    switch (msg.kind) {
    case UiMsgKind::Ha:
        handle_ha(&msg.ha);
        break;
    case UiMsgKind::TimeSynced:
        status_on_time_synced();
        status_refresh();
        break;
    case UiMsgKind::Cover: {
        MediaView *m = media_for(msg.cover.entity);
        if (m == nullptr) {
            free(msg.cover.data);
            break;
        }
        free(m->cover);
        m->cover = msg.cover.data;
        m->cover_len = msg.cover.len;
        ui.media_dirty = true;
        break;
    }
    }
}

/* ---- input ---- */

void open_dialog()
{
    ui.dialog = true;
    ui.dialog_until_ms = now_ms() + DIALOG_MS;
    render_full();
}

void close_dialog()
{
    ui.dialog = false;
    render_full();
}

void request_setup()
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "setup_req", 1);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "restarting into setup mode");
    esp_restart();
}

void press(const ui_placed_button_t *b, bool hold)
{
    const call_t *call = hold ? &b->button->hold : &b->button->tap;
    if (call->domain == nullptr) {
        return; /* this gesture does nothing for this button */
    }
    if (ui.pressed != nullptr) {
        return; /* one call at a time */
    }
    uint32_t request = ui.next_request++;
    if (!ha_call(request, call)) {
        show_toast("Not connected");
        return;
    }
    ui.pressed = b;
    ui.pressed_request = request;
    render_button(b, true);
}

void on_tap_or_hold(int x, int y, bool hold)
{
    if (ui.dialog) {
        if (ui_rect_contains(rect_dialog_cancel(), x, y)) {
            close_dialog();
        } else if (ui_rect_contains(rect_dialog_restart(), x, y)) {
            request_setup();
        }
        return;
    }
    if (ui_rect_contains(rect_gear(), x, y)) {
        open_dialog();
        return;
    }
    if (ui.layout != nullptr && ui.layout->count > 1 && current_remote() != nullptr) {
        if (ui_rect_contains(rect_arrow_left(), x, y)) {
            set_page(ui_page_step(ui.page, ui.layout->count, -1));
            return;
        }
        if (ui_rect_contains(rect_arrow_right(), x, y)) {
            set_page(ui_page_step(ui.page, ui.layout->count, +1));
            return;
        }
    }
    if (current_remote() == nullptr) {
        return;
    }
    const ui_placed_button_t *b = ui_hit_button(&ui.geo, x, y);
    if (b != nullptr) {
        press(b, hold);
    }
}

void page_by(int delta)
{
    if (ui.dialog || ui.layout == nullptr || ui.layout->count < 2 || current_remote() == nullptr) {
        return;
    }
    set_page(ui_page_step(ui.page, ui.layout->count, delta));
}

void poll_input(uint32_t now)
{
    M5.update();

    if (M5.BtnA.wasPressed()) {
        page_by(-1); /* GPIO 37 */
    }
    if (M5.BtnC.wasPressed()) {
        page_by(+1); /* GPIO 39 */
    }

    bool pressed = M5.Touch.getCount() > 0;
    int x = 0, y = 0;
    if (pressed) {
        auto t = M5.Touch.getDetail();
        x = t.x;
        y = t.y;
    }
    int gx = 0, gy = 0;
    switch (ui_touch_update(&ui.touch, pressed, x, y, now, &gx, &gy)) {
    case GESTURE_TAP:
        on_tap_or_hold(gx, gy, false);
        break;
    case GESTURE_HOLD:
        on_tap_or_hold(gx, gy, true);
        break;
    case GESTURE_SWIPE_LEFT:
        if (gy >= UI_CONTENT_TOP) {
            page_by(+1);
        }
        break;
    case GESTURE_SWIPE_RIGHT:
        if (gy >= UI_CONTENT_TOP) {
            page_by(-1);
        }
        break;
    case GESTURE_NONE:
        break;
    }
}

/* ---- timers ---- */

void tick(uint32_t now)
{
    status_tick(now);

    if (ui.toast && (int32_t)(now - ui.toast_until_ms) >= 0) {
        ui.toast = false;
        render_region(rect_toast(), epd_mode_t::epd_fast);
    }
    if (ui.dialog && (int32_t)(now - ui.dialog_until_ms) >= 0) {
        close_dialog();
    }
    if (ui.media_dirty && now - ui.media_drawn_ms >= MEDIA_MIN_INTERVAL_MS && !ui.dialog) {
        ui.media_dirty = false;
        ui.media_drawn_ms = now;
        render_media_tiles();
    }
    /* Against ghosting: a full refresh after many partial ones, or after a
     * while, but never while the screen is touched or a call is running. */
    bool due = ui.partial_count >= GHOST_PARTIALS ||
               (ui.partial_count > 0 && now - ui.last_full_ms >= GHOST_MS);
    if (due && !ui.touch.down && ui.pressed == nullptr) {
        render_full();
    }
}

void ui_task(void *)
{
    render_init();
    status_init();
    ui.saved_page = nvs_get_i32_or("page", -1);
    render_full();

    for (;;) {
        UiMsg msg;
        if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(20))) {
            handle_msg(msg);
            while (xQueueReceive(queue, &msg, 0)) {
                handle_msg(msg);
            }
        }
        uint32_t now = now_ms();
        poll_input(now);
        tick(now);
    }
}

}  // namespace

void ui_post(const UiMsg &msg)
{
    if (xQueueSend(queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        /* queue full: drop it, but never leak its payload */
        UiMsg copy = msg;
        if (copy.kind == UiMsgKind::Ha) {
            ha_event_free(&copy.ha);
        } else if (copy.kind == UiMsgKind::Cover) {
            free(copy.cover.data);
        }
        ESP_LOGW(TAG, "UI queue full, message dropped");
    }
}

extern "C" void ui_start(const config_t *cfg)
{
    ui.cfg = cfg;
    queue = xQueueCreate(16, sizeof(UiMsg));
    cover_start(cfg);
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, nullptr, 4, nullptr, 1);
    ha_start(cfg, on_ha_event);
}

extern "C" bool ui_take_setup_request(void)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    if (nvs_get_u8(h, "setup_req", &v) == ESP_OK) {
        nvs_erase_key(h, "setup_req");
        nvs_commit(h);
    }
    nvs_close(h);
    return v == 1;
}
