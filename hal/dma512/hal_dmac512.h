/**
  ******************************************************************************
  * @file    hal_dmac512.h
  * @author  Lakshmikanth
  * @version V1.0.0
  * @date    20-March-2025
  * 	     This file provides an abstraction layer declarations for DMAC512 module 
  * 	     allowing  configuruing and functioning without direct 
  * 	     BSP dependencies.
  *
  ******************************************************************************
  * @warning
  * 
  * Limitations:
  * 1) Current implementation is using generic define labels whenever that particular field is not yet documented.  
  *
  ******************************************************************************
  * @copyright
  * © 2025 EinsNeXT. All rights reserved.
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

#ifndef __HAL_DMAC512_H
#define __HAL_DMAC512_H

#include <stdbool.h>
#include "platform.h"
#include "rvv_dmac512.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup EinsNeXT HAL DMAC512 Module
 *  @brief 	  This module handles DMAC512 configuration,
 *  		      initialization and functionality
 *  		      of the module
 *  @{
 */


/**
 * @brief dfb_beat , dob_beat
 */
typedef enum {

    DMAC512_AXI_TRANS_2,
    DMAC512_AXI_TRANS_4,
    DMAC512_AXI_TRANS_8,
    DMAC512_AXI_TRANS_16,
    DMAC512_AXI_TRANS_32,
    DMAC512_AXI_TRANS_64

} DMAC512_DB_B_t;

/**
 * @brief DMAC512 Operation Mode
 */
typedef enum {

    DMAC512_NORMAL_MODE = 0,  			      /*!< normal transfer mode(default) */

} DMAC512_OP_MODE_t;

/**
 * @brief DMAC512 transfers Enable/Disable macros
 */
typedef enum {

    DMAC512_DISABLE_TRANSFERS = 0,  			/*!< Disable Transfers */
    DMAC512_ENABLE_TRANSFERS	       			/*!< Enable Transfers  */

} DMAC512_HandshakeMode_t;


/** 
 * @brief    	DMAC64 configuration structure. This structure holds the
 * 		        required configuration for the transaction to be issued 
 */
typedef struct{

    DMAC512_DB_B_t     dob_beat;   /*!< data output beat */
    DMAC512_DB_B_t     dfb_beat;   /*!< data fetch beat */
    DMAC512_OP_MODE_t  DmacMode; 	/*!< Dmac operation mode */ 

    uint64_t	SrcAddr;	          /*!< source address */ 
    uint64_t	DstAddr;	          /*!< destination address */ 
    uint32_t	XferCount;	        /*!< Total transfer count in byte */ 

}DMAC512_InitTypdef;


/**
 * @struct HAL_DMAC512_HandleTypeDef
 * @brief  Holds base address handle and other important info 
 *
 * This structure stores base address handle for DMAC512 module
 */
typedef struct __HAL_DMAC512_HandleTypeDef {

    DMAC512_RegDef * Instance;	  /*!< Register base address         */
    DMAC512_InitTypdef  Init; 	  /*!< DMA configuration parameters  */
  
}DMAC512_HandleTypeDef;


/**
 * @brief Initializes handle with DMAC512 base address
 *	      Every node has a DMAC512 instance. So the base address 
 *	      is different from N00 to Nxy nodes. A  top level macro
 *	      rom runtime.mk decides which base address to be assigned.
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return None
 */
void HAL_DMAC512InitHandle(DMAC512_HandleTypeDef *dmac512_handle, DMAC512_RegDef *address);


/**
 * @brief Configures a DMAC512  for transfers
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return -1 if handle an other parameters are not initialized Properly.
 * 	        0 upon successful configuration. 
 */
int32_t HAL_DMAC512ConfigureChannel(DMAC512_HandleTypeDef *dmac512_handle);


/**
 * @brief Checks if  DMAC512  transfers are done
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return Status of dma_is_busy bit of status register
 */
bool HAL_DMAC512IsBusy(DMAC512_HandleTypeDef *dmac512_handle);


/**
 * @brief Starts DMAC512  transfers
 *
 * @param[in] dmac512_handle handle pointer.
 * @param[out] None.
 * @return None
 */
void HAL_DMAC512StartTransfers(DMAC512_HandleTypeDef *dmac512_handle);

/** @} */ // End of HAL DMAC512 group

#ifdef __cplusplus
}
#endif //

#endif //__HAL_DMAC512_H

