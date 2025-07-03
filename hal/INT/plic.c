#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
 
#include "plic.h"
 
#define SIZE 0x800000
 
#define CV(x, y) (((uint64_t)(x)) - ((uint64_t)(y)))

static const uint8_t PLIC_TARGET_BASE[3]  = { 0,  2, 18 };   /* plic0,1,2 */
static const uint8_t PLIC_TARGET_COUNT[3] = { 2, 16, 16 };  /* 2, 14, 14 */
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
    asm volatile("" ::: "memory"); /* fence -> memory barrier for simulation */
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
    uint32_t mask = 0x0000FFFF;
    int source_id = obj->tpcregs[target].tar_claim_comp;
    asm volatile("" ::: "memory"); /* fence -> memory barrier for simulation */
    return source_id;
}
 
int PLIC_M_TAR_comp_write(PLIC_RegDef *obj, uint32_t target, uint32_t interrupt_id) {
    if (target > 15)
        return -1;
    uint32_t mask = 0x0000FFFF;
    obj->tpcregs[target].tar_claim_comp = interrupt_id;
    asm volatile("" ::: "memory"); /* fence -> memory barrier for simulation */
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

int PLIC_trigger_interrupt(uint32_t source_hart_id, uint32_t target_hartid){
    if(source_hart_id >= NR_HARTS || target_hartid >= NR_HARTS)
        return -1;
    int plic_index = -1;
    uint32_t target_local_idx = 0;
    
    for(int i = 0; i < 3; ++i){
        uint32_t base = PLIC_TARGET_BASE[i];
        uint32_t count = PLIC_TARGET_COUNT[i];
        if(target_hartid >= base && target_hartid < base + count){
            plic_index = i;
            target_local_idx = target_hartid - base;
            break;
        }
    }

    if(plic_index < 0)
        return -2;

    uint32_t source_id = SOURCE_BASE_ID + target_local_idx * SLOT_PER_TARGET + source_hart_id;
    return PLIC_N_source_pending_write(PLIC_INST[plic_index], source_id);
}

void plic_init_for_this_hart(uint32_t hartid)
{
    uint32_t col = (hartid < 2) ? 0 : 1;

    for (uint32_t i = 0; i < 3; ++i)
        PLIC_INST[i] = (volatile PLIC_RegDef*)plic_base_tbl[i][col];

}

void plic_select(uint32_t hartid, volatile PLIC_RegDef **out_plic, uint32_t *out_tgt_local)
{
    uint32_t plic_idx, tgt_local;

    if (hartid < 2) {           /* hart0,1 ? plic0 */
        plic_idx  = 0;
        tgt_local = hartid;
    } else if (hartid < 18) {   /* hart2?15 ? plic1 */
        plic_idx  = 1;
        tgt_local = hartid - 2;
    } else {                    /* hart16?24 ? plic2 */
        plic_idx  = 2;
        tgt_local = hartid - 18;
    }

    *out_plic      = PLIC_INST[plic_idx];
    *out_tgt_local = tgt_local;
}


void PLIC_enable_interrupt(irq_source_id_t irq_id, uint32_t hart_id){

    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;

    plic_select(hart_id, &plic, &tgt_local);    

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