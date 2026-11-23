/*
 * Host unit tests for layout_parse() and action_to_call(). Run with
 * `make test-host` from the project root.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "layout.h"

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

#define STREQ(a, b) ((a) != NULL && (b) != NULL && strcmp((a), (b)) == 0)

/* ---- log capture ---- */

static char logbuf[4096];

static void capture(const char *msg)
{
    strncat(logbuf, msg, sizeof logbuf - strlen(logbuf) - 2);
    strcat(logbuf, "\n");
}

static bool parse_text(const char *json, layout_t *out)
{
    logbuf[0] = '\0';
    cJSON *root = cJSON_Parse(json);
    CHECK(root != NULL, "test JSON does not parse: %.60s", json);
    bool ok = layout_parse(root, out, capture);
    cJSON_Delete(root);
    return ok;
}

static cJSON *load_fixture(void)
{
    FILE *f = fopen("test/host/fixtures/media-control.json", "rb");
    if (f == NULL) {
        return NULL;
    }
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    return cJSON_Parse(buf);
}

static bool target_is(const call_t *c, const char *entity)
{
    char expected[256];
    snprintf(expected, sizeof expected, "{\"entity_id\":\"%s\"}", entity);
    return STREQ(c->target_json, expected);
}

/* ---- 3.3 views to remotes ---- */

static void test_views(void)
{
    layout_t l;
    CHECK(parse_text("{\"views\":[{\"title\":\"Wohnzimmer\",\"cards\":[]}]}", &l), "parse");
    CHECK(l.count == 1 && STREQ(l.remotes[0].title, "Wohnzimmer"), "one view");
    layout_free(&l);

    CHECK(parse_text("{\"views\":[{\"title\":\"A\"},{\"path\":\"b\"},{},{\"title\":\"\","
                     "\"path\":\"\"}]}",
                     &l),
          "parse");
    CHECK(l.count == 4, "four views, got %d", l.count);
    CHECK(STREQ(l.remotes[0].title, "A"), "title");
    CHECK(STREQ(l.remotes[1].title, "b"), "path fallback");
    CHECK(STREQ(l.remotes[2].title, "Remote 3"), "number fallback: %s", l.remotes[2].title);
    CHECK(STREQ(l.remotes[3].title, "Remote 4"), "empty title and path");
    layout_free(&l);

    CHECK(parse_text("{\"views\":[]}", &l) && l.count == 0, "no views");
    layout_free(&l);
    CHECK(parse_text("{}", &l) && l.count == 0, "no views key");
    layout_free(&l);
    cJSON *arr = cJSON_Parse("[]");
    CHECK(!layout_parse(arr, &l, NULL), "non-object config rejected");
    cJSON_Delete(arr);
}

/* ---- 3.4 card discovery ---- */

static void test_card_discovery(void)
{
    layout_t l;
    parse_text("{\"views\":[{\"title\":\"M\",\"cards\":["
               "{\"type\":\"custom:bubble-card\",\"card_type\":\"separator\",\"name\":\"One\"},"
               "{\"type\":\"entities\",\"entities\":[]},"
               "{\"type\":\"custom:bubble-card\",\"card_type\":\"climate\"},"
               "{\"type\":\"custom:bubble-card\",\"card_type\":\"separator\",\"name\":\"Two\"}"
               "]}]}",
               &l);
    CHECK(l.remotes[0].count == 2, "masonry: 2 elements, got %d", l.remotes[0].count);
    CHECK(STREQ(l.remotes[0].elements[0].name, "One") && STREQ(l.remotes[0].elements[1].name, "Two"),
          "masonry order");
    CHECK(strstr(logbuf, "card type entities not supported") != NULL, "entities logged: %s",
          logbuf);
    CHECK(strstr(logbuf, "bubble-card climate not supported") != NULL, "climate logged");
    layout_free(&l);

    parse_text("{\"views\":[{\"title\":\"S\",\"type\":\"sections\",\"sections\":["
               "{\"type\":\"grid\",\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"separator\",\"name\":\"A\"}]},"
               "{\"type\":\"grid\",\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"separator\",\"name\":\"B\"}]}]}]}",
               &l);
    CHECK(l.remotes[0].count == 2 && STREQ(l.remotes[0].elements[1].name, "B"),
          "sections: cards from all sections in order");
    layout_free(&l);
}

/* ---- 3.5 separator and button ---- */

static void test_separator_button(void)
{
    layout_t l;
    parse_text("{\"views\":[{\"cards\":["
               "{\"type\":\"custom:bubble-card\",\"card_type\":\"separator\",\"name\":\"Licht\","
               "\"icon\":\"mdi:lightbulb\"},"
               "{\"type\":\"custom:bubble-card\",\"card_type\":\"button\",\"entity\":\"light.a\","
               "\"name\":\"Decke\",\"icon\":\"mdi:ceiling-light\"},"
               "{\"type\":\"custom:bubble-card\",\"card_type\":\"button\",\"entity\":\"light.b\","
               "\"tap_action\":{\"action\":\"perform-action\",\"perform_action\":\"light.turn_on\","
               "\"target\":{\"entity_id\":\"light.b\"},\"data\":{\"brightness_pct\":50}}}"
               "]}]}",
               &l);
    remote_t *r = &l.remotes[0];
    CHECK(r->count == 3, "3 elements");
    CHECK(r->elements[0].kind == ELEMENT_HEADING && STREQ(r->elements[0].icon, "mdi:lightbulb"),
          "heading");
    button_t *b = &r->elements[1].row.buttons[0];
    CHECK(r->elements[1].kind == ELEMENT_ROW && r->elements[1].row.count == 1, "button row");
    CHECK(STREQ(b->name, "Decke") && STREQ(b->icon, "mdi:ceiling-light"), "button name/icon");
    CHECK(STREQ(b->tap.domain, "homeassistant") && STREQ(b->tap.service, "toggle") &&
              target_is(&b->tap, "light.a"),
          "button without tap_action toggles its entity");
    button_t *c = &r->elements[2].row.buttons[0];
    CHECK(STREQ(c->tap.domain, "light") && STREQ(c->tap.service, "turn_on") &&
              STREQ(c->tap.service_data_json, "{\"brightness_pct\":50}"),
          "button with perform-action: %s %s", c->tap.service, c->tap.service_data_json);
    CHECK(c->hold.domain == NULL, "no hold action");
    layout_free(&l);
}

/* ---- 3.6, 3.7, 4.4 the real dashboard ---- */

static void test_fixture(void)
{
    cJSON *cfg = load_fixture();
    CHECK(cfg != NULL, "fixture test/host/fixtures/media-control.json not found");
    if (cfg == NULL) {
        return;
    }
    layout_t l;
    logbuf[0] = '\0';
    CHECK(layout_parse(cfg, &l, capture), "fixture parses");
    cJSON_Delete(cfg);
    CHECK(logbuf[0] == '\0', "fixture logs nothing, got: %s", logbuf);

    CHECK(l.count == 1 && STREQ(l.remotes[0].title, "Wohnzimmer"), "one remote Wohnzimmer");
    remote_t *r = &l.remotes[0];
    CHECK(r->count == 7, "heading + media + 5 rows = 7 elements, got %d", r->count);
    if (r->count != 7) {
        layout_free(&l);
        return;
    }

    element_t *h = &r->elements[0];
    CHECK(h->kind == ELEMENT_HEADING && STREQ(h->name, "Apple TV Wohnzimmer"), "heading");

    element_t *m = &r->elements[1];
    CHECK(m->kind == ELEMENT_MEDIA && STREQ(m->entity, "media_player.wohnzimmer_apple_tv_wohnzimmer"),
          "media tile entity");
    CHECK(STREQ(m->name, "Apple TV") && STREQ(m->icon, "mdi:apple"), "media tile name/icon");
    CHECK(m->row.count == 4, "4 media controls without volume, got %d", m->row.count);
    const char *expected_services[] = {"toggle", "media_previous_track", "media_play_pause",
                                       "media_next_track"};
    for (int i = 0; i < 4 && i < m->row.count; i++) {
        CHECK(STREQ(m->row.buttons[i].tap.service, expected_services[i]), "control %d: %s", i,
              m->row.buttons[i].tap.service);
        CHECK(STREQ(m->row.buttons[i].tap.domain, "media_player"), "control domain");
    }
    CHECK(m->row.buttons[2].role == BUTTON_PLAY_PAUSE, "play/pause role");

    int counts[] = {1, 3, 1, 2, 2};
    justify_t justify[] = {JUSTIFY_CENTER, JUSTIFY_CENTER, JUSTIFY_CENTER, JUSTIFY_FILL,
                           JUSTIFY_FILL};
    for (int i = 0; i < 5; i++) {
        element_t *e = &r->elements[2 + i];
        CHECK(e->kind == ELEMENT_ROW && e->row.count == counts[i], "row %d: %d buttons, got %d", i,
              counts[i], e->row.count);
        CHECK(e->row.justify == justify[i], "row %d justify", i);
    }
    CHECK(r->button_count == 4 + 9, "13 buttons, got %d", r->button_count);

    button_t *up = &r->elements[2].row.buttons[0];
    CHECK(STREQ(up->name, "Hoch") && STREQ(up->icon, "mdi:chevron-up"), "Hoch");
    CHECK(up->width == 64 && up->height == 64, "Hoch size %dx%d", up->width, up->height);
    CHECK(STREQ(up->tap.domain, "button") && STREQ(up->tap.service, "press") &&
              target_is(&up->tap, "button.wohnzimmer_apple_tv_wohnzimmer_up"),
          "Hoch tap: %s", up->tap.target_json);
    CHECK(up->hold.domain == NULL, "Hoch hold is none");

    button_t *ok = &r->elements[3].row.buttons[1];
    CHECK(STREQ(ok->name, "OK") && !ok->show_icon && ok->show_name, "OK shows only its name");
    CHECK(ok->width == 80 && ok->height == 80, "OK size");
    CHECK(target_is(&ok->tap, "button.wohnzimmer_apple_tv_wohnzimmer_select"), "OK target");

    button_t *back = &r->elements[5].row.buttons[0];
    CHECK(STREQ(back->name, "Zur\xc3\xbc" "ck"), "Zur\xc3\xbc" "ck name: %s", back->name);
    CHECK(back->width == 0 && back->height == 52, "fill row button: width auto, height 52");
    CHECK(target_is(&back->tap, "button.wohnzimmer_apple_tv_wohnzimmer_back"), "back tap");
    CHECK(target_is(&back->hold, "button.wohnzimmer_apple_tv_wohnzimmer_back_long_press"),
          "back hold: %s", back->hold.target_json);
    button_t *tv = &r->elements[5].row.buttons[1];
    CHECK(target_is(&tv->hold, "button.wohnzimmer_apple_tv_wohnzimmer_tv_long_press"), "TV hold");

    button_t *down = &r->elements[6].row.buttons[0];
    CHECK(STREQ(down->tap.domain, "media_player") && STREQ(down->tap.service, "volume_down") &&
              target_is(&down->tap, "media_player.wohnzimmer_apple_tv_wohnzimmer"),
          "Leiser");
    CHECK(STREQ(r->elements[6].row.buttons[1].tap.service, "volume_up"), "Lauter");
    layout_free(&l);
}

/* ---- 3.7 media player flags ---- */

static void test_media_flags(void)
{
    layout_t l;
    parse_text("{\"views\":[{\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"media-player\",\"entity\":\"media_player.x\"}]}]}",
               &l);
    element_t *m = &l.remotes[0].elements[0];
    CHECK(m->row.count == 6, "all controls incl. volume: %d", m->row.count);
    CHECK(STREQ(m->row.buttons[5].tap.service, "volume_up"), "volume up last");
    layout_free(&l);

    parse_text("{\"views\":[{\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"media-player\",\"entity\":\"media_player.x\",\"hide\":{"
               "\"power_button\":true,\"previous_button\":true,\"next_button\":true,"
               "\"volume_button\":true}}]}]}",
               &l);
    m = &l.remotes[0].elements[0];
    CHECK(m->row.count == 1 && m->row.buttons[0].role == BUTTON_PLAY_PAUSE, "only play/pause");
    layout_free(&l);

    parse_text("{\"views\":[{\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"media-player\"}]}]}",
               &l);
    CHECK(l.remotes[0].count == 0 && strstr(logbuf, "without entity") != NULL,
          "media player without entity skipped");
    layout_free(&l);
}

/* ---- sub-button variants ---- */

static void test_sub_buttons(void)
{
    layout_t l;
    parse_text("{\"views\":[{\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"sub-buttons\",\"entity\":\"light.x\",\"sub_button\":["
               "{\"name\":\"A\",\"width\":\"64px\"},{\"name\":\"B\",\"tap_action\":"
               "{\"action\":\"toggle\"}}]}]}]}",
               &l);
    element_t *e = &l.remotes[0].elements[0];
    CHECK(e->kind == ELEMENT_ROW && e->row.count == 2, "flat list: one row of 2");
    CHECK(e->row.buttons[0].width == 64, "width from \"64px\"");
    CHECK(e->row.buttons[0].tap.domain == NULL, "sub-button without action does nothing");
    CHECK(target_is(&e->row.buttons[1].tap, "light.x"), "toggle uses the card entity");
    layout_free(&l);

    parse_text("{\"views\":[{\"cards\":[{\"type\":\"custom:bubble-card\","
               "\"card_type\":\"sub-buttons\",\"sub_button\":{\"main\":[{\"name\":\"M\"}],"
               "\"bottom\":[{\"name\":\"single\",\"entity\":\"switch.s\",\"tap_action\":"
               "{\"action\":\"toggle\"}},{\"group\":[]}]}}]}]}",
               &l);
    remote_t *r = &l.remotes[0];
    CHECK(r->count == 2, "main row + single-button row, empty group dropped: %d", r->count);
    CHECK(STREQ(r->elements[0].row.buttons[0].name, "M"), "main row first");
    CHECK(target_is(&r->elements[1].row.buttons[0].tap, "switch.s"),
          "single sub-button uses its own entity");
    layout_free(&l);
}

/* ---- 3.8 limits ---- */

static void test_limits(void)
{
    char *json = malloc(65536);
    strcpy(json, "{\"views\":[{\"title\":\"Big\",\"cards\":[{\"type\":\"custom:bubble-card\","
                 "\"card_type\":\"sub-buttons\",\"sub_button\":{\"bottom\":[");
    for (int row = 0; row < 8; row++) {
        strcat(json, row ? ",{\"group\":[" : "{\"group\":[");
        for (int i = 0; i < 10; i++) {
            strcat(json, i ? ",{\"name\":\"b\"}" : "{\"name\":\"b\"}");
        }
        strcat(json, "]}");
    }
    strcat(json, "]}}]}]}");
    layout_t l;
    parse_text(json, &l);
    CHECK(l.remotes[0].button_count == 64, "64 buttons kept, got %d", l.remotes[0].button_count);
    CHECK(l.remotes[0].count == 7, "7 rows (6 full + 1 with 4), got %d", l.remotes[0].count);
    CHECK(strstr(logbuf, "80 buttons, only the first 64 are shown, 16 left out") != NULL,
          "limit logged: %s", logbuf);
    layout_free(&l);

    strcpy(json, "{\"views\":[");
    for (int i = 0; i < 17; i++) {
        strcat(json, i ? ",{}" : "{}");
    }
    strcat(json, "]}");
    parse_text(json, &l);
    CHECK(l.count == 16, "16 remotes kept");
    CHECK(strstr(logbuf, "17 views, only the first 16") != NULL, "view limit logged");
    layout_free(&l);
    free(json);
}

/* ---- 4.1-4.3 action mapping ---- */

static void action_case(const char *json, const char *entity, const char *domain,
                        const char *service, const char *data, const char *target)
{
    cJSON *a = cJSON_Parse(json);
    call_t c;
    bool ok = action_to_call(a, entity, &c);
    cJSON_Delete(a);
    if (domain == NULL) {
        CHECK(!ok && c.domain == NULL, "expected no call for %s", json);
    } else {
        CHECK(ok && STREQ(c.domain, domain) && STREQ(c.service, service) &&
                  STREQ(c.service_data_json, data) && STREQ(c.target_json, target),
              "%s -> %s.%s %s %s", json, c.domain, c.service, c.service_data_json, c.target_json);
    }
    call_free(&c);
}

static void test_actions(void)
{
    action_case("{\"action\":\"perform-action\",\"perform_action\":\"button.press\","
                "\"target\":{\"entity_id\":\"button.x\"}}",
                NULL, "button", "press", "{}", "{\"entity_id\":\"button.x\"}");
    action_case("{\"action\":\"perform-action\",\"perform_action\":\"light.turn_on\","
                "\"target\":{\"area_id\":[\"wz\"]},\"data\":{\"brightness_pct\":50}}",
                NULL, "light", "turn_on", "{\"brightness_pct\":50}", "{\"area_id\":[\"wz\"]}");
    action_case("{\"action\":\"call-service\",\"service\":\"media_player.volume_set\","
                "\"service_data\":{\"entity_id\":\"media_player.tv\",\"volume_level\":0.4}}",
                NULL, "media_player", "volume_set",
                "{\"entity_id\":\"media_player.tv\",\"volume_level\":0.4}", "{}");
    action_case("{\"action\":\"toggle\"}", "switch.plug", "homeassistant", "toggle", "{}",
                "{\"entity_id\":\"switch.plug\"}");
    action_case("{\"action\":\"toggle\"}", NULL, NULL, NULL, NULL, NULL);
    action_case("{\"action\":\"none\"}", "x.y", NULL, NULL, NULL, NULL);
    action_case("{}", "x.y", NULL, NULL, NULL, NULL);
    action_case("{\"action\":\"more-info\"}", "x.y", NULL, NULL, NULL, NULL);
    action_case("{\"action\":\"navigate\",\"navigation_path\":\"/x\"}", NULL, NULL, NULL, NULL,
                NULL);
    action_case("{\"action\":\"url\",\"url_path\":\"https://example.com\"}", NULL, NULL, NULL,
                NULL, NULL);
    action_case("{\"action\":\"perform-action\"}", NULL, NULL, NULL, NULL, NULL);
    action_case("{\"action\":\"perform-action\",\"perform_action\":\"nodot\"}", NULL, NULL, NULL,
                NULL, NULL);
    action_case("{\"action\":\"perform-action\",\"perform_action\":\".press\"}", NULL, NULL,
                NULL, NULL, NULL);

    call_t c;
    CHECK(!action_to_call(NULL, "x.y", &c), "NULL action");
}

int main(void)
{
    test_views();
    test_card_discovery();
    test_separator_button();
    test_fixture();
    test_media_flags();
    test_sub_buttons();
    test_limits();
    test_actions();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
