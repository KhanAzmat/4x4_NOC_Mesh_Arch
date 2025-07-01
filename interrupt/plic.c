#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
 
#include "plic.h"
#include "platform_init/address_manager.h"
 
#define SIZE 0x800000
 
#define CV(x, y) (((uint64_t)(x)) - ((uint64_t)(y)))


uint32_t current_hart_id = 0; 

// static const uint8_t PLIC_TARGET_BASE[3]  = { 0,  8, 16 };   /* plic0,1,2 */
// static const uint8_t PLIC_TARGET_COUNT[3] = { 8,  0,  0 };  /* 8, 0, 0 */
static const uintptr_t plic_base_tbl[3][2] = {
    { PLIC_0_C0C1_BASE, PLIC_0_NXY_BASE },
    { PLIC_1_C0C1_BASE, PLIC_1_NXY_BASE },
    { PLIC_2_C0C1_BASE, PLIC_2_NXY_BASE },
};

volatile PLIC_RegDef *PLIC_INST[3] = { NULL, NULL, NULL };

int PLIC_version(PLIC_RegDef *obj) {
    if (!obj)
        return -1;
    int ver = obj->ver_max_prio & 0x0000FFFF;
    return ver;
}
 
int PLIC_max_prio(PLIC_RegDef *obj) {
    if (!obj)
        return -1;
    int max = (obj->ver_max_prio & 0xFFFF0000) >> 16;
    return max;
}
 
int PLIC_num_tar(PLIC_RegDef *obj) {
    if (!obj)
        return -1;
    int max = (obj->num_tar_intp & 0xFFFF0000) >> 16;
    return max;
}
 
int PLIC_num_intr(PLIC_RegDef *obj) {
    if (!obj)
        return -1;
    int max = obj->num_tar_intp & 0x0000FFFF;
    return max;
}

void PLIC_init(volatile PLIC_RegDef **obj, uint8_t which) {
    switch (which) {
        case 0:
            *obj = (PLIC_RegDef*)PLIC_0_C0C1_BASE;
            break;
        case 1:
            *obj = (PLIC_RegDef*)PLIC_0_NXY_BASE;
            break;
        default:
            *obj = NULL;
            break;
    }
}
 
void PLIC_clear(PLIC_RegDef *obj) {
    memset(obj, 0, sizeof(PLIC_RegDef));
}
 
void PLIC_feature_set(PLIC_RegDef *obj, enum PLIC_FEATURE_TYPE type) {
    uint32_t mask = 0x1;
    mask <<= type;
    obj->feature_enable_reg |= mask;
}
 
void PLIC_feature_clear(PLIC_RegDef *obj, enum PLIC_FEATURE_TYPE type) {
    uint32_t mask = 0x1;
    mask <<= type;
    obj->feature_enable_reg &= (!mask);
}
 
int PLIC_N_priority_set(PLIC_RegDef *obj, uint32_t source, uint8_t priority) {
    if (source <= 0 || source > 1023) {
        return -1;
    }
    obj->sprio_regs[source-1] |= priority;
    return 1;
}
 
int PLIC_N_priority_clear(PLIC_RegDef *obj, uint32_t source, uint8_t priority) {
    if (source <= 0 || source > 1023) {
        return -1;
    }
    obj->sprio_regs[source-1] = 0;
    return 1;
}
 
int PLIC_N_source_pending_read(PLIC_RegDef *obj, uint32_t source) {
    if (source <= 0 || source > 1023) {
        return -1;
    }
 
    uint32_t index, mask = 0x1;
    index = source / 32;
    mask <<= (source % 32);
    return (obj->pending_regs[index] & mask);
}
 
int PLIC_N_source_pending_write(PLIC_RegDef *obj, uint32_t source) {
    if (source <= 0 || source > 1023) {
        return -1;
    }
 
    uint32_t index, mask = 0x1;
    index = source / 32;
    mask <<= (source % 32);
    obj->pending_regs[index] = mask;
    // asm volatile("fence");
    return 1;
}
 
int PLIC_N_source_tri_type_read(PLIC_RegDef *obj, uint32_t source) {
    if (source <= 0 || source > 1023) {
        return -1;
    }
 
    uint32_t index, mask = 0x1;
    index = source / 32;
    mask <<= (source % 32);
    return (obj->trigger_regs[index] & mask);
}
 
int PLIC_N_source_tri_type_write(PLIC_RegDef *obj, uint32_t source) {
    if (source <= 0 || source > 1023) {
        return -1;
    }
 
    uint32_t index, mask = 0x1;
    index = source / 32;
    mask <<= (source % 32);
    obj->trigger_regs[index] |= mask;
    return 1;
}
 
int PLIC_M_TAR_enable(PLIC_RegDef *obj, uint32_t target, uint32_t source) {
    if (target > 15 || source <= 0 || source > 1023)
        return -1;
    int index, mask = 0x1;
    index = source / 32;
    mask <<= source % 32;
    obj->teregs[target].regs[index] |= mask;
    return 1;
}

int PLIC_M_TAR_read(PLIC_RegDef *obj, uint32_t target, uint32_t source) {
    if (target > 15 || source <= 0 || source > 1023)
        return -1;
    int index, mask = 0x1;
    index = source / 32;
    mask <<= source % 32;
    return obj->teregs[target].regs[index] & mask;
}


int PLIC_M_TAR_disable(PLIC_RegDef *obj, uint32_t target, uint32_t source) {
    if (target > 15 || source <= 0 || source > 1023)
        return -1;
    int index, mask = 0x1;
    index = source / 32;
    mask <<= source % 32;
    obj->teregs[target].regs[index] &= (!mask);
    return 1;
}
 
int PLIC_M_TAR_claim_read(PLIC_RegDef *obj, uint32_t target) {
    if (target > 15)
        return -1;
    
    // Get target's priority threshold
    uint32_t threshold = obj->tpcregs[target].tar_prio_thres & 0x0000FFFF;
    
    // Find highest priority pending + enabled interrupt
    uint32_t best_source = 0;
    uint32_t best_priority = 0;
    
    for (uint32_t source = 1; source <= 1023; source++) {
        // Check if source is pending
        uint32_t pend_index = source / 32;
        uint32_t pend_mask = 1U << (source % 32);
        if (!(obj->pending_regs[pend_index] & pend_mask)) {
            continue;  // Not pending
        }
        
        // Check if target is enabled for this source
        uint32_t enable_index = source / 32;
        uint32_t enable_mask = 1U << (source % 32);
        if (!(obj->teregs[target].regs[enable_index] & enable_mask)) {
            continue;  // Not enabled for this target
        }
        
        // Get source priority
        uint32_t priority = obj->sprio_regs[source-1] & 0xFF;
        
        // Check if priority > threshold and higher than current best
        if (priority > threshold && priority > best_priority) {
            best_source = source;
            best_priority = priority;
        }
    }
    
    // If found a valid interrupt, clear its pending bit and return it
    if (best_source > 0) {
        uint32_t pend_index = best_source / 32;
        uint32_t pend_mask = 1U << (best_source % 32);
        obj->pending_regs[pend_index] &= ~pend_mask;  // Clear pending bit
        
        // Store in claim register for completion
        obj->tpcregs[target].tar_claim_comp = best_source;
    }
    
    return best_source;
}
 
int PLIC_M_TAR_comp_write(PLIC_RegDef *obj, uint32_t target, uint32_t interrupt_id) {
    if (target > 15)
        return -1;
    uint32_t mask = 0x0000FFFF;
    obj->tpcregs[target].tar_claim_comp = interrupt_id;
    // asm volatile("fence");
    return 1;
}

int PLIC_M_TAR_thre_write(PLIC_RegDef *obj, uint8_t tar, uint32_t thres) {
    if (tar < 0 || tar > 15)
        return -1;
    obj->tpcregs[tar].tar_prio_thres = (thres & 0x0000FFFF);
    return 1;
}
 
int PLIC_M_TAR_thre_read(PLIC_RegDef *obj, uint8_t tar) {
    if (tar < 0 || tar > 15)
        return -1;
    int ret = obj->tpcregs[tar].tar_prio_thres & 0x0000FFFF;
    return ret;
}

// Legacy PLIC_trigger_interrupt function - moved to end of file with new implementation

void plic_init_for_this_hart(uint32_t hartid)
{
    uint32_t col = (hartid < 2) ? 0 : 1;
    uint32_t plic_idx = col;  // Use col as PLIC instance index: 0=C0C1, 1=NXY

    // Use address manager to get actual mapped memory instead of raw addresses
    uint8_t* mapped_memory = addr_to_ptr(plic_base_tbl[0][col]);
    
    if (mapped_memory) {
        PLIC_INST[plic_idx] = (volatile PLIC_RegDef*)mapped_memory;
        printf("[PLIC] Hart %d: Using PLIC_INST[%d] = %p (col=%d)\n", hartid, plic_idx, mapped_memory, col);
    } else {
        printf("[PLIC] Hart %d: WARNING - No mapped memory for PLIC address 0x%lx\n", 
               hartid, plic_base_tbl[0][col]);
        PLIC_INST[plic_idx] = NULL;  // Safe fallback
    }
}

void plic_select(uint32_t hartid, volatile PLIC_RegDef **out_plic, uint32_t *out_tgt_local)
{
    uint32_t plic_idx, tgt_local;

    if (hartid < 8) {           /* hart0,1,2,3,4,5,6,7 ? plic0 */
        // Select PLIC instance based on hart: 0,1->C0C1(idx=0), 2-7->NXY(idx=1)
        plic_idx  = (hartid < 2) ? 0 : 1;
        tgt_local = hartid;
    } else {
        /* Invalid hart ID for your platform */
        *out_plic = NULL;
        *out_tgt_local = 0;
        return;
    }

    *out_plic      = PLIC_INST[plic_idx];
    *out_tgt_local = tgt_local;
}


void PLIC_enable_interrupt(irq_source_id_t irq_id, uint32_t hart_id){

    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;

    plic_select(hart_id, &plic, &tgt_local);    
    
    printf("[PLIC_enable_interrupt] Hart %d: enabling source %d on PLIC %p, target_local %d\n",
           hart_id, irq_id, plic, tgt_local);

    PLIC_M_TAR_enable(plic, tgt_local, irq_id); 
}


void PLIC_set_priority(irq_source_id_t irq_id, uint32_t hart_id, uint32_t prior){
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;

    plic_select(hart_id, &plic, &tgt_local);

    PLIC_N_priority_set(plic, irq_id, prior);
}


void PLIC_set_threshold(uint32_t hart_id, uint32_t threshold){
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;

    plic_select(hart_id, &plic, &tgt_local);

    PLIC_M_TAR_thre_write(plic, tgt_local, threshold);        

}

// Enhanced PLIC functions for bidirectional communication

/**
 * Calculate a unique source ID for a given source->target interrupt with specific type
 * Formula: SOURCE_BASE_ID + (source_hart * NUM_IRQ_TYPES) + irq_type_offset
 */
uint32_t PLIC_calculate_source_id(uint32_t source_hart, uint32_t target_hart, irq_source_id_t irq_type) {
    // Use a more systematic approach for source ID allocation
    // Reserve ranges: 32-63 for hart 0, 64-95 for hart 1, etc.
    uint32_t hart_base = SOURCE_BASE_ID + (source_hart * 32);
    uint32_t type_offset = (uint32_t)irq_type;
    
    // Ensure we don't exceed PLIC source limits (1023 max)
    uint32_t source_id = hart_base + type_offset;
    if (source_id > 1023) {
        printf("[PLIC] WARNING: Source ID %d exceeds PLIC limit\n", source_id);
        return 0; // Invalid source
    }
    
    return source_id;
}

/**
 * Setup bidirectional interrupt capabilities for all harts
 */
int PLIC_setup_bidirectional_interrupts(void) {
    printf("[PLIC] Setting up bidirectional interrupt support...\n");
    
    // Enhanced interrupt types to support
    irq_source_id_t supported_types[] = {
        IRQ_MESH_NODE,      // Legacy compatibility
        IRQ_TASK_COMPLETE,
        IRQ_TASK_ASSIGN,
        IRQ_ERROR_REPORT,
        IRQ_DMA_COMPLETE,
        IRQ_SYNC_REQUEST,
        IRQ_SYNC_RESPONSE,
        IRQ_SHUTDOWN_REQUEST
    };
    int num_types = sizeof(supported_types) / sizeof(supported_types[0]);
    
    // Configure each hart to handle interrupts from all other harts
    for (uint32_t target_hart = 0; target_hart < NR_HARTS; target_hart++) {
        printf("[PLIC] Configuring hart %d interrupt capabilities...\n", target_hart);
        
        // Set threshold (same for all)
        PLIC_set_threshold(target_hart, 1);
        
        for (uint32_t source_hart = 0; source_hart < NR_HARTS; source_hart++) {
            if (source_hart == target_hart) continue; // No self-interrupts
            
            // Enable each interrupt type from each potential source
            for (int type_idx = 0; type_idx < num_types; type_idx++) {
                irq_source_id_t irq_type = supported_types[type_idx];
                uint32_t source_id = PLIC_calculate_source_id(source_hart, target_hart, irq_type);
                
                if (source_id > 0) {
                    // Enable this source for this target
                    PLIC_enable_interrupt((irq_source_id_t)source_id, target_hart);
                    
                    // Set priority (different priorities for different types)
                    uint32_t priority = 2; // Default priority
                    switch (irq_type) {
                        case IRQ_ERROR_REPORT:
                        case IRQ_SHUTDOWN_REQUEST:
                            priority = 7; // High priority
                            break;
                        case IRQ_TASK_ASSIGN:
                        case IRQ_SYNC_REQUEST:
                            priority = 5; // Medium-high priority
                            break;
                        case IRQ_TASK_COMPLETE:
                        case IRQ_DMA_COMPLETE:
                            priority = 3; // Medium priority
                            break;
                        default:
                            priority = 2; // Normal priority
                            break;
                    }
                    
                    PLIC_set_priority((irq_source_id_t)source_id, target_hart, priority);
                    
                    printf("[PLIC] Hart %d: enabled source %d (hart %d -> type %d) priority %d\n",
                           target_hart, source_id, source_hart, (int)irq_type, priority);
                }
            }
        }
    }
    
    printf("[PLIC] Bidirectional interrupt setup complete\n");
    return 1;
}

/**
 * Trigger a typed interrupt from source hart to target hart
 */
int PLIC_trigger_typed_interrupt(uint32_t source_hart, uint32_t target_hart, irq_source_id_t irq_type) {
    if (source_hart >= NR_HARTS || target_hart >= NR_HARTS) {
        printf("[PLIC] Invalid hart IDs: source %d, target %d\n", source_hart, target_hart);
        return -1;
    }
    
    if (source_hart == target_hart) {
        printf("[PLIC] Self-interrupts not supported\n");
        return -2;
    }
    
    // Calculate the unique source ID for this source->target interrupt type
    uint32_t source_id = PLIC_calculate_source_id(source_hart, target_hart, irq_type);
    if (source_id == 0) {
        printf("[PLIC] Failed to calculate valid source ID\n");
        return -3;
    }
    
    // Find which PLIC instance to use based on target hart
    int plic_index = -1;
    uint32_t target_local_idx = 0;
    
    for (int i = 0; i < 1; ++i) {
        uint32_t base = PLIC_TARGET_BASE[i];
        uint32_t count = PLIC_TARGET_COUNT[i];
        if (target_hart >= base && target_hart < base + count) {
            plic_index = i;
            target_local_idx = target_hart - base;
            break;
        }
    }
    
    if (plic_index < 0) {
        printf("[PLIC] No PLIC instance found for target hart %d\n", target_hart);
        return -4;
    }
    
    // Select the correct PLIC instance based on target hart
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(target_hart, &plic, &tgt_local);
    
    if (!plic) {
        printf("[PLIC] No valid PLIC instance for target hart %d\n", target_hart);
        return -5;
    }
    
    printf("[PLIC] Triggering: hart %d -> hart %d, type %d, source_id %d\n",
           source_hart, target_hart, (int)irq_type, source_id);
    
    return PLIC_N_source_pending_write((PLIC_RegDef*)plic, source_id);
}

// Legacy function - now implemented using the enhanced system
int PLIC_trigger_interrupt(uint32_t source_hart_id, uint32_t target_hartid) {
    // Use the legacy IRQ_MESH_NODE type for backward compatibility
    return PLIC_trigger_typed_interrupt(source_hart_id, target_hartid, IRQ_MESH_NODE);
}