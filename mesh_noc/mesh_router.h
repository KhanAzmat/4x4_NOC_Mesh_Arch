#ifndef MESH_ROUTER_H
#define MESH_ROUTER_H

#include "noc_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int noc_trace_enabled;

/* Send a single 512‑bit flit/packet – blocking in reference model */
void noc_send_packet(const noc_packet_t* pkt);

#ifdef __cplusplus
}
#endif
#endif
