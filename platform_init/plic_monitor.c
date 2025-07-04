#include "plic.h"
#include <stdbool.h>

/*
 * Thin monitor helpers that the HAL test-suite uses to peek at / drive
 * the PLIC.  They do NOT bypass the HAL – every operation goes through
 * the official HAL functions so the normal side-effects still happen.
 */

uint32_t plic_monitor_claim_interrupt(volatile PLIC_RegDef *plic, uint32_t target)
{
    /* The test passes the register base that it got from the HAL's
     * global PLIC_INST[] table, so we can cast away the volatile for the
     * call.  The call itself is routed through the linker-wrap so that
     * the simulation fabric can update the claim register first.
     */
    return PLIC_M_TAR_claim_read((PLIC_RegDef *)plic, target);
}

int plic_monitor_complete_interrupt(volatile PLIC_RegDef *plic,
                                    uint32_t            target,
                                    uint32_t            interrupt_id)
{
    /* Again, call the regular HAL helper – this ends up in the wrapped
     * implementation where the simulation fabric will clear the
     * corresponding pending bit and re-arbitrate.
     */
    return PLIC_M_TAR_comp_write((PLIC_RegDef *)plic, target, interrupt_id);
}

bool is_enabled(volatile PLIC_RegDef *plic, uint32_t source, uint32_t target)
{
    if (source == 0 || source > 1023 || target >= N_TARGET_EN)
        return false;

    uint32_t index = source / 32;
    uint32_t mask  = 1u << (source % 32);

    return (plic->teregs[target].regs[index] & mask) != 0;
} 