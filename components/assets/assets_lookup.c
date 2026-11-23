/* Lookups in the generated tables. Pure C, tested on the host. */

#include <string.h>

#include "assets.h"

int icon_index(const char *name)
{
    if (name == NULL) {
        return -1;
    }
    if (strncmp(name, "mdi:", 4) == 0) {
        name += 4;
    }
    int lo = 0, hi = icon_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(name, icon_table[mid].name);
        if (c == 0) {
            return mid;
        }
        if (c < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return -1;
}

const char *tz_posix_for(const char *iana)
{
    if (iana == NULL) {
        return NULL;
    }
    int lo = 0, hi = tz_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(iana, tz_table[mid].iana);
        if (c == 0) {
            return tz_table[mid].posix;
        }
        if (c < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return NULL;
}
