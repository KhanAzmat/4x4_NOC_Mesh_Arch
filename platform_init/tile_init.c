#include <stdio.h>
#include "c0_master/c0_controller.h"

void platform_init_tiles(tile_core_t* tiles, int count)
{
    for (int i = 0; i < count; ++i) {
        tiles[i].id = i;
        tiles[i].x  = (i % 4);
        tiles[i].y  = (i / 4) * 2; /* nodes on rows 0 and 2 */
        printf("Init Node%d at (%d,%d)\n", i, tiles[i].x, tiles[i].y);
    }
}
