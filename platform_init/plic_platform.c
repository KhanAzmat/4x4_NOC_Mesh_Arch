#include <stdio.h>
#include <string.h>
#include "c0_master/c0_controller.h"
#include "plic.h"

#define MAX_TILES 8  // Platform has 8 tiles

// ============================================================================
// SIMPLE PLATFORM PLIC SUPPORT FUNCTIONS
// ============================================================================

int platform_init_plic_stats(mesh_platform_t* p) {
    if (!p) return -1;
    
    // Initialize statistics arrays to zero
    memset(&p->plic_stats, 0, sizeof(plic_interrupt_stats_t));
    
    printf("[Platform] Initialized PLIC interrupt statistics\n");
    return 0;
}

// These functions are provided for completeness but in the correct flow:
// Tests → HAL → Hardware, the HAL functions will trigger interrupts naturally

int platform_trigger_task_complete(mesh_platform_t* p, uint32_t source_hart_id, uint32_t task_id) {
    // In real flow: Tests call HAL functions, HAL completes and triggers interrupt
    // This function exists for manual testing/simulation if needed
    if (!p || source_hart_id >= MAX_TILES) return -1;
    
    (void)task_id; // Suppress unused parameter warning
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(source_hart_id, &plic, &tgt_local);
    return PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_MESH_NODE);
}

int platform_trigger_dma_complete(mesh_platform_t* p, uint32_t source_hart_id, uint32_t transfer_id) {
    // In real flow: HAL_DMAC512_Start() completes and triggers interrupt
    // This function exists for manual testing/simulation if needed
    if (!p || source_hart_id >= MAX_TILES) return -1;
    
    (void)transfer_id; // Suppress unused parameter warning
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(source_hart_id, &plic, &tgt_local);
    return PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_DMA512);
}

int platform_trigger_error(mesh_platform_t* p, uint32_t source_hart_id, uint32_t error_code) {
    // In real flow: HAL functions detect errors and trigger interrupt
    // This function exists for manual testing/simulation if needed
    if (!p || source_hart_id >= MAX_TILES) return -1;
    
    (void)error_code; // Suppress unused parameter warning
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(source_hart_id, &plic, &tgt_local);
    return PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_GPIO);
} 