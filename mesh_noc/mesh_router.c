#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "generated/mem_map.h"
#include "mesh_routing.h"
#include "noc_packet.h"
#include "platform_init/address_manager.h"

// Include interrupt system headers for NoC interrupt packet handling
#include "../c0_master/c0_controller.h"

// External reference to platform context (defined in c0_controller.c)
extern mesh_platform_t* g_platform_context;

// Forward declarations for interrupt handling
void noc_handle_interrupt_packet(const noc_packet_t* pkt);
int parse_irq_packet(const noc_packet_t* pkt, interrupt_request_t* irq);

int noc_trace_enabled = 0;

// Simulate hardware NOC arbitration for simultaneous access
#define MAX_DESTINATIONS 16
static pthread_mutex_t destination_arbitration_locks[MAX_DESTINATIONS];
static int arbitration_counters[MAX_DESTINATIONS] = {0};  // Track access order
static bool noc_arbitration_initialized = false;

// Initialize NOC arbitration simulation (call once at startup)
void noc_init_arbitration(void) {
    if (!noc_arbitration_initialized) {
        for (int i = 0; i < MAX_DESTINATIONS; i++) {
            pthread_mutex_init(&destination_arbitration_locks[i], NULL);
            arbitration_counters[i] = 0;
        }
        noc_arbitration_initialized = true;
        printf("[NOC-INIT] Hardware arbitration simulation initialized\n");
    }
}

// Get destination lock index based on address
int get_destination_lock_index(uint64_t dst_addr) {
    // Map destination addresses to lock indices
    int dst_tile = get_tile_id_from_address(dst_addr);
    if (dst_tile >= 0 && dst_tile < 8) {
        return dst_tile;  // Tiles 0-7 use locks 0-7
    }
    
    // Check if it's a DMEM address
    int dmem_id = get_dmem_id_from_address(dst_addr);
    if (dmem_id >= 0 && dmem_id < 8) {
        return 8 + dmem_id;  // DMEM 0-7 use locks 8-15
    }
    
    return -1;  // No arbitration needed
}

int noc_send_packet(const noc_packet_t* pkt)
{
    // Initialize arbitration if not done yet
    if (!noc_arbitration_initialized) {
        noc_init_arbitration();
    }
    
    int hops = 0;
    calc_xy_route(pkt->hdr.src_x, pkt->hdr.src_y,
                  pkt->hdr.dest_x, pkt->hdr.dest_y, &hops);

    int src_node = pkt->hdr.src_y * 4 + pkt->hdr.src_x;
    int dst_node = pkt->hdr.dest_y * 4 + pkt->hdr.dest_x;
    
    // Simulate NOC hardware arbitration for destination access
    int lock_index = get_destination_lock_index(pkt->hdr.dst_addr);
    
    // Handle interrupt packets first (highest priority)
    if (pkt->hdr.type == PKT_INTERRUPT_REQ || pkt->hdr.type == PKT_INTERRUPT_ACK) {
        // Route interrupt packet to destination's interrupt handler
        noc_handle_interrupt_packet(pkt);
        return 0; // Success
    }
    
    if (pkt->hdr.type == PKT_DMA_TRANSFER && pkt->hdr.src_addr && pkt->hdr.dst_addr) {
        uint8_t* src_ptr = addr_to_ptr(pkt->hdr.src_addr);
        uint8_t* dst_ptr = addr_to_ptr(pkt->hdr.dst_addr);
        
        if (src_ptr && dst_ptr && pkt->hdr.length > 0) {
            if (lock_index >= 0) {
                // Simulate packet arriving at destination router
                printf("[NOC-PACKET] Node %d packet arrived at destination (addr 0x%lx)\n", 
                       src_node, pkt->hdr.dst_addr);
                
                // Hardware arbitration - first to acquire lock wins
                printf("[NOC-ARBITRATION] Node %d requesting arbitration for destination lock %d...\n", 
                       src_node, lock_index);
                
                struct timespec start_time, arbitration_time, end_time;
                clock_gettime(CLOCK_MONOTONIC, &start_time);
                
                pthread_mutex_lock(&destination_arbitration_locks[lock_index]);
                
                clock_gettime(CLOCK_MONOTONIC, &arbitration_time);
                int access_order = ++arbitration_counters[lock_index];
                
                printf("[NOC-ARBITRATION-WON] Node %d won arbitration for destination lock %d (access #%d)\n", 
                       src_node, lock_index, access_order);
                
                // Simulate hardware transfer time (proportional to data size)
                int transfer_time_us = (pkt->hdr.length * 10);  // 10us per byte
                printf("[NOC-TRANSFER] Node %d executing transfer (%u bytes, %d us)...\n",
                       src_node, pkt->hdr.length, transfer_time_us);
                
                usleep(transfer_time_us);
                
                // Perform the actual data transfer
                memcpy(dst_ptr, src_ptr, pkt->hdr.length);
                
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                
                // Calculate timing
                long wait_time_us = (arbitration_time.tv_sec - start_time.tv_sec) * 1000000 + 
                                   (arbitration_time.tv_nsec - start_time.tv_nsec) / 1000;
                long total_time_us = (end_time.tv_sec - start_time.tv_sec) * 1000000 + 
                                    (end_time.tv_nsec - start_time.tv_nsec) / 1000;
                
                printf("[NOC-COMPLETE] Node %d completed transfer (waited %ld us, total %ld us)\n",
                       src_node, wait_time_us, total_time_us);
                
                pthread_mutex_unlock(&destination_arbitration_locks[lock_index]);
                
                printf("[NOC-RELEASE] Node %d released destination lock %d\n", 
                       src_node, lock_index);
            } else {
                // No contention, direct transfer
                memcpy(dst_ptr, src_ptr, pkt->hdr.length);
            }
        }
    }
    
    return 0; // Success
}

// Handle interrupt packets routed through NoC
void noc_handle_interrupt_packet(const noc_packet_t* pkt) {
    if (!pkt || (pkt->hdr.type != PKT_INTERRUPT_REQ && pkt->hdr.type != PKT_INTERRUPT_ACK)) {
        printf("[NOC-IRQ-ERROR] Invalid interrupt packet\n");
        return;
    }
    
    int dst_entity = pkt->hdr.dest_y * 4 + pkt->hdr.dest_x;
    int src_entity = pkt->hdr.src_y * 4 + pkt->hdr.src_x;
    
    if (noc_trace_enabled) {
        printf("[NOC-IRQ] Routing interrupt packet from entity %d to entity %d\n",
               src_entity, dst_entity);
    }
    
    if (pkt->hdr.type == PKT_INTERRUPT_REQ) {
        // This is an interrupt request - extract and deliver to destination
        printf("[NOC-IRQ-REQ] Entity %d sent interrupt request to entity %d\n",
               src_entity, dst_entity);
        
        // Extract compact interrupt data from packet payload
        if (pkt->hdr.length == sizeof(compact_interrupt_packet_t) && pkt->hdr.length <= sizeof(pkt->payload)) {
            compact_interrupt_packet_t compact_irq;
            memcpy(&compact_irq, pkt->payload, sizeof(compact_interrupt_packet_t));
            
            // Convert compact format back to full interrupt request
            interrupt_request_t irq;
            memset(&irq, 0, sizeof(irq));
            
            irq.source_tile = (int)compact_irq.source_tile;
            irq.type = (interrupt_type_t)compact_irq.type;
            irq.priority = (interrupt_priority_t)compact_irq.priority;
            irq.timestamp = compact_irq.timestamp;
            irq.data = compact_irq.data;
            irq.valid = compact_irq.valid ? true : false;
            
            // Copy message (compact has smaller message field)
            strncpy(irq.message, compact_irq.message, sizeof(irq.message) - 1);
            irq.message[sizeof(irq.message) - 1] = '\0';
            
            // Deliver to destination entity
            if (dst_entity == 0 && g_platform_context) {
                // This is for C0 master - deliver to interrupt controller
                int result = noc_handle_received_interrupt(g_platform_context, &irq);
                if (result == 0) {
                    printf("[NOC-IRQ-DELIVERED] Successfully delivered %s interrupt from tile %d to C0\n",
                           get_irq_type_name(irq.type), src_entity);
                } else {
                    printf("[NOC-IRQ-DROPPED] Failed to deliver interrupt from tile %d to C0\n", src_entity);
                }
            } else {
                printf("[NOC-IRQ-UNSUPPORTED] Interrupt delivery to tile %d not yet supported\n", dst_entity);
            }
        } else {
            printf("[NOC-IRQ-ERROR] Invalid interrupt packet payload size (expected %zu, got %u)\n",
                   sizeof(compact_interrupt_packet_t), pkt->hdr.length);
        }
        
    } else if (pkt->hdr.type == PKT_INTERRUPT_ACK) {
        // This is an interrupt acknowledgment
        printf("[NOC-IRQ-ACK] Entity %d sent interrupt acknowledgment to entity %d\n",
               src_entity, dst_entity);
        // Could implement ACK handling here if needed
    }
    
    // Simulate minimal interrupt routing delay (much faster than data transfers)
    usleep(1); // 1 microsecond for interrupt routing
}