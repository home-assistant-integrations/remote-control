/* Host unit tests for media_state_apply() (subscribe_entities events). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media_state.h"

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

static const char *TV = "media_player.wohnzimmer_apple_tv_wohnzimmer";

static int reports;
static ha_media_t last;

static void on_changed(const ha_media_t *m, void *ctx)
{
    (void)ctx;
    reports++;
    last = *m;
}

static void apply_text(ha_media_t *list, int count, const char *json)
{
    cJSON *ev = cJSON_Parse(json);
    CHECK(ev != NULL, "test JSON parses");
    media_state_apply(list, count, ev, on_changed, NULL);
    cJSON_Delete(ev);
}

/* Two events recorded from Home Assistant 2026.11.2: the complete state, and
 * a position update 30 seconds later. */
static void test_recorded(void)
{
    FILE *f = fopen("test/host/fixtures/subscribe_entities.json", "rb");
    CHECK(f != NULL, "fixture test/host/fixtures/subscribe_entities.json not found");
    if (f == NULL) {
        return;
    }
    static char buf[16384];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    cJSON *events = cJSON_Parse(buf);
    CHECK(cJSON_GetArraySize(events) == 2, "two recorded events");

    ha_media_t list[2] = {0};
    snprintf(list[0].entity_id, sizeof list[0].entity_id, "%s", TV);
    snprintf(list[1].entity_id, sizeof list[1].entity_id, "media_player.other");

    reports = 0;
    media_state_apply(list, 2, cJSON_GetArrayItem(events, 0), on_changed, NULL);
    CHECK(reports == 1, "complete state reported once, got %d", reports);
    CHECK(strcmp(last.entity_id, TV) == 0, "reported entity");
    CHECK(strcmp(last.state, "playing") == 0, "state playing: %s", last.state);
    CHECK(strcmp(last.app, "waipu") == 0, "app_name: %s", last.app);
    CHECK(last.title[0] == '\0' && last.picture[0] == '\0', "no title or picture");
    CHECK(list[1].state[0] == '\0', "other player untouched");

    reports = 0;
    media_state_apply(list, 2, cJSON_GetArrayItem(events, 1), on_changed, NULL);
    CHECK(reports == 0, "position-only update not reported, got %d", reports);
    CHECK(strcmp(list[0].app, "waipu") == 0, "app kept after partial update");
    cJSON_Delete(events);
}

static void test_changes(void)
{
    ha_media_t list[1] = {0};
    snprintf(list[0].entity_id, sizeof list[0].entity_id, "%s", TV);
    char json[1024];

    snprintf(json, sizeof json,
             "{\"a\":{\"%s\":{\"s\":\"paused\",\"a\":{\"media_title\":\"Tatort\","
             "\"app_name\":\"ARD\",\"entity_picture\":\"/api/media_player_proxy/x?token=1\"}}}}",
             TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && strcmp(last.title, "Tatort") == 0, "title from complete state");

    snprintf(json, sizeof json, "{\"c\":{\"%s\":{\"+\":{\"s\":\"playing\"}}}}", TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && strcmp(last.state, "playing") == 0, "state change reported");
    CHECK(strcmp(last.title, "Tatort") == 0, "title kept on state change");

    snprintf(json, sizeof json,
             "{\"c\":{\"%s\":{\"+\":{\"a\":{\"media_title\":\"Tagesschau\"}}}}}", TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && strcmp(last.title, "Tagesschau") == 0, "title change reported");

    snprintf(json, sizeof json,
             "{\"c\":{\"%s\":{\"+\":{\"a\":{\"media_title\":\"Tagesschau\"}}}}}", TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 0, "same title again not reported");

    snprintf(json, sizeof json,
             "{\"c\":{\"%s\":{\"-\":{\"a\":[\"media_title\",\"entity_picture\"]}}}}", TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && last.title[0] == '\0' && last.picture[0] == '\0',
          "removed attributes cleared");
    CHECK(strcmp(last.app, "ARD") == 0, "other attributes kept");

    snprintf(json, sizeof json,
             "{\"c\":{\"%s\":{\"+\":{\"s\":\"off\",\"a\":{}},\"-\":{\"a\":[\"app_name\"]}}}}", TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && strcmp(last.state, "off") == 0 && last.app[0] == '\0',
          "state and removal in one change");

    snprintf(json, sizeof json, "{\"r\":[\"%s\"]}", TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && strcmp(last.state, "unavailable") == 0, "removed entity");

    reports = 0;
    apply_text(list, 1, "{\"c\":{\"media_player.unknown\":{\"+\":{\"s\":\"on\"}}},\"r\":[5]}");
    CHECK(reports == 0, "unknown entity and bad removal ignored");

    snprintf(json, sizeof json, "{\"a\":{\"%s\":{\"s\":\"idle\",\"a\":{\"media_title\":7}}}}",
             TV);
    reports = 0;
    apply_text(list, 1, json);
    CHECK(reports == 1 && last.title[0] == '\0', "non-string title becomes empty");

    char longtitle[600];
    memset(longtitle, 'x', sizeof longtitle - 1);
    longtitle[sizeof longtitle - 1] = '\0';
    char *big = malloc(2048);
    snprintf(big, 2048, "{\"c\":{\"%s\":{\"+\":{\"a\":{\"media_title\":\"%s\"}}}}}", TV,
             longtitle);
    apply_text(list, 1, big);
    CHECK(strlen(list[0].title) == sizeof list[0].title - 1, "long title truncated safely");
    free(big);
}

int main(void)
{
    test_recorded();
    test_changes();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
