#pragma once

#include "cJSON.h"
#include "ha.h"

typedef void (*media_changed_fn)(const ha_media_t *media, void *ctx);

/* Applies one subscribe_entities event to the players in list (matched by
 * entity_id). Calls changed() for every player that got a complete state, and
 * for every player whose state, title, app or picture changed. Updates of
 * other attributes, such as media_position_updated_at, are not reported. */
void media_state_apply(ha_media_t *list, int count, const cJSON *event, media_changed_fn changed,
                       void *ctx);
