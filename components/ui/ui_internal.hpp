#pragma once

/* FreeRTOS.h must come before any other FreeRTOS or M5GFX header. */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <M5Unified.h>

#include "assets.h"
#include "device_config.h"
#include "ha.h"
#include "ui_logic.h"

/* ---- messages to the UI task ---- */

enum class UiMsgKind : uint8_t {
    Ha,          /* ha holds an event from the HA connection */
    TimeSynced,  /* SNTP set the system clock */
    Cover,       /* cover holds downloaded cover art */
};

struct UiCover {
    char entity[96];
    uint8_t *data; /* malloc'ed image (JPEG or PNG), receiver frees */
    size_t len;
};

struct UiMsg {
    UiMsgKind kind;
    ha_event_t ha;
    UiCover cover;
};

void ui_post(const UiMsg &msg);

/* ---- state, owned by the UI task ---- */

enum class Status : uint8_t {
    WifiConnecting,
    HaConnecting,
    Connected,
    TokenRejected,
    DashboardMissing,
};

constexpr int UI_MAX_MEDIA = 16;

struct MediaView {
    ha_media_t state;
    uint8_t *cover; /* image data or nullptr */
    size_t cover_len;
};

struct UiState {
    const config_t *cfg = nullptr;
    Status status = Status::WifiConnecting;
    bool wifi_up = false;
    bool ha_up = false;

    layout_t *layout = nullptr;
    int page = 0;
    int saved_page = -1; /* from NVS, applied to the first layout */
    ui_geometry_t geo = {};

    MediaView media[UI_MAX_MEDIA] = {};
    int media_count = 0;
    bool media_dirty = false;
    uint32_t media_drawn_ms = 0;

    const ui_placed_button_t *pressed = nullptr;
    uint32_t pressed_request = 0;
    uint32_t next_request = 1;

    bool toast = false;
    uint32_t toast_until_ms = 0;
    char toast_text[96] = {};

    bool dialog = false;
    uint32_t dialog_until_ms = 0;

    int partial_count = 0;
    uint32_t last_full_ms = 0;

    ui_touch_t touch = {};

    int battery_pct = -1;
    int wifi_level = -1;
    int shown_minute = -1;
};

extern UiState ui;

/* ---- rendering (ui_render.cpp) ---- */

void render_init();
/* Draws the whole screen and refreshes it with full quality. */
void render_full();
/* Redraws rect from the current state and refreshes only that region. */
void render_region(ui_rect_t rect, epd_mode_t mode);
/* Draws one button pressed (inverted) or normal and refreshes it. */
void render_button(const ui_placed_button_t *b, bool pressed);
void render_toast(const char *text);
void render_media_tiles();

ui_rect_t rect_status_bar();
ui_rect_t rect_title_row();
ui_rect_t rect_content();
ui_rect_t rect_toast();
ui_rect_t rect_gear();
ui_rect_t rect_arrow_left();
ui_rect_t rect_arrow_right();
ui_rect_t rect_dialog_cancel();
ui_rect_t rect_dialog_restart();

/* The remote on screen, or nullptr while a status screen is shown. */
const remote_t *current_remote();
MediaView *media_for(const char *entity_id);

/* Draw helpers shared with the status bar. */
void draw_icon(const char *name, int size, int x, int y, bool inverted);
void use_font(int px);

/* ---- status bar (ui_status.cpp) ---- */

void status_init();
/* Called every loop: updates time, battery and Wi-Fi when due. */
void status_tick(uint32_t now_ms);
void status_draw();          /* draws the bar into the frame buffer */
void status_refresh();       /* draws and refreshes only the bar */
void status_set_time_zone(const char *iana);
void status_on_wifi_up();    /* starts SNTP once */
void status_on_time_synced();

/* ---- cover art (ui_cover.cpp) ---- */

void cover_start(const config_t *cfg);
void cover_request(const char *entity_id, const char *picture);
