/* Host unit tests for the generated icon and time-zone tables. */

#include <stdio.h>
#include <string.h>

#include "assets.h"

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

int main(void)
{
    /* 2.5 icons */
    int i = icon_index("mdi:chevron-up");
    CHECK(i >= 0 && strcmp(icon_table[i].name, "chevron-up") == 0, "mdi:chevron-up found");
    CHECK(icon_index("chevron-up") == i, "name without prefix");
    CHECK(icon_index("mdi:does-not-exist") == -1, "unknown icon rejected");
    CHECK(icon_index("") == -1 && icon_index(NULL) == -1, "empty and NULL");
    CHECK(icon_index("mdi:") == -1, "prefix only");
    for (int k = 0; k < icon_count; k++) {
        CHECK(icon_index(icon_table[k].name) == k, "every icon found: %s", icon_table[k].name);
        CHECK(icon_table[k].offset48 + 48 * 48 / 2 <= icon_blob_size, "offset in blob");
        if (k > 0) {
            CHECK(strcmp(icon_table[k - 1].name, icon_table[k].name) < 0, "table sorted");
        }
    }
    const char *needed[] = {"apple", "sofa", "chevron-down", "chevron-left", "chevron-right",
                            "arrow-u-left-top", "television", "volume-minus", "volume-plus",
                            "cog", "play", "pause", "skip-next", "skip-previous", "power",
                            "wifi-strength-1", "wifi-strength-4", "wifi-strength-off-outline",
                            "wifi-strength-alert-outline", "battery", "battery-outline",
                            "battery-alert-variant-outline"};
    for (size_t k = 0; k < sizeof needed / sizeof needed[0]; k++) {
        CHECK(icon_index(needed[k]) >= 0, "required icon %s", needed[k]);
    }

    /* 2.7 time zones */
    const char *berlin = tz_posix_for("Europe/Berlin");
    CHECK(berlin != NULL && strcmp(berlin, "CET-1CEST,M3.5.0,M10.5.0/3") == 0, "Europe/Berlin: %s",
          berlin ? berlin : "(null)");
    CHECK(tz_posix_for("Etc/UTC") != NULL, "Etc/UTC known");
    CHECK(tz_posix_for("America/New_York") != NULL, "America/New_York known");
    CHECK(tz_posix_for("Mars/Olympus_Mons") == NULL, "unknown zone");
    CHECK(tz_posix_for(NULL) == NULL, "NULL zone");
    for (int k = 1; k < tz_count; k++) {
        CHECK(strcmp(tz_table[k - 1].iana, tz_table[k].iana) < 0, "tz table sorted at %d", k);
    }

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
