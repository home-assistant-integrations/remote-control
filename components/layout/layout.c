/*
 * Dashboard configuration to remotes. See layout.h.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "layout.h"

/* ---- helpers ---- */

typedef struct {
    layout_log_fn log;
    bool oom;
} ctx_t;

static void logf_(ctx_t *ctx, const char *fmt, ...)
{
    if (ctx->log == NULL) {
        return;
    }
    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    ctx->log(msg);
}

static char *dup_opt(ctx_t *ctx, const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d == NULL) {
        ctx->oom = true;
        return NULL;
    }
    memcpy(d, s, n);
    return d;
}

static const char *str(const cJSON *obj, const char *key)
{
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
}

/* Booleans default to def when missing or not a boolean. */
static bool flag(const cJSON *obj, const char *key, bool def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : def;
}

/* CSS pixel sizes: a number, or a string like "64" or "64px". 0 if absent. */
static int pixels(const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    int px = 0;
    if (cJSON_IsNumber(v)) {
        px = (int)v->valuedouble;
    } else if (cJSON_IsString(v) && v->valuestring != NULL) {
        px = atoi(v->valuestring);
    }
    return px > 0 && px <= 2000 ? px : 0;
}

/* ---- building blocks ---- */

static void button_free(button_t *b)
{
    free(b->name);
    free(b->icon);
    call_free(&b->tap);
    call_free(&b->hold);
    memset(b, 0, sizeof *b);
}

static void row_free(button_row_t *row)
{
    for (int i = 0; i < row->count; i++) {
        button_free(&row->buttons[i]);
    }
    free(row->buttons);
    memset(row, 0, sizeof *row);
}

static void element_free(element_t *e)
{
    free(e->name);
    free(e->icon);
    free(e->entity);
    row_free(&e->row);
    memset(e, 0, sizeof *e);
}

static button_t *row_add(ctx_t *ctx, button_row_t *row)
{
    button_t *b = realloc(row->buttons, (size_t)(row->count + 1) * sizeof *b);
    if (b == NULL) {
        ctx->oom = true;
        return NULL;
    }
    row->buttons = b;
    b = &row->buttons[row->count++];
    memset(b, 0, sizeof *b);
    return b;
}

static element_t *remote_add(ctx_t *ctx, remote_t *r, element_kind_t kind)
{
    element_t *e = realloc(r->elements, (size_t)(r->count + 1) * sizeof *e);
    if (e == NULL) {
        ctx->oom = true;
        return NULL;
    }
    r->elements = e;
    e = &r->elements[r->count++];
    memset(e, 0, sizeof *e);
    e->kind = kind;
    return e;
}

/* Fills a button from a Bubble Card button or sub-button object. */
static void button_from_json(ctx_t *ctx, button_t *b, const cJSON *obj, const char *card_entity,
                             bool default_toggle)
{
    const char *entity = str(obj, "entity");
    if (entity == NULL) {
        entity = card_entity;
    }
    b->name = dup_opt(ctx, str(obj, "name"));
    b->icon = dup_opt(ctx, str(obj, "icon"));
    b->show_icon = flag(obj, "show_icon", true);
    b->show_name = flag(obj, "show_name", true);
    b->width = pixels(obj, "width");
    b->height = pixels(obj, "custom_height");
    b->role = BUTTON_PLAIN;

    const cJSON *tap = cJSON_GetObjectItemCaseSensitive(obj, "tap_action");
    if (tap != NULL) {
        action_to_call(tap, entity, &b->tap);
    } else if (default_toggle && entity != NULL) {
        /* A Bubble Card button without tap_action toggles its entity. */
        cJSON *t = cJSON_CreateObject();
        cJSON_AddStringToObject(t, "action", "toggle");
        action_to_call(t, entity, &b->tap);
        cJSON_Delete(t);
    }
    action_to_call(cJSON_GetObjectItemCaseSensitive(obj, "hold_action"), entity, &b->hold);
}

static void media_control(ctx_t *ctx, button_row_t *row, const char *entity, const char *service,
                          const char *icon, button_role_t role)
{
    button_t *b = row_add(ctx, row);
    if (b == NULL) {
        return;
    }
    b->icon = dup_opt(ctx, icon);
    b->show_icon = true;
    b->show_name = false;
    b->role = role;
    cJSON *action = cJSON_CreateObject();
    char svc[64];
    snprintf(svc, sizeof svc, "media_player.%s", service);
    cJSON_AddStringToObject(action, "action", "perform-action");
    cJSON_AddStringToObject(action, "perform_action", svc);
    cJSON *target = cJSON_AddObjectToObject(action, "target");
    cJSON_AddStringToObject(target, "entity_id", entity);
    action_to_call(action, entity, &b->tap);
    cJSON_Delete(action);
}

/* ---- cards ---- */

static void add_separator(ctx_t *ctx, remote_t *r, const cJSON *card)
{
    element_t *e = remote_add(ctx, r, ELEMENT_HEADING);
    if (e != NULL) {
        e->name = dup_opt(ctx, str(card, "name"));
        e->icon = dup_opt(ctx, str(card, "icon"));
    }
}

static void add_button_card(ctx_t *ctx, remote_t *r, const cJSON *card)
{
    element_t *e = remote_add(ctx, r, ELEMENT_ROW);
    if (e == NULL) {
        return;
    }
    e->row.justify = JUSTIFY_FILL;
    button_t *b = row_add(ctx, &e->row);
    if (b != NULL) {
        button_from_json(ctx, b, card, str(card, "entity"), true);
    }
}

static void add_media_player(ctx_t *ctx, remote_t *r, const cJSON *card)
{
    const char *entity = str(card, "entity");
    if (entity == NULL) {
        logf_(ctx, "media-player card without entity skipped");
        return;
    }
    element_t *e = remote_add(ctx, r, ELEMENT_MEDIA);
    if (e == NULL) {
        return;
    }
    e->entity = dup_opt(ctx, entity);
    e->name = dup_opt(ctx, str(card, "name"));
    e->icon = dup_opt(ctx, str(card, "icon"));
    e->row.justify = JUSTIFY_FILL;

    const cJSON *hide = cJSON_GetObjectItemCaseSensitive(card, "hide");
    if (!flag(hide, "power_button", false)) {
        media_control(ctx, &e->row, entity, "toggle", "mdi:power", BUTTON_PLAIN);
    }
    if (!flag(hide, "previous_button", false)) {
        media_control(ctx, &e->row, entity, "media_previous_track", "mdi:skip-previous",
                      BUTTON_PLAIN);
    }
    if (!flag(hide, "play_pause_button", false)) {
        media_control(ctx, &e->row, entity, "media_play_pause", "mdi:play", BUTTON_PLAY_PAUSE);
    }
    if (!flag(hide, "next_button", false)) {
        media_control(ctx, &e->row, entity, "media_next_track", "mdi:skip-next", BUTTON_PLAIN);
    }
    if (!flag(hide, "volume_button", false)) {
        media_control(ctx, &e->row, entity, "volume_down", "mdi:volume-minus", BUTTON_PLAIN);
        media_control(ctx, &e->row, entity, "volume_up", "mdi:volume-plus", BUTTON_PLAIN);
    }
}

/* One row of sub-buttons: either a group ({"group":[...]}) or a single
 * sub-button object. */
static void add_sub_row(ctx_t *ctx, remote_t *r, const cJSON *entry, const char *card_entity)
{
    const cJSON *group = cJSON_GetObjectItemCaseSensitive(entry, "group");
    element_t *e = remote_add(ctx, r, ELEMENT_ROW);
    if (e == NULL) {
        return;
    }
    const char *justify = str(entry, "justify_content");
    e->row.justify = justify != NULL && strcmp(justify, "fill") == 0 ? JUSTIFY_FILL : JUSTIFY_CENTER;

    if (cJSON_IsArray(group)) {
        const cJSON *item;
        cJSON_ArrayForEach(item, group) {
            if (cJSON_IsObject(item)) {
                button_t *b = row_add(ctx, &e->row);
                if (b != NULL) {
                    button_from_json(ctx, b, item, card_entity, false);
                }
            }
        }
    } else {
        e->row.justify = JUSTIFY_FILL;
        button_t *b = row_add(ctx, &e->row);
        if (b != NULL) {
            button_from_json(ctx, b, entry, card_entity, false);
        }
    }
}

static void add_sub_buttons(ctx_t *ctx, remote_t *r, const cJSON *card)
{
    const char *entity = str(card, "entity");
    const cJSON *sub = cJSON_GetObjectItemCaseSensitive(card, "sub_button");
    if (cJSON_IsArray(sub)) {
        /* Older format: a flat list of sub-buttons, shown as one row. */
        element_t *e = remote_add(ctx, r, ELEMENT_ROW);
        if (e == NULL) {
            return;
        }
        e->row.justify = JUSTIFY_FILL;
        const cJSON *item;
        cJSON_ArrayForEach(item, sub) {
            button_t *b = row_add(ctx, &e->row);
            if (b != NULL) {
                button_from_json(ctx, b, item, entity, false);
            }
        }
        return;
    }
    const char *parts[] = {"main", "bottom"};
    for (int p = 0; p < 2; p++) {
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(sub, parts[p]);
        const cJSON *entry;
        cJSON_ArrayForEach(entry, rows) {
            if (cJSON_IsObject(entry)) {
                add_sub_row(ctx, r, entry, entity);
            }
        }
    }
}

static void add_card(ctx_t *ctx, remote_t *r, const cJSON *card)
{
    const char *type = str(card, "type");
    if (type == NULL || strcmp(type, "custom:bubble-card") != 0) {
        logf_(ctx, "%s: card type %s not supported, skipped", r->title,
              type != NULL ? type : "(none)");
        return;
    }
    const char *ct = str(card, "card_type");
    if (ct == NULL) {
        ct = "(none)";
    }
    if (strcmp(ct, "separator") == 0) {
        add_separator(ctx, r, card);
    } else if (strcmp(ct, "button") == 0) {
        add_button_card(ctx, r, card);
    } else if (strcmp(ct, "media-player") == 0) {
        add_media_player(ctx, r, card);
    } else if (strcmp(ct, "sub-buttons") == 0) {
        add_sub_buttons(ctx, r, card);
    } else {
        logf_(ctx, "%s: bubble-card %s not supported, skipped", r->title, ct);
    }
}

/* ---- limits ---- */

static void apply_button_limit(ctx_t *ctx, remote_t *r)
{
    int total = 0, kept = 0;
    for (int i = 0; i < r->count; i++) {
        button_row_t *row = &r->elements[i].row;
        int keep = LAYOUT_MAX_BUTTONS - kept;
        if (keep < 0) {
            keep = 0;
        }
        total += row->count;
        if (row->count > keep) {
            for (int k = keep; k < row->count; k++) {
                button_free(&row->buttons[k]);
            }
            row->count = keep;
        }
        kept += row->count;
    }
    /* Drop rows that ended up empty because of the limit. */
    int w = 0;
    for (int i = 0; i < r->count; i++) {
        element_t *e = &r->elements[i];
        if (e->kind == ELEMENT_ROW && e->row.count == 0) {
            element_free(e);
            continue;
        }
        r->elements[w++] = *e;
    }
    r->count = w;
    r->button_count = kept;
    if (total > kept) {
        logf_(ctx, "%s: %d buttons, only the first %d are shown, %d left out", r->title, total,
              kept, total - kept);
    }
}

/* ---- entry points ---- */

bool layout_parse(const cJSON *config, layout_t *out, layout_log_fn log)
{
    memset(out, 0, sizeof *out);
    if (!cJSON_IsObject(config)) {
        return false;
    }
    ctx_t ctx = {.log = log};
    const cJSON *views = cJSON_GetObjectItemCaseSensitive(config, "views");
    int n = 0;
    const cJSON *view;
    cJSON_ArrayForEach(view, views) {
        n++;
        if (out->count == LAYOUT_MAX_REMOTES) {
            continue;
        }
        remote_t *r = &out->remotes[out->count++];
        const char *title = str(view, "title");
        if (title == NULL || title[0] == '\0') {
            title = str(view, "path");
        }
        char fallback[24]; /* "Remote " + any int */
        if (title == NULL || title[0] == '\0') {
            snprintf(fallback, sizeof fallback, "Remote %d", n);
            title = fallback;
        }
        r->title = dup_opt(&ctx, title);

        /* masonry, panel and sidebar views: cards[]; sections views:
         * sections[].cards[] */
        const cJSON *card;
        cJSON_ArrayForEach(card, cJSON_GetObjectItemCaseSensitive(view, "cards")) {
            add_card(&ctx, r, card);
        }
        const cJSON *section;
        cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(view, "sections")) {
            cJSON_ArrayForEach(card, cJSON_GetObjectItemCaseSensitive(section, "cards")) {
                add_card(&ctx, r, card);
            }
        }
        apply_button_limit(&ctx, r);
    }
    if (n > LAYOUT_MAX_REMOTES) {
        logf_(&ctx, "%d views, only the first %d are used as remotes", n, LAYOUT_MAX_REMOTES);
    }
    if (ctx.oom) {
        layout_free(out);
        return false;
    }
    return true;
}

void layout_free(layout_t *layout)
{
    if (layout == NULL) {
        return;
    }
    for (int i = 0; i < layout->count; i++) {
        remote_t *r = &layout->remotes[i];
        for (int k = 0; k < r->count; k++) {
            element_free(&r->elements[k]);
        }
        free(r->elements);
        free(r->title);
    }
    memset(layout, 0, sizeof *layout);
}
