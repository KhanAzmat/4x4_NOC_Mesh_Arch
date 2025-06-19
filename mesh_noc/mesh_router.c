#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "generated/mem_map.h"
#include "mesh_routing.h"
#include "noc_packet.h"
#include "platform_init/address_manager.h"

int noc_trace_enabled = 0;

void noc_send_packet(const noc_packet_t* pkt)
{
    int hops = 0;
    calc_xy_route(pkt->hdr.src_x, pkt->hdr.src_y,
                  pkt->hdr.dest_x, pkt->hdr.dest_y, &hops);

    if (noc_trace_enabled) {
        printf("[TRACE] Packet %u→%u (%d hops) type %d len %u\n",
               pkt->hdr.src_y*4+pkt->hdr.src_x,
               pkt->hdr.dest_y*4+pkt->hdr.dest_x,
               hops, pkt->hdr.type, pkt->hdr.length);
    }

    if (pkt->hdr.type == PKT_DMA_TRANSFER && pkt->hdr.src_addr && pkt->hdr.dst_addr) {
        uint8_t* src_ptr = addr_to_ptr(pkt->hdr.src_addr);
        uint8_t* dst_ptr = addr_to_ptr(pkt->hdr.dst_addr);
        
        if (src_ptr && dst_ptr && pkt->hdr.length > 0) {
            memcpy(dst_ptr, src_ptr, pkt->hdr.length);
            
            if (noc_trace_enabled) {
                printf("[TRACE] Data transfer: 0x%lx -> 0x%lx (%u bytes)\n",
                       pkt->hdr.src_addr, pkt->hdr.dst_addr, pkt->hdr.length);
            }
        }
    }
}