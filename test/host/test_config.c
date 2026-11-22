/*
 * Host unit tests for config_validate(). Run with `make test-host`.
 * Each test corresponds to a rule or scenario in
 * openspec/changes/add-initial-setup/specs/device-configuration/spec.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_config.h"

static int failures;
static int checks;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

static const char *TOKEN =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJleGFtcGxlIn0.c2lnbmF0dXJl";

/* Builds a configuration from its parts. Parts are inserted as raw JSON. */
static char *make_config(const char *version, const char *wifi, const char *ha)
{
    static char buf[8192];
    snprintf(buf, sizeof buf, "{\"version\":%s,\"wifi\":%s,\"home_assistant\":%s}", version, wifi,
             ha);
    return buf;
}

static const char *WIFI_OK = "{\"ssid\":\"MyNetwork\",\"password\":\"secret-password\"}";

static char *ha_with(const char *url, const char *token, const char *dashboard)
{
    static char buf[4096];
    snprintf(buf, sizeof buf, "{\"url\":%s,\"token\":%s,\"dashboard\":%s}", url, token, dashboard);
    return buf;
}

static char *quoted(const char *s)
{
    static char bufs[4][1024];
    static int n;
    char *b = bufs[n++ % 4];
    snprintf(b, sizeof bufs[0], "\"%s\"", s);
    return b;
}

static bool validate(const char *json, config_t *cfg, config_errors_t *errs)
{
    return config_validate(json, strlen(json), cfg, errs);
}

static bool has_error(const config_errors_t *errs, const char *prefix)
{
    for (int i = 0; i < errs->count && i < CONFIG_MAX_ERRORS; i++) {
        if (strncmp(errs->msg[i], prefix, strlen(prefix)) == 0) {
            return true;
        }
    }
    return false;
}

static void print_errors(const config_errors_t *errs)
{
    for (int i = 0; i < errs->count && i < CONFIG_MAX_ERRORS; i++) {
        printf("       %s\n", errs->msg[i]);
    }
}

/* Expects the file to fail with exactly one error starting with prefix. */
static void expect_one_error(const char *name, const char *json, const char *prefix)
{
    config_errors_t errs;
    config_t cfg;
    bool ok = validate(json, &cfg, &errs);
    CHECK(!ok && errs.count == 1 && has_error(&errs, prefix), "%s: expected one error '%s'", name,
          prefix);
    if (ok || errs.count != 1 || !has_error(&errs, prefix)) {
        print_errors(&errs);
    }
    config_free(&cfg);
}

static void expect_valid(const char *name, const char *json)
{
    config_errors_t errs;
    config_t cfg;
    bool ok = validate(json, &cfg, &errs);
    CHECK(ok && errs.count == 0, "%s: expected valid", name);
    if (!ok) {
        print_errors(&errs);
    }
    config_free(&cfg);
}

/* ---- 3.2 valid files ---- */

static void test_valid(void)
{
    config_errors_t errs;
    config_t cfg;
    const char *json =
        make_config("1", WIFI_OK,
                    ha_with(quoted("http://homeassistant.local:8123"), quoted(TOKEN),
                            quoted("media-control")));
    bool ok = validate(json, &cfg, &errs);
    CHECK(ok, "valid file rejected");
    CHECK(ok && strcmp(cfg.wifi_ssid, "MyNetwork") == 0, "ssid");
    CHECK(ok && strcmp(cfg.wifi_password, "secret-password") == 0, "password");
    CHECK(ok && strcmp(cfg.ha_url, "http://homeassistant.local:8123") == 0, "url");
    CHECK(ok && strcmp(cfg.ha_token, TOKEN) == 0, "token");
    CHECK(ok && strcmp(cfg.ha_dashboard, "media-control") == 0, "dashboard");
    config_free(&cfg);
    CHECK(cfg.ha_token == NULL, "config_free clears fields");

    expect_valid("open network", make_config("1", "{\"ssid\":\"Open\",\"password\":\"\"}",
                                             ha_with(quoted("https://ha.example.com"),
                                                     quoted(TOKEN), quoted("remote_1"))));
    expect_valid("whitespace around the object", "  \n{\"version\":1,"
                                                 "\"wifi\":{\"ssid\":\"a\",\"password\":\"\"},"
                                                 "\"home_assistant\":{\"url\":\"http://h\","
                                                 "\"token\":\"a.b.c\",\"dashboard\":\"d\"}}\n\n");
    expect_valid("utf-8 ssid", make_config("1", "{\"ssid\":\"Caf\xc3\xa9\",\"password\":\"\"}",
                                           ha_with(quoted("http://h"), quoted("a.b.c"),
                                                   quoted("d"))));
}

static void test_example_file(void)
{
    FILE *f = fopen("docs/user/config.example.json", "rb");
    CHECK(f != NULL, "docs/user/config.example.json not found (run from the project root)");
    if (f == NULL) {
        return;
    }
    char buf[CONFIG_MAX_SIZE + 1];
    size_t len = fread(buf, 1, sizeof buf, f);
    fclose(f);
    config_errors_t errs;
    config_t cfg;
    bool ok = config_validate(buf, len, &cfg, &errs);
    CHECK(ok, "example file is invalid");
    if (!ok) {
        print_errors(&errs);
    }
    config_free(&cfg);
}

/* ---- 3.3 file-level errors and spec scenarios ---- */

static void test_file_errors(void)
{
    expect_one_error("not JSON", "{\"version\": 1,", "file: not valid JSON");
    expect_one_error("trailing garbage", "{} x", "file: not valid JSON");
    expect_one_error("array", "[1,2]", "file: must be a JSON object");
    expect_one_error("invalid UTF-8", "{\"version\":1,\"x\":\"\xff\"}", "file: not valid UTF-8");

    char *big = malloc(CONFIG_MAX_SIZE + 2);
    memset(big, ' ', CONFIG_MAX_SIZE + 1);
    big[0] = '{';
    big[CONFIG_MAX_SIZE - 1] = '}';
    big[CONFIG_MAX_SIZE + 1] = '\0';
    expect_one_error("too large", big, "file: larger than 4096 bytes");
    big[CONFIG_MAX_SIZE] = '\0'; /* exactly 4096 bytes: size is fine */
    config_errors_t errs;
    validate(big, NULL, &errs);
    CHECK(!has_error(&errs, "file: larger"), "4096 bytes must not be too large");
    free(big);

    const char *ha = ha_with(quoted("http://h"), quoted(TOKEN), quoted("d"));
    expect_one_error("version 2", make_config("2", WIFI_OK, ha), "version: must be 1");
    expect_one_error("version string", make_config("\"1\"", WIFI_OK, ha), "version: must be 1");
    expect_one_error("version missing",
                     "{\"wifi\":{\"ssid\":\"a\",\"password\":\"\"},"
                     "\"home_assistant\":{\"url\":\"http://h\",\"token\":\"a.b.c\","
                     "\"dashboard\":\"d\"}}",
                     "version: missing");
    expect_one_error("wifi not object", make_config("1", "\"x\"", ha), "wifi: must be an object");
    expect_one_error("ssid not string", make_config("1", "{\"ssid\":5,\"password\":\"\"}", ha),
                     "wifi.ssid: must be a string");
}

static void test_scenario_several_errors(void)
{
    /* spec: "Several errors" */
    config_errors_t errs;
    const char *json = make_config("1", "{\"ssid\":\"\",\"password\":\"short\"}",
                                   "{\"url\":\"http://h\",\"dashboard\":\"d\"}");
    bool ok = validate(json, NULL, &errs);
    CHECK(!ok && errs.count == 3, "several errors: expected 3, got %d", errs.count);
    CHECK(has_error(&errs, "wifi.ssid:"), "several errors: wifi.ssid");
    CHECK(has_error(&errs, "wifi.password:"), "several errors: wifi.password");
    CHECK(has_error(&errs, "home_assistant.token: missing"), "several errors: token");
    if (errs.count != 3) {
        print_errors(&errs);
    }
}

static void test_scenario_misspelled_key(void)
{
    /* spec: "Misspelled key" */
    config_errors_t errs;
    const char *json = make_config("1", "{\"ssid\":\"x\",\"pasword\":\"12345678\"}",
                                   ha_with(quoted("http://h"), quoted(TOKEN), quoted("d")));
    bool ok = validate(json, NULL, &errs);
    CHECK(!ok && errs.count == 2, "misspelled key: expected 2 errors, got %d", errs.count);
    CHECK(has_error(&errs, "wifi.pasword: unknown key"), "misspelled key: unknown key");
    CHECK(has_error(&errs, "wifi.password: missing"), "misspelled key: missing password");

    validate("{\"version\":1,\"extra\":true,\"wifi\":{\"ssid\":\"a\",\"password\":\"\"},"
             "\"home_assistant\":{\"url\":\"http://h\",\"token\":\"a.b.c\",\"dashboard\":\"d\"}}",
             NULL, &errs);
    CHECK(errs.count == 1 && has_error(&errs, "extra: unknown key"), "unknown top-level key");
}

/* ---- wifi password rules ---- */

static void test_password(void)
{
    const char *ha = ha_with(quoted("http://h"), quoted(TOKEN), quoted("d"));
    char wifi[256];
    snprintf(wifi, sizeof wifi, "{\"ssid\":\"a\",\"password\":\"%s\"}", "1234567");
    expect_one_error("password 7 chars", make_config("1", wifi, ha), "wifi.password:");
    snprintf(wifi, sizeof wifi, "{\"ssid\":\"a\",\"password\":\"%s\"}", "12345678");
    expect_valid("password 8 chars", make_config("1", wifi, ha));
    char p63[64];
    memset(p63, 'x', 63);
    p63[63] = '\0';
    snprintf(wifi, sizeof wifi, "{\"ssid\":\"a\",\"password\":\"%s\"}", p63);
    expect_valid("password 63 chars", make_config("1", wifi, ha));
    snprintf(wifi, sizeof wifi, "{\"ssid\":\"a\",\"password\":\"%sx\"}", p63);
    expect_one_error("password 64 chars", make_config("1", wifi, ha), "wifi.password:");

    snprintf(wifi, sizeof wifi, "{\"ssid\":\"%s\",\"password\":\"\"}",
             "123456789012345678901234567890123");
    expect_one_error("ssid 33 bytes", make_config("1", wifi, ha), "wifi.ssid:");
}

/* ---- 3.4 URL ---- */

static void test_url(void)
{
    config_errors_t errs;
    config_t cfg;
    /* spec: "Trailing slash in URL" */
    bool ok = validate(make_config("1", WIFI_OK,
                                   ha_with(quoted("http://homeassistant.local:8123/"),
                                           quoted(TOKEN), quoted("d"))),
                       &cfg, &errs);
    CHECK(ok && strcmp(cfg.ha_url, "http://homeassistant.local:8123") == 0,
          "trailing slash not removed: %s", ok ? cfg.ha_url : "(invalid)");
    config_free(&cfg);

    struct {
        const char *url;
        const char *error; /* NULL: valid */
    } cases[] = {
        {"http://ha", NULL},
        {"https://ha.example.com:443", NULL},
        {"http://192.168.1.10:8123", NULL},
        {"http://[::1]:8123", NULL},
        {"http://[fe80::1]/", NULL},
        {"http://h/api", "home_assistant.url: must not have a path"},
        {"http://h//", "home_assistant.url: must not have a path"},
        {"http://h?x=1", "home_assistant.url: must not have a path"},
        {"http://h#top", "home_assistant.url: must not have a path"},
        {"http://h:8123/lovelace", "home_assistant.url: must not have a path"},
        {"ftp://h", "home_assistant.url: must start with"},
        {"HTTP://h", "home_assistant.url: must start with"},
        {"homeassistant.local", "home_assistant.url: must start with"},
        {"ws://h", "home_assistant.url: must start with"},
        {"http://", "home_assistant.url: must have a valid host"},
        {"http://:8123", "home_assistant.url: must have a valid host"},
        {"http://user@h", "home_assistant.url: must have a valid host"},
        {"http://h a", "home_assistant.url: must have a valid host"},
        {"http://h:0", "home_assistant.url: port"},
        {"http://h:65536", "home_assistant.url: port"},
        {"http://h:99999999999", "home_assistant.url: port"},
        {"http://h:", "home_assistant.url: port"},
        {"http://h:80a", "home_assistant.url: must have a valid host"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const char *json =
            make_config("1", WIFI_OK, ha_with(quoted(cases[i].url), quoted(TOKEN), quoted("d")));
        if (cases[i].error == NULL) {
            expect_valid(cases[i].url, json);
        } else {
            expect_one_error(cases[i].url, json, cases[i].error);
        }
    }
}

/* ---- 3.5 token and dashboard ---- */

static void test_token_dashboard(void)
{
    struct {
        const char *token;
        bool valid;
    } tokens[] = {
        {"a.b.c", true},
        {"abc-_.DEF.123", true},
        {"a.b", false},
        {"a.b.c.d", false},
        {"a..c", false},
        {".b.c", false},
        {"a.b.", false},
        {"a.b c.d", false},
        {"a.b.c ", false},
        {" a.b.c", false},
        {"a.b.c\\n", false},
        {"a.b+.c", false},
        {"a.b=.c", false},
        {"", false},
    };
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        const char *json = make_config(
            "1", WIFI_OK, ha_with(quoted("http://h"), quoted(tokens[i].token), quoted("d")));
        if (tokens[i].valid) {
            expect_valid(tokens[i].token, json);
        } else {
            expect_one_error(tokens[i].token, json, "home_assistant.token:");
        }
    }

    char long_name[66];
    memset(long_name, 'a', 65);
    long_name[65] = '\0';
    struct {
        const char *dashboard;
        bool valid;
    } dashboards[] = {
        {"media-control", true},
        {"remote_2", true},
        {"", false},
        {"Media-Control", false},
        {"media control", false},
        {"media/control", false},
        {long_name, false},
        {long_name + 1, true}, /* 64 characters */
    };
    for (size_t i = 0; i < sizeof dashboards / sizeof dashboards[0]; i++) {
        const char *json = make_config(
            "1", WIFI_OK, ha_with(quoted("http://h"), quoted(TOKEN), quoted(dashboards[i].dashboard)));
        if (dashboards[i].valid) {
            expect_valid(dashboards[i].dashboard, json);
        } else {
            expect_one_error(dashboards[i].dashboard, json, "home_assistant.dashboard:");
        }
    }
}

/* ---- 2.6 / spec "Token is not exposed" ---- */

static void test_no_values_in_errors(void)
{
    const char *secret = "SECRETVALUE";
    char wifi[128];
    snprintf(wifi, sizeof wifi, "{\"ssid\":\"\",\"password\":\"%s%s\"}", secret, secret);
    char token[64];
    snprintf(token, sizeof token, "%s secret", secret);
    char json[1024];
    snprintf(json, sizeof json, "%s",
             make_config("1", wifi, ha_with(quoted("http://h/SECRETVALUE"), quoted(token),
                                            quoted("SECRETVALUE"))));
    config_errors_t errs;
    validate(json, NULL, &errs);
    CHECK(errs.count >= 4, "expected several errors, got %d", errs.count);
    for (int i = 0; i < errs.count && i < CONFIG_MAX_ERRORS; i++) {
        CHECK(strstr(errs.msg[i], secret) == NULL, "error message contains a value: %s",
              errs.msg[i]);
    }
}

int main(void)
{
    test_valid();
    test_example_file();
    test_file_errors();
    test_scenario_several_errors();
    test_scenario_misspelled_key();
    test_password();
    test_url();
    test_token_dashboard();
    test_no_values_in_errors();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
