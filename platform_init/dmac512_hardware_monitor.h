#ifndef DMAC512_HARDWARE_MONITOR_H
#define DMAC512_HARDWARE_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Include full type definitions
#include "c0_master/c0_controller.h"

/**
 * @brief Monitor DMA registers for a specific tile and execute transfers when ready
 * 
 * This function emulates DMA hardware behavior by checking if the HAL has written
 * enable=1 to the DMA control register. If so, it reads the transfer parameters
 * and performs the actual memory copy, then clears the enable bit and generates
 * a completion interrupt.
 * 
 * @param tile_id Tile ID (0-7) 
 * @param platform Platform structure containing tiles and memory
 * @return Number of transfers executed (0 or 1)
 */
int dmac512_monitor_tile_registers(int tile_id, mesh_platform_t* platform);

/**
 * @brief Synchronous DMA execution hook - called immediately when enable bit is written
 * 
 * This function executes DMA transfer immediately when HAL writes enable=1,
 * ensuring that HAL functions return only after hardware operation completes.
 */
int dmac512_execute_on_enable_write(int tile_id, mesh_platform_t* platform, DMAC512_RegDef* regs);

/**
 * @brief Hook function to be called whenever DMAC_TOTAL_XFER_CNT register is written
 * 
 * This provides immediate response to HAL register writes, making the hardware
 * emulation synchronous with HAL operations.
 */
void dmac512_on_register_write_hook(int tile_id, mesh_platform_t* platform, 
                                   uint32_t reg_offset, uint32_t new_value);

/**
 * @brief Perform memory copy using platform address translation
 * 
 * @param platform Platform structure
 * @param src_addr Source address in platform address space
 * @param dst_addr Destination address in platform address space  
 * @param size Number of bytes to copy
 * @return 0 on success, negative on error
 */
int platform_memory_copy(mesh_platform_t* platform, uint64_t src_addr, uint64_t dst_addr, size_t size);

/**
 * @brief Platform function to enable DMA with immediate execution
 * 
 * This function should be called by HAL instead of directly writing registers.
 * It performs the register write AND executes the transfer synchronously.
 */
void platform_dmac512_enable_and_execute(DMAC512_RegDef* regs, int enable_value);

/**
 * @brief Get tile ID from DMAC register pointer
 */
int platform_get_tile_id_from_dmac_regs(DMAC512_RegDef* regs);

// Using post-HAL monitoring approach instead of register interception
// HAL/Driver executes normally, we monitor register state after completion

#endif // DMAC512_HARDWARE_MONITOR_H 