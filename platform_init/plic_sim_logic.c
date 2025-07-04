#include "plic.h"
#undef PLIC_SIM_DEBUG
#include <pthread.h>
#include <stdint.h>

/*
 * Software model of PLIC arbitration / claim-generation that runs
 * immediately AFTER the real HAL functions execute.  The linker wraps
 * PLIC_N_source_pending_write() and PLIC_M_TAR_comp_write() so that
 * all existing call-sites stay untouched.
 */

static pthread_mutex_t g_plic_fabric_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- helper: choose highest-priority deliverable IRQ ---------- */
static uint32_t pick_best_irq(volatile PLIC_RegDef *p, int tgt)
{
    /* Current threshold for this target */
    uint8_t threshold = (uint8_t)(p->tpcregs[tgt].tar_prio_thres & 0xFF);

    uint32_t best_id  = 0;   /* 0 = none deliverable */
    uint8_t  best_pri = 0;

    for (uint32_t id = 1; id <= 1023; ++id) {
        uint32_t word = id >> 5;          /* /32  */
        uint32_t bit  = (uint32_t)1u << (id & 31);

        /* Only the lower 16 bits of each 32-bit pending word are visible
         * to the HAL (see mask in PLIC_M_TAR_claim_read).  Skip all other
         * bits so our arbitration matches the driver exactly. */
        /* keep all 32 pending bits; no 16-bit mask */

        if (!(p->pending_regs[word] & bit))
            continue;
        /* Enabled for this target? */
        if (!(p->teregs[tgt].regs[word] & bit))
            continue;

        uint8_t pri = (uint8_t)(p->sprio_regs[id - 1] & 0x7);
        if (pri < threshold)
            continue;
        if (pri < best_pri)
            continue;
        if (pri == best_pri && best_pri != 0) {
            /* tie – prefer smaller interrupt ID (RISC-V PLIC spec) */
            if (best_id != 0 && id > best_id)
                continue;
        }

        best_pri = pri;
        best_id  = id;
    }
    return best_id;
}

/* Recalculate claim registers for every target in one PLIC instance */
static void fabric_update_instance(volatile PLIC_RegDef *p)
{
    for (int tgt = 0; tgt < N_TARGET_EN; ++tgt) {
        if (p->tpcregs[tgt].tar_claim_comp == 0) {
            uint32_t new_id = pick_best_irq(p, tgt);
            p->tpcregs[tgt].tar_claim_comp = new_id;
        }
    }
}

/* ---------------------------------------------------------------
 *  WRAPPER 1  – pending-bit write
 * ------------------------------------------------------------- */
int __real_PLIC_N_source_pending_write(PLIC_RegDef *obj, uint32_t source);
int __wrap_PLIC_N_source_pending_write(PLIC_RegDef *obj, uint32_t source)
{
    if (source == 0 || source > 1023)
        return __real_PLIC_N_source_pending_write(obj, source);

    /* Word / bit that represents this source */
    uint32_t word = source >> 5;
    uint32_t bit  = (uint32_t)1u << (source & 31);

    /* Keep a copy of the word *before* the HAL overwrites it */
    uint32_t old_word = obj->pending_regs[word];

    /* Call the genuine HAL first (destructive write, clears other bits) */
    int rc = __real_PLIC_N_source_pending_write(obj, source);

    if (rc == 1) {
        pthread_mutex_lock(&g_plic_fabric_lock);
            /* Restore previous bits and add the new one */
            obj->pending_regs[word] = old_word | bit;

            fabric_update_instance(obj);
        pthread_mutex_unlock(&g_plic_fabric_lock);
    }
    return rc;
}

/* ---------------------------------------------------------------
 *  WRAPPER 2  – end-of-interrupt / completion write
 * ------------------------------------------------------------- */
int __real_PLIC_M_TAR_comp_write(PLIC_RegDef *obj, uint32_t target, uint32_t interrupt_id);
int __wrap_PLIC_M_TAR_comp_write(PLIC_RegDef *obj, uint32_t target, uint32_t interrupt_id)
{
    int rc = __real_PLIC_M_TAR_comp_write(obj, target, interrupt_id);

    if (rc == 1 && interrupt_id != 0 && target < N_TARGET_EN) {
        uint32_t word = interrupt_id >> 5;
        uint32_t bit  = (uint32_t)1u << (interrupt_id & 31);

        pthread_mutex_lock(&g_plic_fabric_lock);
            /* Clear the serviced pending bit */
            obj->pending_regs[word] &= ~bit;
            /* Clear claim register so fabric can refill */
            obj->tpcregs[target].tar_claim_comp = 0;
            fabric_update_instance(obj);
        pthread_mutex_unlock(&g_plic_fabric_lock);
    }
    return rc;
}

/* ---------------------------------------------------------------
 *  WRAPPER 3  – claim read
 * ------------------------------------------------------------- */
int __real_PLIC_M_TAR_claim_read(PLIC_RegDef *obj, uint32_t target);
int __wrap_PLIC_M_TAR_claim_read(PLIC_RegDef *obj, uint32_t target)
{
    if (target >= N_TARGET_EN)
        return __real_PLIC_M_TAR_claim_read(obj, target);

    uint32_t claimed_id;

    /* ----------- prepare value to be read ------------- */
    pthread_mutex_lock(&g_plic_fabric_lock);

        uint32_t best_id = pick_best_irq(obj, target);
        obj->tpcregs[target].tar_claim_comp = best_id;

    pthread_mutex_unlock(&g_plic_fabric_lock);

    /* ------------ perform the actual read ------------- */
    claimed_id = __real_PLIC_M_TAR_claim_read(obj, target);

    /* --------------- post-read side-effects ------------ */
    if (claimed_id != 0) {
        uint32_t word = claimed_id >> 5;
        uint32_t bit  = (uint32_t)1u << (claimed_id & 31);

        pthread_mutex_lock(&g_plic_fabric_lock);
            /* Clear pending bit for this source */
            obj->pending_regs[word] &= ~bit;
            /* Clear claim register so next read will re-arbitrate */
            obj->tpcregs[target].tar_claim_comp = 0;
        pthread_mutex_unlock(&g_plic_fabric_lock);
    }

    return claimed_id;
} 