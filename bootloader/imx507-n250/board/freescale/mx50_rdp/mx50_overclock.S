/*
 * Copyright (C) 2010-2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>
#include <asm/arch/mx50.h>

#define ARM_CTRL_DCACHE     1 << 2
#define ARM_CTRL_ICACHE     1 << 12
#define ARM_AUXCR_L2EN      1 << 1

/* PLL Register Offsets */
#define MXC_PLL_DP_CTL          0x00
#define MXC_PLL_DP_CONFIG       0x04
#define MXC_PLL_DP_OP           0x08
#define MXC_PLL_DP_MFD          0x0C
#define MXC_PLL_DP_MFN          0x10
#define MXC_PLL_DP_MFNMINUS     0x14
#define MXC_PLL_DP_MFNPLUS      0x18
#define MXC_PLL_DP_HFS_OP       0x1C
#define MXC_PLL_DP_HFS_MFD      0x20
#define MXC_PLL_DP_HFS_MFN      0x24
#define MXC_PLL_DP_MFN_TOGC     0x28
#define MXC_PLL_DP_DESTAT       0x2c


.section ".text", "x"
/*
 *  mx50_overclock
 *
 *  Set the DDR into Self Refresh
 *  Switch CPU clock to PLL2
 *  re-lock PLL1 to 1GHz
 *  Switch CPU clock to PLL1
 *  DDR exit Self-Refresh
 *  Note: IRQs are already disabled.
 */
.globl mx50_overclock
mx50_overclock:
    stmfd   sp!, {r6,r8,r9,r12}     @ Save registers

    mov    r6, r0                       @save databahn address
    mov    r8, r1                       @save ccm base address
    mov    r12, r2                      @save pll1 base address

    /* Make sure no TLB miss will occur when the DDR is in self refresh. */
    /* Invalidate TLB single entry to ensure that the address is not
      * already in the TLB.
      */
    adr   r9, LoopCKE2               @Address in this function.
    mcr   p15, 0, r9, c8, c7, 1      @ Make sure freq code address
                                             @ is not already in TLB.
    mcr   p15, 0, r8, c8, c7, 1      @ Make sure CCM address
                                             @ is not already in TLB.
    mcr   p15, 0, r6, c8, c7, 1      @ make sure Databahn address
                                             @ is not already in TLB.
    mcr   p15, 0, r12, c8, c7, 1     @ make sure PLL address
                                             @ is not already in TLB.

    mrc   p15, 0, r0, c10, c0, 0    @ Read the TLB lockdown register
    orr    r0, r0, #1                    @ Set the Preserve bit.
    mcr   p15, 0, r0, c10, c0, 0    @ Write to the lockdown register
    ldr    r2, [r8]                        @ TLB will miss,
                                             @CCM address will be loaded
    ldr    r2, [r6]                        @ TLB will miss,
                                             @Databahn address will be loaded
    ldr    r2, [r12]                        @ TLB will miss,
                                             @PLL address will be loaded

    ldr    r2, [r9]                        @ TLB will miss
    mrc  p15, 0, r0, c10, c0, 0      @ Read the lockdown register
                                             @ (victim will be incremented)
    bic   r0, r0, #1                     @ Clear the preserve bit
    mcr   p15, 0, r0, c10, c0, 0     @ Write to the lockdown register.


/* Before putting DDR into self-refresh, make sure
    any LPM mode that the DDR might be in is exited.
*/
    /* If Databahn is in LPM4, exit that mode first. */
    ldr     r0,[r6, #0x50] 
    bic     r0, r0, #0x1F
    str     r0,[r6, #0x50]

LoopCKE2:
    /*Wait for CKE = 1 */
    ldr     r0,[r6, #0xfc]
    and    r0, r0, #0x10000
    ldr     r2, =0x10000
    cmp     r0, r2
    bne     LoopCKE2

    /* Enter self-refresh mode */
    ldr     r0,[r6, #0x4c]
    orr     r0,r0,#0x1
    str     r0,[r6, #0x4c]

LoopCKE0:
    /* Wait for CKE = 0 */
    ldr     r0,[r6, #0xfc]
    and    r0, r0, #0x10000
    ldr     r2, =0x10000
    cmp     r0, r2
    beq     LoopCKE0

    /* Wait for ~4 us. */
    mov   r1, #100
1: subs   r1, r1, #1
    bge    1b

    /* Mask the DLL lock state change, set bit 8 in int_mask. */
    ldr    r0, [r6, #0xac]
    orr    r0, r0, #0x100
    str    r0, [r6, #0xac]

    /* Stop controller */
    ldr     r0,[r6]
    bic     r0, r0, #0x1
    str     r0,[r6]

    /* Clear the DLL lock state change bit 8 in int_ack */
    ldr     r0, [r6, #0xa8]
    orr     r0, r0, #0x1000000
    str     r0, [r6, #0xa8]

    /* Clear the interrupt mask for DLL lock state.
      * Bit 8 in int_mask */
    ldr     r0, [r6, #0xac]
    bic     r0, r0, #0x100
    str     r0, [r6, #0xac]

    /* Apply the PLL issue workaround.
       DDR is in self-refresh. So no need to move DDR clock.
       1. Change ARM clock to be sourced from PLL2.
       2. Update MFN to transition to PLL1 to 1056MHz by applying the
          following factors:
          MFI = 10, MFN = 180, MFD = 179. PLL1 Freq = ~1056MHz
       3. Request PLL to load new MFN using DP_CONFIG (LDREQ bit).
       4. No need to wait for new PLL rate.  PLL will be disabled
           during suspend.
     */

   /* Step-clk is sourced from PLL2/1 */
    ldr      r1, [r8, #0x0c]
    bic      r1, r1, #(0xf << 3)
	orr		 r1, r1, #(0x2 << 7)
    str      r1, [r8, #0x0c]

    /* Set ARM to be sourced from Step-clk. */
    ldr     r1, [r8, #0x0c]
    orr     r1, r1, #0x4
    str     r1, [r8, #0x0c]

    /* Now do the MFN changes to relock PLL1 at 1056MHz. */
    ldr     r1, [r12, #MXC_PLL_DP_CONFIG]
    bic     r1, r1, #0x2
    str     r1, [r12, #MXC_PLL_DP_CONFIG] /* disable auto-restart AREN bit */

   /* MFI = 10, MFN = 180, MFD = 179. PLL1 Freq = ~ 1056MHz. */
    ldr     r1, =0xA0
    str     r1, [r12, #MXC_PLL_DP_OP]
    str     r1, [r12, #MXC_PLL_DP_HFS_OP]

    ldr  	r1, =180
    str     r1, [r12, #MXC_PLL_DP_MFN]
    str     r1, [r12, #MXC_PLL_DP_HFS_MFN]

    ldr  	r1, =179
    str     r1, [r12, #MXC_PLL_DP_MFD]
    str     r1, [r12, #MXC_PLL_DP_HFS_MFD]

    /* Manually restart PLL1 */
    ldr     r1, =0x00001236 /* Set PLM =1, manual restart and enable PLL*/
    str     r1, [r12, #MXC_PLL_DP_CTL]

    /* Make sure the PLL is locked. */
2:    ldr    r1, [r12, #MXC_PLL_DP_CTL]
    ands  r1, r1, #0x1
    beq   2b

    /* Wait for ~10 us. */
    mov   r1, #250
3: subs   r1, r1, #1
    bge    3b

    /* Continue DPLL issue workaround.
      5. System will resume with PLL1 locked at 1056 MHz.
          ARM will be sourced from 24MHz OSC.
      6. Update MFN to transition to 1000 MHz by applying
          the following factor:
          MFN = 75
      7. Request PLL to load new MFN using DP_CONFIG (LDREQ bit).
      8. Wait for acknowledge of new MFN factor from
          PLL by polling DP_CONFIG (LDREQ bit).
      9. PLL1 will now be locked at 1000 MHz.
      10. Delay 4 usec to avoid PLL instability window
      11. Move ARM clock to be sourced from PLL1.
    */

    /* Set PLL1 to 1000MHz, MFN = 75. */
    ldr    r1, =75
    str   r1, [r12, #MXC_PLL_DP_MFN]
    str   r1, [r12, #MXC_PLL_DP_HFS_MFN]

    /* Set up the LDREQ */
    ldr    r1, [r12, #MXC_PLL_DP_CONFIG]
    orr    r1, r1, #1
    str    r1, [r12, #MXC_PLL_DP_CONFIG]

   /* Wait for LDREQ bit to clear. */
4: ldr    r1, [r12, #MXC_PLL_DP_CONFIG]
    tst  r1, #1
    bne   4b

    /* Wait for ~4 us. */
    mov   r1, #100
5: subs   r1, r1, #1
    bge    5b

    /* Move ARM back to PLL1 */
    ldr     r1, [r8, #0x0c]
    bic     r1, r1, #0x4
    str     r1, [r8, #0x0c]

	/* Update new DDR divider */
	ldr     r0, [r8, #0x98]
	bic     r0, r0, #0x3f
	orr     r0, r3
	str     r0, [r8, #0x98]

    /* Wait for ~4 us. */
    mov   r1, #100
6:  subs   r1, r1, #1
    bge    6b

    /* Start DDR controller */
    ldr     r0,[r6]
    orr     r0,r0,#0x1
    str     r0,[r6]

LoopPHY:
    /* Wait for PHY ready */
    ldr     r0,[r6, #0x264]
    and    r0, r0, #0xfffffffe
    ldr      r2, =0x0
    cmp    r0, r2
    beq     LoopPHY

DllLock:
    ldr    r0, [r6, #0xa8]
    and    r0, r0, #0x100
    ldr    r2, =0x100
    cmp    r0, r2
    bne    DllLock

    /*Leave self-refresh mode */
    ldr     r0,[r6, #0x4c]
    and    r0,r0,#0xfffffffe
    str     r0,[r6, #0x4c]

LoopCKE1:
    /*Wait for CKE = 1 */
    ldr     r0,[r6, #0xfc]
    and    r0, r0, #0x10000
    ldr     r2, =0x10000
    cmp     r0, r2
    bne     LoopCKE1

    /* Restore registers */
    ldmfd sp!, {r6,r8,r9,r12}
    mov     pc, lr

