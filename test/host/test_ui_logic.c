/* Host unit tests for ui_logic.c. Run from the project root. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_logic.h"

static int failures;
static int checks;

#define CHECK(cond, ...)                                \
    do {                                                \
        checks++;                                       \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static const ui_rect_t CONTENT = {UI_MARGIN, UI_CONTENT_TOP, UI_WIDTH - 2 * UI_MARGIN,
                                  UI_CONTENT_BOTTOM - UI_CONTENT_TOP};

static layout_t load_fixture(void)
{
    layout_t l = {0};
    FILE *f = fopen("test/host/fixtures/media-control.json", "rb");
    CHECK(f != NULL, "fixture not found");
    if (f == NULL) {
        return l;
    }
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    cJSON *cfg = cJSON_Parse(buf);
    layout_parse(cfg, &l, NULL);
    cJSON_Delete(cfg);
    return l;
}

static bool overlap(ui_rect_t a, ui_rect_t b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static bool inside(ui_rect_t inner, ui_rect_t outer)
{
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

/* 8.2: the real dashboard fits at scale 1 */
static void test_fixture_geometry(void)
{
    layout_t l = load_fixture();
    if (l.count != 1) {
        CHECK(false, "fixture has one remote");
        return;
    }
    const remote_t *r = &l.remotes[0];
    int need = ui_measure(r);
    /* heading 40 + media 160+64 + rows 64,80,64,52,52 + 6 gaps of 8 */
    CHECK(need == 40 + 224 + 312 + 48, "natural height %d", need);

    static ui_geometry_t g;
    ui_place(r, CONTENT, &g);
    CHECK(g.scale_pct == 100, "scale 100%%, got %d", g.scale_pct);
    CHECK(g.cut_off == 0, "nothing cut off");
    CHECK(g.element_count == 7, "7 elements placed");
    CHECK(g.button_count == 13, "13 buttons placed, got %d", g.button_count);

    for (int i = 0; i < g.button_count; i++) {
        CHECK(inside(g.buttons[i].rect, CONTENT), "button %d inside content", i);
        for (int k = i + 1; k < g.button_count; k++) {
            CHECK(!overlap(g.buttons[i].rect, g.buttons[k].rect), "buttons %d and %d overlap", i, k);
        }
    }

    /* D-pad: centered rows keep their widths, OK is 80 wide and centered */
    const ui_placed_button_t *up = NULL, *ok = NULL, *back = NULL, *tv = NULL;
    for (int i = 0; i < g.button_count; i++) {
        const char *n = g.buttons[i].button->name;
        if (n && strcmp(n, "Hoch") == 0) up = &g.buttons[i];
        if (n && strcmp(n, "OK") == 0) ok = &g.buttons[i];
        if (n && strcmp(n, "Zur\xc3\xbc" "ck") == 0) back = &g.buttons[i];
        if (n && strcmp(n, "TV") == 0) tv = &g.buttons[i];
    }
    CHECK(up && up->rect.w == 64 && up->rect.h == 64, "Hoch 64x64");
    CHECK(ok && ok->rect.w == 80 && ok->rect.h == 80, "OK 80x80");
    CHECK(up && ok && up->rect.x + up->rect.w / 2 == ok->rect.x + ok->rect.w / 2,
          "Hoch centered above OK");
    CHECK(back && tv && back->rect.w == tv->rect.w && back->rect.w > 200, "fill row halves: %d",
          back ? back->rect.w : -1);
    CHECK(back && back->rect.h == 52, "fill row height 52");

    /* hit testing */
    CHECK(ok && ui_hit_button(&g, ok->rect.x + 5, ok->rect.y + 5) == ok, "hit OK");
    CHECK(ui_hit_button(&g, 0, 0) == NULL, "no button at 0,0");
    layout_free(&l);
}

static void test_scaling(void)
{
    /* 20 rows of 56 px = 1120 + 19*8 = 1272 > 840: scaled down, to the minimum */
    static remote_t r;
    static element_t els[20];
    static button_t btn[20];
    memset(&r, 0, sizeof r);
    for (int i = 0; i < 20; i++) {
        els[i].kind = ELEMENT_ROW;
        els[i].row.count = 1;
        els[i].row.buttons = &btn[i];
        els[i].row.justify = JUSTIFY_FILL;
    }
    r.count = 12; /* 12*56 + 11*8 = 760: fits */
    r.elements = els;
    static ui_geometry_t g;
    ui_place(&r, CONTENT, &g);
    CHECK(g.scale_pct == 100 && g.cut_off == 0, "12 rows fit unscaled");

    r.count = 15; /* 15*56 + 14*8 = 952 -> 840*100/952 = 88% */
    ui_place(&r, CONTENT, &g);
    CHECK(g.scale_pct == 88 && g.cut_off == 0, "15 rows scaled to %d%%, cut %d", g.scale_pct,
          g.cut_off);
    for (int i = 0; i < g.button_count; i++) {
        CHECK(inside(g.buttons[i].rect, CONTENT), "scaled button %d inside", i);
    }

    r.count = 20; /* needs 63%: fits at 63 */
    ui_place(&r, CONTENT, &g);
    CHECK(g.scale_pct >= UI_MIN_SCALE_PCT && g.cut_off == 0, "20 rows: %d%% cut %d", g.scale_pct,
          g.cut_off);

    /* far too much: clamped to 60% and cut off */
    static element_t many[40];
    static button_t mb[40];
    for (int i = 0; i < 40; i++) {
        many[i].kind = ELEMENT_ROW;
        many[i].row.count = 1;
        many[i].row.buttons = &mb[i];
    }
    r.elements = many;
    r.count = 40;
    ui_place(&r, CONTENT, &g);
    CHECK(g.scale_pct == UI_MIN_SCALE_PCT && g.cut_off > 0, "40 rows: 60%% and cut off (%d)",
          g.cut_off);
}

static void test_center_row_too_wide(void)
{
    static button_t b[4];
    for (int i = 0; i < 4; i++) {
        b[i] = (button_t){.width = 200};
    }
    static element_t e = {.kind = ELEMENT_ROW, .row = {.justify = JUSTIFY_CENTER, .count = 4,
                                                       .buttons = b}};
    static remote_t r = {.count = 1, .elements = &e};
    static ui_geometry_t g;
    ui_place(&r, CONTENT, &g);
    CHECK(g.button_count == 4, "4 buttons");
    for (int i = 0; i < 4; i++) {
        CHECK(inside(g.buttons[i].rect, CONTENT), "too-wide center row falls back to fill");
    }
}

static void test_gestures(void)
{
    ui_touch_t t = {0};
    int gx, gy;
    CHECK(ui_touch_update(&t, true, 100, 100, 1000, &gx, &gy) == GESTURE_NONE, "press");
    CHECK(ui_touch_update(&t, true, 105, 102, 1100, &gx, &gy) == GESTURE_NONE, "small move");
    CHECK(ui_touch_update(&t, false, 0, 0, 1200, &gx, &gy) == GESTURE_TAP && gx == 100 && gy == 100,
          "tap at start point");

    ui_touch_update(&t, true, 50, 50, 0, &gx, &gy);
    CHECK(ui_touch_update(&t, true, 50, 50, 499, &gx, &gy) == GESTURE_NONE, "not yet hold");
    CHECK(ui_touch_update(&t, true, 50, 50, 500, &gx, &gy) == GESTURE_HOLD, "hold at 500 ms");
    CHECK(ui_touch_update(&t, true, 50, 50, 900, &gx, &gy) == GESTURE_NONE, "hold fires once");
    CHECK(ui_touch_update(&t, false, 0, 0, 1000, &gx, &gy) == GESTURE_NONE, "no tap after hold");

    ui_touch_update(&t, true, 400, 500, 0, &gx, &gy);
    ui_touch_update(&t, true, 250, 520, 150, &gx, &gy);
    CHECK(ui_touch_update(&t, false, 0, 0, 200, &gx, &gy) == GESTURE_SWIPE_LEFT, "swipe left");

    ui_touch_update(&t, true, 100, 500, 0, &gx, &gy);
    ui_touch_update(&t, true, 260, 480, 150, &gx, &gy);
    CHECK(ui_touch_update(&t, false, 0, 0, 200, &gx, &gy) == GESTURE_SWIPE_RIGHT, "swipe right");

    ui_touch_update(&t, true, 100, 500, 0, &gx, &gy);
    ui_touch_update(&t, true, 190, 500, 150, &gx, &gy);
    CHECK(ui_touch_update(&t, false, 0, 0, 200, &gx, &gy) == GESTURE_NONE, "90 px is no swipe");

    ui_touch_update(&t, true, 100, 500, 0, &gx, &gy);
    ui_touch_update(&t, true, 220, 600, 150, &gx, &gy);
    CHECK(ui_touch_update(&t, false, 0, 0, 200, &gx, &gy) == GESTURE_NONE,
          "diagonal (dx < 2*dy) is no swipe");

    ui_touch_update(&t, true, 100, 500, 0, &gx, &gy);
    ui_touch_update(&t, true, 130, 500, 100, &gx, &gy);
    CHECK(ui_touch_update(&t, true, 130, 500, 600, &gx, &gy) == GESTURE_NONE,
          "moved finger does not hold");
    CHECK(ui_touch_update(&t, false, 0, 0, 700, &gx, &gy) == GESTURE_NONE, "nor tap");
}

static void test_status_values(void)
{
    CHECK(ui_battery_percent(4150) == 100 && ui_battery_percent(4300) == 100, "full");
    CHECK(ui_battery_percent(3300) == 0 && ui_battery_percent(3000) == 0, "empty");
    CHECK(ui_battery_percent(3725) == 50, "half: %d", ui_battery_percent(3725));
    CHECK(strcmp(ui_battery_icon(9), "battery-alert-variant-outline") == 0, "alert below 10");
    CHECK(strcmp(ui_battery_icon(10), "battery-20") == 0, "10 -> 20");
    CHECK(strcmp(ui_battery_icon(55), "battery-60") == 0, "55 -> 60");
    CHECK(strcmp(ui_battery_icon(100), "battery") == 0, "100 -> full");

    CHECK(ui_wifi_level(0) == 0, "disconnected");
    CHECK(ui_wifi_level(-40) == 4 && ui_wifi_level(-55) == 4, "level 4");
    CHECK(ui_wifi_level(-56) == 3 && ui_wifi_level(-67) == 3, "level 3");
    CHECK(ui_wifi_level(-68) == 2 && ui_wifi_level(-78) == 2, "level 2");
    CHECK(ui_wifi_level(-79) == 1 && ui_wifi_level(-95) == 1, "level 1");

    struct tm t = {.tm_year = 2026 - 1900, .tm_mon = 10, .tm_mday = 22, .tm_hour = 20, .tm_min = 15};
    char buf[32];
    ui_format_time(&t, buf, sizeof buf);
    CHECK(strcmp(buf, "22.11.2026 20:15") == 0, "date format: %s", buf);
    struct tm old = {.tm_year = 70, .tm_mday = 1};
    ui_format_time(&old, buf, sizeof buf);
    CHECK(strcmp(buf, "--.--.---- --:--") == 0, "invalid time placeholder");
    ui_format_time(NULL, buf, sizeof buf);
    CHECK(strcmp(buf, "--.--.---- --:--") == 0, "NULL time");
}

static void test_media_and_paging(void)
{
    char buf[64];
    ui_state_line("Tatort", "ARD", "playing", buf, sizeof buf);
    CHECK(strcmp(buf, "Tatort \xc2\xb7 ARD") == 0, "title and app: %s", buf);
    ui_state_line("", "waipu", "playing", buf, sizeof buf);
    CHECK(strcmp(buf, "waipu") == 0, "app only");
    ui_state_line("Tatort", NULL, "playing", buf, sizeof buf);
    CHECK(strcmp(buf, "Tatort") == 0, "title only");
    ui_state_line(NULL, "", "off", buf, sizeof buf);
    CHECK(strcmp(buf, "off") == 0, "state fallback");

    CHECK(ui_page_step(0, 3, 1) == 1 && ui_page_step(2, 3, 1) == 0, "next wraps");
    CHECK(ui_page_step(0, 3, -1) == 2, "previous wraps");
    CHECK(ui_page_step(0, 1, 1) == 0 && ui_page_step(0, 0, 1) == 0, "single and empty");
}

static void test_image_size(void)
{
    static const uint8_t png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13,
                                  'I', 'H', 'D', 'R', 0, 0, 0x01, 0x2C, 0, 0, 0, 0xC8};
    int w = 0, h = 0;
    CHECK(ui_image_size(png, sizeof png, &w, &h) && w == 300 && h == 200, "PNG %dx%d", w, h);
    /* JPEG: SOI, APP0 (len 16), SOF0 with 8-bit, height 360, width 640 */
    uint8_t jpg[64] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    memset(jpg + 6, 0, 14);
    uint8_t sof[] = {0xFF, 0xC0, 0x00, 0x11, 0x08, 0x01, 0x68, 0x02, 0x80, 0x03};
    memcpy(jpg + 2 + 2 + 16, sof, sizeof sof);
    CHECK(ui_image_size(jpg, sizeof jpg, &w, &h) && w == 640 && h == 360, "JPEG %dx%d", w, h);
    CHECK(!ui_image_size((const uint8_t *)"GIF89a", 6, &w, &h), "other formats rejected");
    CHECK(!ui_image_size(jpg, 3, &w, &h), "truncated JPEG rejected");
}

static void test_cover_url(void)
{
    const char *ha = "http://homeassistant.local:8123";
    char url[256];
    CHECK(ui_cover_url(ha, "/api/media_player_proxy/media_player.tv?token=abc&cache=1", url,
                       sizeof url) &&
              strcmp(url, "http://homeassistant.local:8123/api/media_player_proxy/media_player.tv"
                          "?token=abc&cache=1") == 0,
          "relative path: %s", url);
    CHECK(ui_cover_url(ha, "http://homeassistant.local:8123/api/image/x", url, sizeof url) &&
              strcmp(url, "http://homeassistant.local:8123/api/image/x") == 0,
          "absolute URL on HA");
    const char *refused[] = {
        "https://example.com/cover.jpg",
        "http://homeassistant.local:8123.evil.com/x",
        "http://homeassistant.local:81234/x",
        "//evil.com/x",
        "http://homeassistant.local:8123//evil.com/x",
        "/api/x@evil.com",
        "/api\\..\\x",
        "api/no-leading-slash",
        "",
        "http://homeassistant.local:8123",
    };
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        CHECK(!ui_cover_url(ha, refused[i], url, sizeof url), "refused: %s", refused[i]);
    }
    char small[16];
    CHECK(!ui_cover_url(ha, "/api/x", small, sizeof small), "too long for buffer");
    CHECK(!ui_cover_url(NULL, "/x", url, sizeof url) && !ui_cover_url(ha, NULL, url, sizeof url),
          "NULL inputs");
}

static void test_many_buttons(void)
{
    static button_t b[LAYOUT_MAX_BUTTONS];
    memset(b, 0, sizeof b);
    static element_t e;
    e = (element_t){.kind = ELEMENT_ROW,
                    .row = {.justify = JUSTIFY_FILL, .count = LAYOUT_MAX_BUTTONS, .buttons = b}};
    static remote_t r;
    r = (remote_t){.count = 1, .elements = &e};
    static ui_geometry_t g;
    ui_place(&r, CONTENT, &g);
    CHECK(g.button_count == LAYOUT_MAX_BUTTONS, "all buttons placed");
    for (int i = 0; i < g.button_count; i++) {
        CHECK(g.buttons[i].rect.w >= 1, "button %d has positive width (%d)", i, g.buttons[i].rect.w);
        CHECK(inside(g.buttons[i].rect, CONTENT), "button %d inside", i);
    }
}

int main(void)
{
    test_cover_url();
    test_many_buttons();
    test_fixture_geometry();
    test_scaling();
    test_center_row_too_wide();
    test_gestures();
    test_status_values();
    test_media_and_paging();
    test_image_size();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
