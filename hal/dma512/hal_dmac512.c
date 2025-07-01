/**
  ******************************************************************************
  * @file    hal_dma512.c
  * @author  Lakshmikanth
  * @version V1.0.0
  * @date    20-March-2025
  * @brief   System HAL driver module header for DMAC512 driver protottypes. 
  * 	     This file provides register offsets and bit descriptions for DMAC512
  * 	     module. Also defines macro level enable disable and other utility 
  * 	     functions
  *
  ******************************************************************************
  * @warning
  * 
  * Limitations:
  * 1) Current implementation is using generic define labels whenever that particular field is not yet documented.  
  *
  ******************************************************************************
  * @copyright
  * © 2025 EinsNXT. All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */ 

#include "hal_dmac512.h"
#include "../platform_init/address_manager.h"
#include "../generated/mem_map.h"
#include <string.h>

/** @defgroup DMAC512 HAL driver module
 *  @brief This module defines register offsets, Bit definitions and utility macros
 *  @{
 */

/**
 * @brief Initializes handle with DMAC512 base address
 *	  Every node has a DMAC512 instance. So the base address 
 *	  is different from N00 to Nxy nodes. A  top level macro
 *	  from runtime.mk decides which base address to be assigned.
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return None
 */
void HAL_DMAC512InitHandle(DMAC512_HandleTypeDef *dmac512_handle, DMAC512_RegDef *address)
{
  dmac512_handle->Instance = address ;	
}

/**
 * @brief Configures a DMAC512  for transfers
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return -1 if handle an other parameters are not initialized Properly.
 * 	        0 upon successful configuration. 
 */
int32_t HAL_DMAC512ConfigureChannel(DMAC512_HandleTypeDef *dmac512_handle)
{
	/* @todo Return specific error codes instead of -1. */
	if(dmac512_handle == NULL) {
		return -1; 
	}
		
  // Set DMAC512 dob beat 
  SET_DMAC512_CTRL_DOB_B(dmac512_handle->Instance->DMAC_CONTROL,dmac512_handle->Init.dob_beat);
	
  // Set DMAC512 dfb beat  
  SET_DMAC512_CTRL_DFB_B(dmac512_handle->Instance->DMAC_CONTROL,dmac512_handle->Init.dfb_beat);

	// Set DMAC512 DMAC mode 
	SET_DMAC512_CTRL_MODE(dmac512_handle->Instance->DMAC_CONTROL,dmac512_handle->Init.DmacMode); 
	
	// Set DMAC512 Source address
	SET_DMAC512_SRC_ADDR(dmac512_handle->Instance->DMAC_SRC_ADDR,dmac512_handle->Init.SrcAddr); 
	
	// Set DMAC512 Destination address
	SET_DMAC512_DST_ADDR(dmac512_handle->Instance->DMAC_DST_ADDR,dmac512_handle->Init.DstAddr); 
	
	// Set DMAC512 Transfer count value
	SET_DMAC512_TOTAL_XFER_CNT(dmac512_handle->Instance->DMAC_TOTAL_XFER_CNT,dmac512_handle->Init.XferCount); 
 
	return 0;  // Success
}

/**
 * @brief Starts DMAC512  transfers
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return None
 */
void HAL_DMAC512StartTransfers(DMAC512_HandleTypeDef *dmac512_handle)
{
	/*Enable DMAC512 Transfers */
	SET_DMAC512_DMAC_EN(dmac512_handle->Instance->DMAC_TOTAL_XFER_CNT,DMAC512_ENABLE_TRANSFERS);
	
	// For software simulation: Perform the actual data transfer
	// In real hardware, the DMA engine would handle this autonomously
	uint64_t src_addr = dmac512_handle->Init.SrcAddr;
	uint64_t dst_addr = dmac512_handle->Init.DstAddr;
	uint32_t size = dmac512_handle->Init.XferCount;
	
	// Translate addresses to pointers for simulation
	uint8_t* src_ptr = addr_to_ptr(src_addr);
	uint8_t* dst_ptr = addr_to_ptr(dst_addr);
	
	if (src_ptr && dst_ptr && validate_address(src_addr, size) && validate_address(dst_addr, size)) {
		// Simulate DMA transfer
		memcpy(dst_ptr, src_ptr, size);
		
		// Mark transfer as complete by clearing busy bit
		dmac512_handle->Instance->DMAC_STATUS &= ~DMAC512_STATUS_DMAC_BUSY_MASK;
		
		// Set interrupt flag if transfer completes
		dmac512_handle->Instance->DMAC_INTR |= DMAC512_INTR_DMAC_INTR_MASK;
	} else {
		// Transfer failed - set busy bit to indicate error state
		dmac512_handle->Instance->DMAC_STATUS |= DMAC512_STATUS_DMAC_BUSY_MASK;
	}
}

/**
 * @brief Checks if  DMAC512  transfers are done
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return Status of dma_is_busy bit of status register
 */
bool HAL_DMAC512IsBusy(DMAC512_HandleTypeDef *dmac512_handle)
{
	/*Check if dma is busy and return status */
	return GET_DMAC512_STATUS_DMAC_BUSY(dmac512_handle->Instance->DMAC_STATUS);
}

/**
 * @brief Initializes and configures a DMAC512 handle for a tile
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[in] tile_id Tile ID (used to get DMA register base address).
 * @param[out] None.
 * @return 0 on success, -1 on failure
 */
int HAL_DMAC512InitTile(DMAC512_HandleTypeDef *dmac512_handle, int tile_id)
{
	if (!dmac512_handle || tile_id < 0 || tile_id >= 8) {
		return -1;
	}
	
	// Get DMA register base address for this tile
	uint64_t dma_reg_bases[] = {
		TILE0_DMA_REG_BASE, TILE1_DMA_REG_BASE, TILE2_DMA_REG_BASE, TILE3_DMA_REG_BASE,
		TILE4_DMA_REG_BASE, TILE5_DMA_REG_BASE, TILE6_DMA_REG_BASE, TILE7_DMA_REG_BASE
	};
	
	uint64_t dma_base = dma_reg_bases[tile_id];
	DMAC512_RegDef* dma_regs = (DMAC512_RegDef*)addr_to_ptr(dma_base);
	
	if (!dma_regs) {
		return -1;
	}
	
	// Initialize handle with register base
	HAL_DMAC512InitHandle(dmac512_handle, dma_regs);
	
	// Set default configuration
	dmac512_handle->Init.dob_beat = DMAC512_AXI_TRANS_8;
	dmac512_handle->Init.dfb_beat = DMAC512_AXI_TRANS_8; 
	dmac512_handle->Init.DmacMode = DMAC512_NORMAL_MODE;
	dmac512_handle->Init.SrcAddr = 0;
	dmac512_handle->Init.DstAddr = 0;
	dmac512_handle->Init.XferCount = 0;
	
	// Reset DMA controller
	SET_DMAC512_CTRL_RST(dmac512_handle->Instance->DMAC_CONTROL, 1);
	SET_DMAC512_CTRL_RST(dmac512_handle->Instance->DMAC_CONTROL, 0);
	
	// Clear status and interrupt flags
	dmac512_handle->Instance->DMAC_STATUS = 0;
	dmac512_handle->Instance->DMAC_INTR = 0;
	
	return 0;
}

/**
 * @brief Performs a complete DMA transfer using DMAC512
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[in] src_addr Source address for transfer.
 * @param[in] dst_addr Destination address for transfer.
 * @param[in] size Transfer size in bytes.
 * @param[out] None.
 * @return Transfer size on success, -1 on failure
 */
int HAL_DMAC512Transfer(DMAC512_HandleTypeDef *dmac512_handle, 
                       uint64_t src_addr, uint64_t dst_addr, size_t size)
{
	if (!dmac512_handle || size == 0 || size > 0xFFFFFF) {  // 24-bit max size
		return -1;
	}
	
	// Validate addresses
	if (!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
		return -1;
	}
	
	// Configure transfer parameters
	dmac512_handle->Init.SrcAddr = src_addr;
	dmac512_handle->Init.DstAddr = dst_addr;
	dmac512_handle->Init.XferCount = (uint32_t)size;
	
	// Configure and start transfer
	int32_t config_result = HAL_DMAC512ConfigureChannel(dmac512_handle);
	if (config_result != 0) {
		return -1;
	}
	
	// Mark as busy before starting transfer
	dmac512_handle->Instance->DMAC_STATUS |= DMAC512_STATUS_DMAC_BUSY_MASK;
	
	// Start transfer (this performs the actual copy in simulation)
	HAL_DMAC512StartTransfers(dmac512_handle);
	
	// In simulation, transfer completes immediately
	// In real hardware, would need to wait for completion
	return (int)size;
}

/** @} */ // End of Driver DMAC512 group
