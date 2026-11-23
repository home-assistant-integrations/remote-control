/* Access to the embedded icon and font files (EMBED_FILES). */

#include "assets.h"

extern const uint8_t icons_bin_start[] asm("_binary_icons_bin_start");
extern const uint8_t font_20_start[] asm("_binary_font_20_vlw_start");
extern const uint8_t font_28_start[] asm("_binary_font_28_vlw_start");
extern const uint8_t font_36_start[] asm("_binary_font_36_vlw_start");

const uint8_t *icon_find(const char *name, int size)
{
    int i = icon_index(name);
    if (i < 0 || (size != 32 && size != 48)) {
        return NULL;
    }
    return icons_bin_start + (size == 32 ? icon_table[i].offset32 : icon_table[i].offset48);
}

const uint8_t *font_20(void)
{
    return font_20_start;
}

const uint8_t *font_28(void)
{
    return font_28_start;
}

const uint8_t *font_36(void)
{
    return font_36_start;
}
