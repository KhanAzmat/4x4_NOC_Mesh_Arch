/**
  ******************************************************************************
  * @file    rvv_dmac512.h
  * @author  Lakshmikanth
  * @version V1.0.0
  * @date    18-March-2025
  * @brief   System low level driver module header for DMAC512 driver protottypes. 
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

#ifndef __RVV_DMAC512_H
#define __RVV_DMAC512_H

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif
/** @defgroup DMAC512 Low level driver Module
 *  @brief This module defines register offsets, Bit definitions and utility macros
 *  @{
 */

	
/**
 * @name  DMAC512 channel registers Definitions
 * @brief These set of registers are common for all channels n the system 
 * @{
 */
typedef struct {

    __IO uint32_t   DMAC_CONTROL;           	/*!< DMAC512 Control Register (Offset 0x00) */
    __IO uint32_t   DMAC_STATUS;              /*!< DMAC512 Status           (Offset 0x04) */
    __IO uint32_t   RESERVED0[2];	           	/*!< Reserved (Offset 0x08 ~ 0x0F) */

    __IO uint32_t   DMAC_INTR;	              /*!< DMAC512 Interrupt Register      (Offset 0x10) */ 
    __IO uint32_t   DMAC_INTR_MASK;           /*!< DMAC512 Interrupt Mask register (offset 0x14) */
    __IO uint32_t   RESERVED1[2];           	/*!< Reserved (Offset 0x18 - 0x1F) */

    __IO uint64_t   DMAC_SRC_ADDR;            /*!< DMAC512 source address Register (Offset 0x20) */ 
    __IO uint32_t   RESERVED2[2];        	 	  /*!< Reserved (Offset 0x28 - 0x2F) */

    __IO uint64_t   DMAC_DST_ADDR;   	        /*!< DMAC512 destination address Register (Offset 0x30) */ 
    __IO uint32_t   RESERVED3[2];        	 	  /*!< Reserved (Offset 0x38 - 0x3F) */

    __IO uint32_t   DMAC_TOTAL_XFER_CNT;      /*!< Reserved (0x40) */
    __IO uint32_t   RESERVED4[3];        	 	  /*!< Reserved (Offset 0x44 - 0x4F) */

} DMAC512_RegDef;

/** @} */ // End of DMAC512 register offset structure definition


/******************************************************************************/
/*                                                                            */
/*               DMAC512 registers bit definitions                            */
/*                                                                            */
/******************************************************************************/


/**************************************************************************
 *  bit masks and positions of DMAC512 ctrl register (Offset 0x00)
 **************************************************************************/

/* bit positions */
#define DMAC512_CTRL_DOB_B_SHIFT        (20)    // [22:20]
#define DMAC512_CTRL_DFB_B_SHIFT        (16)    // [18:16]
#define DMAC512_CTRL_DMAC_MODE_SHIFT     (8)   	// [9:8]
#define DMAC512_CTRL_DMAC_RST_SHIFT    	 (0)   	// [0]
											
/* bit masks */
#define DMAC512_CTRL_DOB_B_MASK         ( 0x7 << DMAC512_CTRL_DOB_B_SHIFT )      // 3 bits
#define DMAC512_CTRL_DFB_B_MASK         ( 0x7 << DMAC512_CTRL_DFB_B_SHIFT )      // 3 bits
#define DMAC512_CTRL_DMAC_MODE_MASK    	( 0x3 << DMAC512_CTRL_DMAC_MODE_SHIFT )  // 2 bits
#define DMAC512_CTRL_DMAC_RST_MASK    	( 0x1 << DMAC512_CTRL_DMAC_RST_SHIFT )   // 1 bit

/* operation */
#define SET_DMAC512_CTRL_DOB_B(REG,VAL)  ( (REG) = ( (REG) & ~DMAC512_CTRL_DOB_B_MASK )   | ( (VAL) << DMAC512_CTRL_DOB_B_SHIFT) )
#define SET_DMAC512_CTRL_DFB_B(REG,VAL)  ( (REG) = ( (REG) & ~DMAC512_CTRL_DFB_B_MASK )   | ( (VAL) << DMAC512_CTRL_DFB_B_SHIFT) )
#define SET_DMAC512_CTRL_MODE(REG, VAL)  ( (REG) = ( (REG) & ~DMAC512_CTRL_DMAC_MODE_MASK)| ( (VAL) << DMAC512_CTRL_DMAC_MODE_SHIFT) )
#define SET_DMAC512_CTRL_RST(REG, VAL)   ( (REG) = ( (REG) & ~DMAC512_CTRL_DMAC_RST_MASK) | ( (VAL) << DMAC512_CTRL_DMAC_RST_SHIFT) )

#define GET_DMAC512_DOB_B(REG)      ( ((REG) & DMAC512_CTRL_DOB_B_MASK)     >> DMAC512_CTRL_DOB_B_SHIFT )
#define GET_DMAC512_DFB_B(REG)      ( ((REG) & DMAC512_CTRL_DFB_B_MASK)     >> DMAC512_CTRL_DFB_B_SHIFT )
#define GET_DMAC512_MODE(REG)       ( ((REG) & DMAC512_CTRL_DMAC_MODE_MASK) >> DMAC512_CTRL_DMAC_MODE_SHIFT )
#define GET_DMAC512_RST(REG)        ( ((REG) & DMAC512_CTRL_DMAC_RST_MASK ) >> DMAC512_CTRL_DMAC_RST_SHIFT )


/**************************************************************************
 *  bit masks and positions of DMAC512 status register (Offset 0x04)
 **************************************************************************/

/* bit positions */
#define DMAC512_STATUS_DMAC_BUSY_SHIFT  (0)   // [0]	

/* bit masks */
#define DMAC512_STATUS_DMAC_BUSY_MASK   (0x1U << DMAC512_CTRL_DMAC_MODE_SHIFT)   // 1 bit

/* operation */
#define GET_DMAC512_STATUS_DMAC_BUSY(REG)   (((REG) & DMAC512_STATUS_DMAC_BUSY_MASK) >> DMAC512_STATUS_DMAC_BUSY_SHIFT)


/**************************************************************************
 *  bit masks and positions of DMAC512 interrupt register (Offset 0x10)
 **************************************************************************/

/* bit positions */
#define DMAC512_INTR_DMAC_INTR_SHIFT    (0)   // [0]

/* bit masks */
#define DMAC512_INTR_DMAC_INTR_MASK    	(0x1U << DMAC512_INTR_DMAC_INTR_SHIFT)   // 1 bit

/* operation */
#define CLEAR_DMAC512_DMAC_INTR(REG)       ( (REG) = ((REG) |= DMAC512_INTR_DMAC_INTR_MASK) )
#define GET_DMAC512_DMAC_INTR_STATUS(REG)  ( ((REG) & DMAC512_INTR_DMAC_INTR_MASK) >> DMAC512_INTR_DMAC_INTR_SHIFT )


/*********************************************************************************
 *  bit masks and positions of DMAC512 interrupt mask register (Offset 0x14)
 *********************************************************************************/

#define DMAC512_MASK_DMAC_INTR(REG)     ((REG) |= DMAC512_INTR_DMAC_INTR_MASK)  // Disable DMAC512 Transfer done Interrrupt

#define DMAC512_UNMASK_DMAC_INTR(REG)   ((REG) &= ~DMAC512_INTR_DMAC_INTR_MASK) // Enable DMAC512 Transfer done Interrrupt


/*****************************************************************************************
 *  bit masks and positions of DMAC512 source address register (Offset 0x20)
 *****************************************************************************************/

/* bit positions */
#define DMAC512_SRC_ADDR_SHIFT   (0)   // [63:0]

/* bit masks */
#define DMAC512_SRC_ADDR_MASK    (0xFFFFFFFFU << DMAC512_SRC_ADDR_SHIFT)  // 64 bits

/* operation */
#define SET_DMAC512_SRC_ADDR(REG, VAL)  ((REG) = ((REG) & ~DMAC512_SRC_ADDR_MASK) | ((VAL) << DMAC512_SRC_ADDR_SHIFT))
#define GET_DMAC512_SRC_ADDR(REG)       (((REG) & DMAC512_SRC_ADDR_MASK) >> DMAC512_SRC_ADDR_SHIFT)



/*********************************************************************************************
 *  bit masks and positions of DMAC512 destination address register (Offset 0x30)
 *********************************************************************************************/

/* bit positions */
#define DMAC512_DST_ADDR_SHIFT   (0)   // [63:0]

/* operation */
#define DMAC512_DST_ADDR_MASK    ( 0xFFFFFFFFU << DMAC512_DST_ADDR_SHIFT )  // 64 bits

/* operation */
#define SET_DMAC512_DST_ADDR(REG, VAL)  ( (REG) = ((REG) & ~DMAC512_DST_ADDR_MASK) | ((VAL) << DMAC512_DST_ADDR_SHIFT) )
#define GET_DMAC512_DST_ADDR(REG)       ( ((REG) & DMAC512_DST_ADDR_MASK) >> DMAC512_DST_ADDR_SHIFT )


/*************************************************************************************************
 *  bit masks and positions of DMAC512 total transfer counter register (Offset 0x40)
 *************************************************************************************************/

/* bit positions */
#define DMAC512_TOTAL_XFER_CNT_DMAC_EN_SHIFT    (31)   // [31]
#define DMAC512_TOTAL_XFER_CNT_SHIFT	   	       (0)   // [23:0]

/* bit masks */
#define DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK     ( 0x1U << DMAC512_TOTAL_XFER_CNT_DMAC_EN_SHIFT )   //  1 bit
#define DMAC512_TOTAL_XFER_CNT_MASK    	        ( 0xFFFFFFU << DMAC512_TOTAL_XFER_CNT_SHIFT )      // 24 bits

/* operation */
#define SET_DMAC512_DMAC_EN(REG, VAL)           ( (REG) = ((REG) & ~DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) | ((VAL) << DMAC512_TOTAL_XFER_CNT_DMAC_EN_SHIFT) )
#define SET_DMAC512_TOTAL_XFER_CNT(REG, VAL)    ( (REG) = ((REG) & ~DMAC512_TOTAL_XFER_CNT_MASK) | ((VAL) << DMAC512_TOTAL_XFER_CNT_SHIFT) )

#define GET_DMAC512_TOTAL_XFER_CNT(REG)    ( (REG) = ((REG) & DMAC512_TOTAL_XFER_CNT_MASK) >> DMAC512_TOTAL_XFER_CNT_SHIFT )


/** @} */ // End of Driver DMAC512 group

#ifdef __cplusplus
}
#endif //

#endif //__RVV_DMAC512_H	
