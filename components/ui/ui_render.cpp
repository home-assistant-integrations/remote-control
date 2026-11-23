/*
 * Drawing: title row, remotes, media tiles, status screens, the setup dialog
 * and the toast. Only the UI task calls these functions.
 */

#include "ui_internal.hpp"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"

namespace {

lgfx::PointerWrapper data20, data28, data36;
lgfx::VLWfont font20, font28, font36;

constexpr int RADIUS = 10;

uint32_t now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* Draws text so that it fits into max_w, cutting it with "…" if needed. */
void draw_fitted(const char *text, int x, int y, int max_w, textdatum_t datum)
{
    auto &d = M5.Display;
    if (text == nullptr || text[0] == '\0' || max_w <= 0) {
        return;
    }
    d.setTextDatum(datum);
    if (d.textWidth(text) <= max_w) {
        d.drawString(text, x, y);
        return;
    }
    char buf[192] = "";
    size_t n = strlen(text);
    if (n >= sizeof buf - 4) {
        n = sizeof buf - 4;
    }
    while (n > 0) {
        /* step back over UTF-8 continuation bytes */
        do {
            n--;
        } while (n > 0 && (text[n] & 0xC0) == 0x80);
        memcpy(buf, text, n);
        strcpy(buf + n, "\xe2\x80\xa6"); /* … */
        if (d.textWidth(buf) <= max_w) {
            break;
        }
    }
    d.drawString(buf, x, y);
}

/* Draws text word-wrapped and centered into r. */
void draw_wrapped(const char *text, ui_rect_t r)
{
    auto &d = M5.Display;
    d.setTextDatum(textdatum_t::top_center);
    int line_h = d.fontHeight() + 6;
    char lines[8][256];
    int count = 0;
    char line[256] = "";
    const char *p = text;
    while (*p != '\0' && count < 8) {
        const char *end = p;
        while (*end != '\0' && *end != ' ' && *end != '\n') {
            end++;
        }
        char word[96];
        size_t wl = (size_t)(end - p) < sizeof word - 1 ? (size_t)(end - p) : sizeof word - 1;
        memcpy(word, p, wl);
        word[wl] = '\0';
        char candidate[256];
        strlcpy(candidate, line, sizeof candidate);
        if (line[0] != '\0') {
            strlcat(candidate, " ", sizeof candidate);
        }
        strlcat(candidate, word, sizeof candidate);
        if (line[0] != '\0' && d.textWidth(candidate) > r.w) {
            strlcpy(lines[count++], line, sizeof lines[0]);
            strlcpy(line, word, sizeof line);
        } else {
            strlcpy(line, candidate, sizeof line);
        }
        if (*end == '\n' && count < 8) {
            strlcpy(lines[count++], line, sizeof lines[0]);
            line[0] = '\0';
        }
        p = *end != '\0' ? end + 1 : end;
    }
    if (line[0] != '\0' && count < 8) {
        strlcpy(lines[count++], line, sizeof lines[0]);
    }
    int y = r.y + (r.h - count * line_h) / 2;
    for (int i = 0; i < count; i++) {
        draw_fitted(lines[i], r.x + r.w / 2, y + i * line_h, r.w, textdatum_t::top_center);
    }
}

void frame(ui_rect_t r, bool filled)
{
    auto &d = M5.Display;
    if (filled) {
        d.fillRoundRect(r.x, r.y, r.w, r.h, RADIUS, TFT_BLACK);
    } else {
        d.fillRoundRect(r.x, r.y, r.w, r.h, RADIUS, TFT_WHITE);
        d.drawRoundRect(r.x, r.y, r.w, r.h, RADIUS, TFT_BLACK);
        d.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, RADIUS - 1, TFT_BLACK);
    }
}

/* ---- buttons ---- */

const char *button_icon(const ui_placed_button_t *pb)
{
    const button_t *b = pb->button;
    if (b->role == BUTTON_PLAY_PAUSE) {
        MediaView *m = pb->element != nullptr ? media_for(pb->element->entity) : nullptr;
        bool playing = m != nullptr && strcmp(m->state.state, "playing") == 0;
        return playing ? "mdi:pause" : "mdi:play";
    }
    return b->icon;
}

void draw_button(const ui_placed_button_t *pb, bool pressed)
{
    auto &d = M5.Display;
    const button_t *b = pb->button;
    ui_rect_t r = pb->rect;
    frame(r, pressed);
    uint32_t fg = pressed ? TFT_WHITE : TFT_BLACK;
    uint32_t bg = pressed ? TFT_BLACK : TFT_WHITE;
    d.setTextColor(fg, bg);

    const char *icon = button_icon(pb);
    bool want_icon = b->show_icon && icon != nullptr;
    bool has_icon = want_icon && icon_index(icon) >= 0;
    bool want_name = b->show_name && b->name != nullptr && b->name[0] != '\0';
    int size = r.h >= 72 && r.w >= 72 ? 48 : 32;
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;

    if (has_icon) {
        use_font(20);
        bool stacked = want_name && r.h >= size + d.fontHeight() + 12;
        bool beside = want_name && !stacked && r.w >= size + 24 + d.textWidth(b->name);
        if (stacked) {
            int top = cy - (size + 4 + d.fontHeight()) / 2;
            draw_icon(icon, size, cx - size / 2, top, pressed);
            draw_fitted(b->name, cx, top + size + 4, r.w - 8, textdatum_t::top_center);
        } else if (beside) {
            int total = size + 8 + d.textWidth(b->name);
            int left = cx - total / 2;
            draw_icon(icon, size, left, cy - size / 2, pressed);
            draw_fitted(b->name, left + size + 8, cy, r.w - size - 16, textdatum_t::middle_left);
        } else {
            draw_icon(icon, size, cx - size / 2, cy - size / 2, pressed);
        }
        return;
    }
    /* No usable icon: the name, or the icon name without "mdi:". */
    const char *text = want_name                        ? b->name
                       : (b->name && b->name[0])        ? b->name
                       : (icon && strncmp(icon, "mdi:", 4) == 0) ? icon + 4
                       : icon                           ? icon
                                                        : "?";
    use_font(r.h >= 64 ? 28 : 20);
    d.setTextColor(fg, bg);
    draw_fitted(text, cx, cy, r.w - 12, textdatum_t::middle_center);
}

/* ---- elements ---- */

void draw_heading(const element_t *e, ui_rect_t r)
{
    auto &d = M5.Display;
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    use_font(28);
    int x = r.x;
    if (e->icon != nullptr && icon_index(e->icon) >= 0) {
        draw_icon(e->icon, 32, x, r.y + (r.h - 32) / 2, false);
        x += 40;
    }
    draw_fitted(e->name ? e->name : "", x, r.y + r.h / 2, r.x + r.w - x, textdatum_t::middle_left);
}

void draw_cover(MediaView *m, const element_t *e, ui_rect_t box)
{
    auto &d = M5.Display;
    d.drawRect(box.x, box.y, box.w, box.h, TFT_BLACK);
    int w = 0, h = 0;
    if (m != nullptr && m->cover != nullptr && ui_image_size(m->cover, m->cover_len, &w, &h)) {
        float s = std::min((float)(box.w - 2) / w, (float)(box.h - 2) / h);
        int dw = (int)(w * s), dh = (int)(h * s);
        int x = box.x + 1 + (box.w - 2 - dw) / 2, y = box.y + 1 + (box.h - 2 - dh) / 2;
        bool ok = m->cover[0] == 0x89
                      ? d.drawPng(m->cover, m->cover_len, x, y, dw, dh, 0, 0, s, s)
                      : d.drawJpg(m->cover, m->cover_len, x, y, dw, dh, 0, 0, s, s);
        if (ok) {
            return;
        }
    }
    const char *icon = e->icon != nullptr && icon_index(e->icon) >= 0 ? e->icon : "mdi:speaker";
    draw_icon(icon, 48, box.x + (box.w - 48) / 2, box.y + (box.h - 48) / 2, false);
}

void draw_media(const element_t *e, ui_rect_t r)
{
    auto &d = M5.Display;
    frame(r, false);
    MediaView *m = media_for(e->entity);
    int pad = 12;
    ui_rect_t cover = {r.x + pad, r.y + pad, r.h - 2 * pad, r.h - 2 * pad};
    draw_cover(m, e, cover);

    int tx = cover.x + cover.w + pad;
    int tw = r.x + r.w - pad - tx;
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    use_font(28);
    draw_fitted(e->name ? e->name : e->entity, tx, r.y + pad + 4, tw, textdatum_t::top_left);

    char line[256];
    if (m != nullptr && m->state.state[0] != '\0') {
        ui_state_line(m->state.title, m->state.app, m->state.state, line, sizeof line);
    } else {
        snprintf(line, sizeof line, "\xe2\x80\xa6"); /* not known yet */
    }
    use_font(20);
    draw_fitted(line, tx, r.y + pad + 48, tw, textdatum_t::top_left);
}

/* ---- regions ---- */

void draw_title_row()
{
    auto &d = M5.Display;
    ui_rect_t r = rect_title_row();
    d.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
    const remote_t *rem = current_remote();
    if (rem == nullptr) {
        return;
    }
    int pages = ui.layout->count;
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    use_font(28);
    int text_w = pages > 1 ? r.w - 2 * 64 : r.w - 2 * UI_MARGIN;
    draw_fitted(rem->title, r.x + r.w / 2, r.y + (pages > 1 ? 4 : 12), text_w,
                textdatum_t::top_center);
    if (pages > 1) {
        ui_rect_t lft = rect_arrow_left(), rgt = rect_arrow_right();
        draw_icon("mdi:chevron-left", 48, lft.x + (lft.w - 48) / 2, lft.y + (lft.h - 48) / 2, false);
        draw_icon("mdi:chevron-right", 48, rgt.x + (rgt.w - 48) / 2, rgt.y + (rgt.h - 48) / 2,
                  false);
        int spacing = 18;
        int x0 = r.x + r.w / 2 - (pages - 1) * spacing / 2;
        for (int i = 0; i < pages; i++) {
            int cx = x0 + i * spacing, cy = r.y + r.h - 10;
            if (i == ui.page) {
                d.fillCircle(cx, cy, 5, TFT_BLACK);
            } else {
                d.drawCircle(cx, cy, 5, TFT_BLACK);
            }
        }
    }
}

void draw_status_screen()
{
    char msg[256];
    switch (ui.status) {
    case Status::WifiConnecting:
        snprintf(msg, sizeof msg, "Connecting to %s \xe2\x80\xa6", ui.cfg->wifi_ssid);
        break;
    case Status::HaConnecting:
        snprintf(msg, sizeof msg, "Connecting to Home Assistant \xe2\x80\xa6\n%s", ui.cfg->ha_url);
        break;
    case Status::TokenRejected:
        snprintf(msg, sizeof msg,
                 "Home Assistant rejected the token. Tap the setup button to upload a new "
                 "configuration.");
        break;
    case Status::DashboardMissing:
        snprintf(msg, sizeof msg, "Dashboard %s not found.", ui.cfg->ha_dashboard);
        break;
    case Status::Connected:
    default:
        snprintf(msg, sizeof msg, "The dashboard has no remotes.");
        break;
    }
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    use_font(28);
    ui_rect_t r = rect_content();
    draw_wrapped(msg, {r.x + 16, r.y, r.w - 32, r.h - 120});
}

void draw_content()
{
    auto &d = M5.Display;
    ui_rect_t c = {0, UI_CONTENT_TOP - 8, UI_WIDTH, UI_HEIGHT - (UI_CONTENT_TOP - 8)};
    d.fillRect(c.x, c.y, c.w, c.h, TFT_WHITE);
    const remote_t *rem = current_remote();
    if (rem == nullptr) {
        draw_status_screen();
        return;
    }
    for (int i = 0; i < ui.geo.element_count; i++) {
        const ui_placed_element_t *pe = &ui.geo.elements[i];
        if (pe->element->kind == ELEMENT_HEADING) {
            draw_heading(pe->element, pe->rect);
        } else if (pe->element->kind == ELEMENT_MEDIA) {
            draw_media(pe->element, pe->rect);
        }
    }
    for (int i = 0; i < ui.geo.button_count; i++) {
        draw_button(&ui.geo.buttons[i], &ui.geo.buttons[i] == ui.pressed);
    }
}

void draw_dialog()
{
    auto &d = M5.Display;
    ui_rect_t box = {50, 300, UI_WIDTH - 100, 300};
    d.fillRoundRect(box.x - 4, box.y - 4, box.w + 8, box.h + 8, RADIUS + 2, TFT_WHITE);
    frame(box, false);
    d.drawRoundRect(box.x + 2, box.y + 2, box.w - 4, box.h - 4, RADIUS - 2, TFT_BLACK);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    use_font(28);
    draw_wrapped("Restart into setup mode to upload a new configuration?",
                 {box.x + 24, box.y + 20, box.w - 48, box.h - 130});
    ui_rect_t cancel = rect_dialog_cancel(), restart = rect_dialog_restart();
    frame(cancel, false);
    frame(restart, true);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    draw_fitted("Cancel", cancel.x + cancel.w / 2, cancel.y + cancel.h / 2, cancel.w - 8,
                textdatum_t::middle_center);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    draw_fitted("Restart", restart.x + restart.w / 2, restart.y + restart.h / 2, restart.w - 8,
                textdatum_t::middle_center);
}

void draw_toast_band(const char *text)
{
    auto &d = M5.Display;
    ui_rect_t r = rect_toast();
    d.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    use_font(20);
    draw_fitted(text, r.x + r.w / 2, r.y + r.h / 2, r.w - 24, textdatum_t::middle_center);
}

/* Draws everything for the current state into the frame buffer. */
void draw_all()
{
    status_draw();
    draw_title_row();
    draw_content();
    if (ui.dialog) {
        draw_dialog();
    }
    if (ui.toast) {
        draw_toast_band(ui.toast_text);
    }
}

void refresh(ui_rect_t r, epd_mode_t mode)
{
    auto &d = M5.Display;
    d.setEpdMode(mode);
    d.display(r.x, r.y, r.w, r.h);
    ui.partial_count++;
}

}  // namespace

/* ---- shared helpers ---- */

void use_font(int px)
{
    M5.Display.setFont(px >= 36 ? &font36 : px >= 28 ? &font28 : &font20);
}

void draw_icon(const char *name, int size, int x, int y, bool inverted)
{
    const uint8_t *img = icon_find(name, size);
    if (img == nullptr) {
        return;
    }
    M5.Display.pushGrayscaleImage(x, y, size, size, img, lgfx::color_depth_t::grayscale_4bit,
                                  inverted ? TFT_WHITE : TFT_BLACK,
                                  inverted ? TFT_BLACK : TFT_WHITE);
}

ui_rect_t rect_status_bar() { return {0, 0, UI_WIDTH, UI_BAR_HEIGHT}; }
ui_rect_t rect_title_row() { return {0, UI_TITLE_TOP, UI_WIDTH, UI_TITLE_HEIGHT}; }
ui_rect_t rect_content()
{
    return {UI_MARGIN, UI_CONTENT_TOP, UI_WIDTH - 2 * UI_MARGIN, UI_CONTENT_BOTTOM - UI_CONTENT_TOP};
}
ui_rect_t rect_toast() { return {0, UI_HEIGHT - UI_TOAST_HEIGHT, UI_WIDTH, UI_TOAST_HEIGHT}; }
ui_rect_t rect_gear() { return {UI_WIDTH - 52, 0, 52, UI_BAR_HEIGHT}; }
ui_rect_t rect_arrow_left() { return {0, UI_TITLE_TOP, 64, UI_TITLE_HEIGHT}; }
ui_rect_t rect_arrow_right() { return {UI_WIDTH - 64, UI_TITLE_TOP, 64, UI_TITLE_HEIGHT}; }
ui_rect_t rect_dialog_cancel() { return {74, 500, 180, 72}; }
ui_rect_t rect_dialog_restart() { return {UI_WIDTH - 74 - 180, 500, 180, 72}; }

const remote_t *current_remote()
{
    if (ui.layout == nullptr || ui.layout->count == 0 || ui.status == Status::TokenRejected ||
        ui.status == Status::DashboardMissing) {
        return nullptr;
    }
    const remote_t *r = &ui.layout->remotes[ui.page];
    return r->count > 0 ? r : nullptr;
}

MediaView *media_for(const char *entity_id)
{
    for (int i = 0; entity_id != nullptr && i < ui.media_count; i++) {
        if (strcmp(ui.media[i].state.entity_id, entity_id) == 0) {
            return &ui.media[i];
        }
    }
    return nullptr;
}

void render_init()
{
    auto &d = M5.Display;
    d.setAutoDisplay(false); /* every refresh is explicit */
    data20.set(font_20());
    data28.set(font_28());
    data36.set(font_36());
    font20.loadFont(&data20);
    font28.loadFont(&data28);
    font36.loadFont(&data36);
}

void render_full()
{
    auto &d = M5.Display;
    d.startWrite();
    d.fillScreen(TFT_WHITE);
    draw_all();
    d.endWrite();
    d.setEpdMode(epd_mode_t::epd_quality);
    d.display();
    ui.partial_count = 0;
    ui.last_full_ms = now_ms();
}

void render_region(ui_rect_t r, epd_mode_t mode)
{
    auto &d = M5.Display;
    d.startWrite();
    d.setClipRect(r.x, r.y, r.w, r.h);
    draw_all();
    d.clearClipRect();
    d.endWrite();
    refresh(r, mode);
}

void render_button(const ui_placed_button_t *b, bool pressed)
{
    auto &d = M5.Display;
    d.startWrite();
    d.setClipRect(b->rect.x, b->rect.y, b->rect.w, b->rect.h);
    draw_button(b, pressed);
    d.clearClipRect();
    d.endWrite();
    refresh(b->rect, pressed ? epd_mode_t::epd_fastest : epd_mode_t::epd_fast);
}

void render_toast(const char *text)
{
    auto &d = M5.Display;
    d.startWrite();
    draw_toast_band(text);
    d.endWrite();
    refresh(rect_toast(), epd_mode_t::epd_fast);
}

void render_media_tiles()
{
    for (int i = 0; i < ui.geo.element_count; i++) {
        const ui_placed_element_t *pe = &ui.geo.elements[i];
        if (pe->element->kind != ELEMENT_MEDIA) {
            continue;
        }
        /* the tile and its control row (play/pause icon depends on the state) */
        ui_rect_t r = pe->rect;
        r.h += UI_MEDIA_CONTROLS;
        render_region(r, epd_mode_t::epd_fast);
    }
}
