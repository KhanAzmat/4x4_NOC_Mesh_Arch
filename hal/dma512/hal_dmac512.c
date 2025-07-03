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


/** @} */ // End of Driver DMAC512 group
