#ifndef NOC_PACKET_H
#define NOC_PACKET_H
#include <stdint.h>
#include "generated/mem_map.h"

typedef enum {
    PKT_READ_REQ,
    PKT_READ_RESP,
    PKT_WRITE_REQ,
    PKT_WRITE_ACK,
    PKT_DMA_TRANSFER,
} pkt_type_t;

typedef struct {
    uint8_t dest_x, dest_y;
    uint8_t src_x,  src_y;
    pkt_type_t type;
    uint16_t length;          /* payload bytes (multiple of 64)   */
    uint8_t  hop_count;

    uint64_t src_addr;
    uint64_t dst_addr;
} pkt_header_t;

typedef struct {
    pkt_header_t hdr;
    uint8_t      payload[NOC_LINK_WIDTH_BITS/8];  /* one flit payload */
} noc_packet_t;

#endif
