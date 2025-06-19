#ifndef MESH_ROUTING_H
#define MESH_ROUTING_H
#include <stdint.h>
#include <stdlib.h>
#include "generated/mem_map.h"

static inline void calc_xy_route(uint8_t src_x, uint8_t src_y,
                                 uint8_t dst_x, uint8_t dst_y,
                                 int* hops)
{
    *hops = abs(dst_x - src_x) + abs(dst_y - src_y);
}

#endif /* MESH_ROUTING_H */