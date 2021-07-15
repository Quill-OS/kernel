/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <asm/arch/mx50.h>

#ifdef	CONFIG_FLASH_HEADER
#ifndef CONFIG_FLASH_HEADER_OFFSET
# error "Must define the offset of flash header"
#endif

#define DPLL_ERRATA

#if defined(CONFIG_ZQ_CALIB)
.macro do_zq_calib
/*=============================================================================
 * ZQ calibration
 *===========================================================================*/
	ldr r0, =DATABAHN_BASE_ADDR

/*=============================================================================
 * Pu calibration based on 240 Ohm
 *===========================================================================*/
	ldr r1, =0x0
pu_loop:
	cmp r1, #0x20
	beq pu_out

	// (pd + 1) << 24 | (pu + 1) << 16
	ldr r2, =0x0
	add r2, r2, #0x1
	add r1, r1, #0x1
	mov r3, r1, LSL #16
	orr r3, r3, r2, LSL #24

	// Set SW_CFG1
	str r3, [r0, #0x128]

	sub r2, r2, #0x1
	sub r1, r1, #0x1
	// (pd << 8) | pu
	mov r3, r1
	// Set SW_CFG2
	str r3, [r0, #0x12c]

	// Start ZQ comparator
	ldr r2, =0x10000
	str r2, [r0, #0x124]

	// Delay 300ns at least
	ldr r3, =0x0
pu_delay:
	add r3, r3, #0x1
	cmp r3, #0x1000
	bne pu_delay

	// Read compare result
	ldr r3, [r0,#0x14c]
	and r3, r3, #0x1

	// Stop ZQ comparator
	ldr r2, =0x0
	str r2, [r0, #0x124]

	// Add pu value
	add r1, r1, #0x1
	cmp r3, #0x1
	bne pu_loop

pu_out:
	// Pu calibration result in r1
	sub r1, r1, #0x1

/*=============================================================================
 * PD calibration start (based on pu)
 *===========================================================================*/
	ldr r2, =0x0
pd_loop:
	cmp r2, #0xf
	beq pd_out

	// (pd + 1) << 24 | (pu + 1) << 16 | 1 << 4
	add r2, r2, #0x1
	add r1, r1, #0x1
	mov r3, r2, lsl #24
	orr r3, r3, r1, lsl #16
	orr r3, r3, #0x10
	// Set SW_CFG1
	str r3, [r0, #0x128]

	sub r2, r2, #0x1
	sub r1, r1, #0x1
	// (pd << 8) | pu
	mov r3, r2, LSL #8
	orr r3, r3, r1
	// Set SW_CFG2
	str r3, [r0, #0x12c]

	// Start ZQ comparator
	ldr r3, =0x10000
	str r3, [r0, #0x124]

	// Delay 300ns at least
	ldr r3, =0x0
pd_delay:
	add r3, r3, #0x1
	cmp r3, #0x1000
	bne pd_delay

	// Read compare result
	ldr r3, [r0, #0x14c]
	and r3, r3, #0x1

	// Stop ZQ comparator
	ldr r8, =0x0
	str r8, [r0, #0x124]

	// Add pd value
	add r2, r2, #0x1

	cmp r3, #0x0
	bne pd_loop

	// Make sure r2 > 0
	cmp r2, #0x0
	beq pd_loop

pd_out:
	// Pd calibration result in r2
	sub r2, r2, #0x2

#if defined(CONFIG_LPDDR2)
	// Pd add 3
	add  r2, r2, #0x3

/*=============================================================================
 * Pu calibration based on pd value
 *===========================================================================*/
	ldr r1, =0x0
pu_loop_pd:
	cmp r1, #0x20
	beq pu_out_pd

	// (pd + 1) << 24 | (pu + 1) << 16 | 1 << 4
	add r2, r2, #0x1
	add r1, r1, #0x1
	mov r3, r2, LSL #24
	orr r3, r3, r1, LSL #16
	orr r3, r3, #0x10
	// Set SW_CFG1
	str r3, [r0,#0x128]

	sub r2, r2, #0x1
	sub r1, r1, #0x1
	// (pd << 8) | pu
	mov r3, r2, LSL #8
	orr r3, r3, r1
	// Set SW_CFG2
	str r3, [r0,#0x12c]

	// Start ZQ comparator
	ldr r3, =0x10000
	str r3, [r0,#0x124]

	// Delay 300ns at least
	ldr r3, =0x0
pu_delay_pd:
	add r3, r3, #0x1
	cmp r3,#0x1000
	bne pu_delay_pd

	// Read compare result
	ldr r3, [r0, #0x14c]
	and r3, r3, #0x1

	// Stop ZQ comparator
	ldr r8, =0x0
	str r8, [r0,#0x124]

	// Add pu value
	add r1, r1, #0x1
	cmp r3, #0x1
	bne pu_loop_pd

pu_out_pd:
	// Pu calibration result in r1
	sub r1, r1, #0x1
#endif

/*=============================================================================
 * Software load PU/PD value,PU is stored in r1, and PD is stored in r2
 *===========================================================================*/
	// (pd << 8) | pu
	mov r3, r2, LSL #8
	orr r3, r3, r1
	// Set SW_CFG2
	str r3, [r0, #0x12c]

	// ((pd + 1) << 24) | ((pu + 1) <<16)
	add r2, r2, #0x1
	add r1, r1, #0x1
	mov r3, r2, LSL #24
	orr r3, r3, r1, LSL #16
	// Load PU, pu_pd_sel=0
	str r3, [r0, #0x128]

	// Set SW_CFG
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
	ldr r2, =0x00200000
	str r2, [r0, #0x124]

	// Load PD, pu_pd_sel=1
	orr r3, r3, #0x10
	str r3, [r0, #0x128]

	// Set SW_CFG
	str r1, [r0, #0x124]
	str r2, [r0, #0x124]
.endm
#endif

.section ".text.flasheader", "x"
	b	_start
	.org	CONFIG_FLASH_HEADER_OFFSET

/* First IVT to copy the plugin that initializes the system into OCRAM */
ivt_header:        .long 0x402000D1    /* Tag=0xD1, Len=0x0020, Ver=0x40 */
app_code_jump_v:   .long 0xF8006458    /* Plugin entry point */
reserv1:           .long 0x0
dcd_ptr:           .long 0x0
boot_data_ptr:     .long 0xF8006420
self_ptr:          .long 0xF8006400
app_code_csf:      .long 0x0 	       /* reserve 4K for csf */
reserv2:           .long 0x0
boot_data:         .long 0xF8006000
image_len:         .long 4*1024        /* Can copy upto 72K, OCRAM free space */
plugin:            .long 0x1  	       /* Enable plugin flag */

/* Second IVT to give entry point into the bootloader copied to DDR */
ivt2_header:       .long 0x402000D1    //Tag=0xD1, Len=0x0020, Ver=0x40
app2_code_jump_v:  .long _start   // Entry point for the bootloader
reserv3:           .long 0x0
dcd2_ptr:          .long 0x0
boot_data2_ptr:    .long boot_data2
self_ptr2:         .long ivt2_header
app_code_csf2:     .long 0x0 // reserve 4K for csf
reserv4:           .long 0x0
boot_data2:        .long TEXT_BASE
image_len2:        .long _end - TEXT_BASE
plugin2:           .long 0x0

/*=============================================================================
 * Here starts the plugin code
 *===========================================================================*/

plugin_start:

/* Save the return address and the function arguments */
	push    {r0-r6, r8, lr}

/* To return to ROM from plugin, we need to fill in these argument.
 * Here is what need to do:
 * Need to construct the paramters for this function before return to ROM:
 * plugin_download(void **start, size_t *bytes, UINT32 *ivt_offset)
 */
 /* Check r0 if valid address
  * Mfgtools or sb_loader run-plug maybe pass down invalidate data
 */
 	cmp r0, #0xf8000000
	mov r4, r0
 	ldr r3, DDR_DEST_ADDR
	strhi r3, [r0]
	ldr r3, COPY_SIZE
	strhi r3, [r1]
	ldr r3, IVT2_HDR_OFS
	strhi r3, [r2]

/*=============================================================================
 *init script for all MX50 boards
 *===========================================================================*/

/* Setup PLL1 to be 800 MHz */
	ldr r0, =CCM_BASE_ADDR

/* Switch ARM domain to be clocked from LP-APM */
	mov r1, #0x4
	str r1, [r0, #CLKCTL_CCSR]

	ldr r0, =PLL1_BASE_ADDR

#ifdef DPLL_ERRATA
	/*
	 * PLL1 workaround as the following
	 *      (1) Disable AREN bit to avoid PLL1 restart during MFN change
	 *      (2) set PLL1 to ~864Mhz with MFI = 8, MFN = 180, MFD = 179, PDF = 0
	 *      (3) Manual restart PLL1
	 *      (4) Wait PLL1 lock
	 *      (5) Set PLL1 to 800Mhz with only change MFN to 60, others keep
	 *      (6) Set LDREQ bit to load new MFN
	 *      (7) Poll on LDREQ bit for MFN update to be completed
	 *      (8) Delay at least 4 us to avoid PLL1 instability window
	 *      (9) Switch ARM back to PLL1
	 */
	/* Step 1 * /
	ldr r1, [r0, #PLL_DP_CONFIG]
	bic r1, r1, #0x2
	str r1, [r0, #PLL_DP_CONFIG] /* disable auto-restart AREN bit */

	/* Step 2 */
	ldr   r1, =0x80
	str   r1, [r0, #PLL_DP_OP]
	str   r1, [r0, #PLL_DP_HFS_OP]

	ldr   r1, =180
	str   r1, [r0, #PLL_DP_MFN]
	str   r1, [r0, #PLL_DP_HFS_MFN]

	ldr   r1, =179
	str   r1, [r0, #PLL_DP_MFD]
	str   r1, [r0, #PLL_DP_HFS_MFD]

	/* Step 3. */
	ldr    r1, =0x00001236 /* Set PLM =1, manual restart and enable PLL*/
	str   r1, [r0, #PLL_DP_CTL]

	/* Step 4 */
1:    ldr    r1, [r0, #0x0]
	ands r1, r1, #0x1
	beq   1b

	/* Step 5 -  set PLL1 to 800Mhz */
	ldr    r1, =60
	str   r1, [r0, #PLL_DP_MFN]
	str   r1, [r0, #PLL_DP_HFS_MFN]

	/* Step 6 - Set the LDREQ bit */
	ldr    r1, [r0, #PLL_DP_CONFIG]
	orr    r1, r1, #1
	str    r1, [r0, #PLL_DP_CONFIG]

	/* Step 7 - Poll until LDREQ clears */
2:    ldr    r1, [r0, #PLL_DP_CONFIG]
	tst    r1, #0x1
	bne   2b

	/* Step 8 - delay more than 4 us */
	mov   r1, #100
3:   subs   r1, r1, #1
	bge    3b

	/* Step 9 - Switch ARM back to PLL1 */
#else
	ldr r1, =0x1232
	str r1, [r0, #PLL_DP_CTL]     /* Set DPLL ON (set UPEN bit); BRMO=1 */
	ldr r1, =0x2
	str r1, [r0, #PLL_DP_CONFIG]  /* Enable auto-restart AREN bit */

	ldr r1, =DP_OP_800
	str r1, [r0, #PLL_DP_OP]
	str r1, [r0, #PLL_DP_HFS_OP]

	ldr r1, =DP_MFD_800
	str r1, [r0, #PLL_DP_MFD]
	str r1, [r0, #PLL_DP_HFS_MFD]

	ldr r1, =DP_MFN_800
	str r1, [r0, #PLL_DP_MFN]
	str r1, [r0, #PLL_DP_HFS_MFN]

	/* Now restart PLL */
	ldr r1, =0x1232
	str r1, [r0, #PLL_DP_CTL]
wait_pll1_lock:
	ldr r1, [r0, #PLL_DP_CTL]
	ands r1, r1, #0x1
	beq wait_pll1_lock
	
/* Switch ARM back to PLL1 */
#endif
	ldr r0, =CCM_BASE_ADDR
	ldr r1, =0x0
	str r1, [r0,#CLKCTL_CCSR]

/*=============================================================================
 * Enable all clocks (they are disabled by ROM code)
 *===========================================================================*/

	mov r1, #0xffffffff
	str r1, [r0, #0x68]
	str r1, [r0, #0x6c]
	str r1, [r0, #0x70]
	str r1, [r0, #0x74]
	str r1, [r0, #0x78]
	str r1, [r0, #0x7c]
	str r1, [r0, #0x80]
	str r1, [r0, #0x84]

#if defined(CONFIG_LPDDR2)

/* DDR clock setting -- Set DDR to be div 3 to get 266MHz */
/* setmem /32 0x53FD4098 = 0x80000003 */
	ldr r1, =0x80000003
	str r1, [r0, #0x98]

/* poll to make sure DDR dividers take effect */
1:
	ldr r1, [r0, #0x8c]
	ands r1, r1, #0x4
	bne 1b

/*=============================================================================
 * IOMUX
 *===========================================================================*/
	ldr r0, =0x53fa8600
	mov r1, #0x04000000
	ldr r3, =0x00200000
	mov r2, #0x0
//setmem /32 0x53fa86ac = 0x04000000
//IOMUXC_SW_PAD_CTL_GRP_DDR_TYPE, ddr_sel=2'b01 (LPDDR2)
	str r1, [r0, #0xac]
//setmem /32 0x53fa8670 = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDRPKE
// bit 7: PKE=0 (All ddr pads except DQS)
	str r2, [r0, #0x70]
//setmem /32 0x53fa86a4 = 0x00200000
//IOMUXC_SW_PAD_CTL_GRP_CTLDS, dse=3'b100
	str r3, [r0, #0xa4]
//setmem /32 0x53fa8668 = 0x00200000
//IOMUXC_SW_PAD_CTL_GRP_ADDDS, dse=3'b100
	str r3, [r0, #0x68]
//setmem /32 0x53fa8698 = 0x00200000
//IOMUXC_SW_PAD_CTL_GRP_B0DS, dse=3'b100
	str r3, [r0, #0x98]
//setmem /32 0x53fa86a0 = 0x00200000
//IOMUXC_SW_PAD_CTL_GRP_B1DS, dse=3'b100
	str r3, [r0, #0xa0]
//setmem /32 0x53fa86a8 = 0x00200000
//IOMUXC_SW_PAD_CTL_GRP_B2DS, dse=3'b100
	str r3, [r0, #0xa8]
//setmem /32 0x53fa86b4 = 0x00200000
//IOMUXC_SW_PAD_CTL_GRP_B3DS, dse=3'b100
	str r3, [r0, #0xb4]

	ldr r0, =0x53fa8400
//setmem /32 0x53fa8490 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_OPEN
	str r3, [r0, #0x90]
//setmem /32 0x53fa8494 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_OPENFB
	str r3, [r0, #0x94]
//setmem /32 0x53fa8498 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_1
	str r3, [r0, #0x98]
//setmem /32 0x53fa849c = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_0
	str r3, [r0, #0x9c]
//setmem /32 0x53fa84f0 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM0, dse=3'b100
	str r3, [r0, #0xf0]
//setmem /32 0x53fa8500 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM1, dse=3'b100
	str r3, [r0, #0x100]
//setmem /32 0x53fa84c8 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM2, dse=3'b100
	str r3, [r0, #0xc8]
//setmem /32 0x53fa8528 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM3, dse=3'b100
	str r3, [r0, #0x128]
//setmem /32 0x53fa84f4 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS0, dse=3'b100
	str r3, [r0, #0xf4]
//setmem /32 0x53fa84fc = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS1, dse=3'b100
	str r3, [r0, #0xfc]
//setmem /32 0x53fa84cc = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS2, dse=3'b100
	str r3, [r0, #0xcc]
//setmem /32 0x53fa8524 = 0x00200000
//IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS3, dse=3'b100
	str r3, [r0, #0x124]


//*===========================================
//   DDR controller setting
//*===========================================
// CTL setting
	ldr r0, =DATABAHN_BASE_ADDR

#if defined(CONFIG_ZQ_CALIB)
	do_zq_calib
#else
// setmem /32 0x1400012C = 0x00000817  // pd=<<8, pu=<<0
	ldr r1, =0x00000817
	str r1, [r0, #0x12c]
// setmem /32 0x14000128 = 0x09180000  // (pd+1)<<24, (pu+1)<<16
	ldr r1, =0x09180000
	str r1, [r0, #0x128]
// load PU, pu_pd_sel=0
// setmem /32 0x14000124 = 0x00310000  // software load ZQ: 3<<20, 1<<16
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
// setmem /32 0x14000124 = 0x00200000  // clear for next load
	ldr r1, =0x00200000
	str r1, [r0, #0x124]
// load PD, pu_pd_sel=1
// setmem /32 0x14000128 = 0x09180010  // (pd+1)<<24, (pu+1)<<16, 1<<4
	ldr r1, =0x09180010
	str r1, [r0, #0x128]
// setmem /32 0x14000124 = 0x00310000  // software load ZQ: 3<<20, 1<<16
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
// setmem /32 0x14000124 = 0x00200000  // clear for next load
	ldr r1, =0x00200000
	str r1, [r0, #0x124]
#endif

/*===========================================================================
 * DDR setting
 *===========================================================================*/

//setmem /32 0x14000000 = 0x00000500
    ldr r1, =0x00000500
    str r1, [r0, #0x0]
//setmem /32 0x14000008 = 0x0000001b
    ldr r1, =0x0000001b
    str r1, [r0, #0x8]
//setmem /32 0x1400000c = 0x0000d056
    ldr r1, =0x0000d056
    str r1, [r0, #0xc]
//setmem /32 0x14000010 = 0x0000010b
    ldr r1, =0x0000010b
    str r1, [r0, #0x10]
//setmem /32 0x14000014 = 0x00000a6b
    ldr r1, =0x00000a6b
    str r1, [r0, #0x14]
//setmem /32 0x14000018 = 0x02020d0c
    ldr r1, =0x02020d0c
    str r1, [r0, #0x18]
//setmem /32 0x1400001c = 0x0c110302
    ldr r1, =0x0c110302
    str r1, [r0, #0x1c]
//setmem /32 0x14000020 = 0x05020503
    ldr r1, =0x05020503
    str r1, [r0, #0x20]
//setmem /32 0x14000024 = 0x0048eb05
    ldr r1, =0x0048eb05
    str r1, [r0, #0x24]
//setmem /32 0x14000028 = 0x00000606
    ldr r1, =0x00000606
    str r1, [r0, #0x28]
//setmem /32 0x1400002c = 0x09040501
    ldr r1, =0x09040501
    str r1, [r0, #0x2c]
//setmem /32 0x14000030 = 0x02000000
    ldr r1, =0x02000000
    str r1, [r0, #0x30]
//setmem /32 0x14000034 = 0x00000e02
    ldr r1, =0x00000e02
    str r1, [r0, #0x34]
//setmem /32 0x14000038 = 0x00000006
    ldr r1, =0x00000006
    str r1, [r0, #0x38]
//setmem /32 0x1400003c = 0x00002301
    ldr r1, =0x00002301
    str r1, [r0, #0x3c]
//setmem /32 0x14000040 = 0x00050408
    ldr r1, =0x00050408
    str r1, [r0, #0x40]
//setmem /32 0x14000044 = 0x00000300
    ldr r1, =0x00000300
    str r1, [r0, #0x44]
//setmem /32 0x14000048 = 0x00260026
    ldr r1, =0x00260026
    str r1, [r0, #0x48]
//setmem /32 0x1400004c = 0x00010000
    ldr r1, =0x00010000
    str r1, [r0, #0x4c]
//setmem /32 0x1400005c = 0x02000000
    ldr r1, =0x02000000
    str r1, [r0, #0x5c]
//setmem /32 0x14000060 = 0x00000002
    ldr r1, =0x00000002
    str r1, [r0, #0x60]
//setmem /32 0x14000064 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x64]
//setmem /32 0x14000068 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x68]
//setmem /32 0x1400006c = 0x00040042
    ldr r1, =0x00040042
    str r1, [r0, #0x6c]
//setmem /32 0x14000070 = 0x00000001
    ldr r1, =0x00000001
    str r1, [r0, #0x70]
//setmem /32 0x14000074 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x74]
//setmem /32 0x14000078 = 0x00040042
    ldr r1, =0x00040042
    str r1, [r0, #0x78]
//setmem /32 0x1400007c = 0x00000001
    ldr r1, =0x00000001
    str r1, [r0, #0x7c]
//setmem /32 0x14000080 = 0x010b0000
    ldr r1, =0x010b0000
    str r1, [r0, #0x80]
//setmem /32 0x14000084 = 0x00000060
    ldr r1, =0x00000060
    str r1, [r0, #0x84]
//setmem /32 0x14000088 = 0x02400018
    ldr r1, =0x02400018
    str r1, [r0, #0x88]
//setmem /32 0x1400008c = 0x01000e00
    ldr r1, =0x01000e00
    str r1, [r0, #0x8c]
//setmem /32 0x14000090 = 0x0a010101
    ldr r1, =0x0a010101
    str r1, [r0, #0x90]
//setmem /32 0x14000094 = 0x01011f1f
    ldr r1, =0x01011f1f
    str r1, [r0, #0x94]
//setmem /32 0x14000098 = 0x01010101
    ldr r1, =0x01010101
    str r1, [r0, #0x98]
//setmem /32 0x1400009c = 0x00030101
    ldr r1, =0x00030101
    str r1, [r0, #0x9c]
//setmem /32 0x140000a0 = 0x00010000
    ldr r1, =0x00010000
    str r1, [r0, #0xa0]
//setmem /32 0x140000a4 = 0x00010000
    ldr r1, =0x00010000
    str r1, [r0, #0xa4]
//setmem /32 0x140000a8 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0xa8]
//setmem /32 0x140000ac = 0x0000ffff
    ldr r1, =0x0000ffff
    str r1, [r0, #0xac]
//setmem /32 0x140000c8 = 0x02020101
    ldr r1, =0x02020101
    str r1, [r0, #0xc8]
//setmem /32 0x140000cc = 0x01000000
    ldr r1, =0x01000000
    str r1, [r0, #0xcc]
//setmem /32 0x140000d0 = 0x01000201
    ldr r1, =0x01000201
    str r1, [r0, #0xd0]

//setmem /32 0x140000d4 = 0x00000200
    ldr r1, =0x00000200
    str r1, [r0, #0xd4]

//setmem /32 0x140000d8 = 0x00000102
    ldr r1, =0x00000102
    str r1, [r0, #0xd8]

//setmem /32 0x140000dc = 0x0000ffff
    ldr r1, =0x0000ffff
    str r1, [r0, #0xdc]

//setmem /32 0x140000e0 = 0x0000ff00
    ldr r1, =0x0000ff00
    str r1, [r0, #0xe0]

//setmem /32 0x140000e4 = 0x02020000
    ldr r1, =0x02020000
    str r1, [r0, #0xe4]

//setmem /32 0x140000e8 = 0x02020202
    ldr r1, =0x02020202
    str r1, [r0, #0xe8]

//setmem /32 0x140000ec = 0x00000202
    ldr r1, =0x00000202
    str r1, [r0, #0xec]

//setmem /32 0x140000f0 = 0x01010064
    ldr r1, =0x01010064
    str r1, [r0, #0xf0]

//setmem /32 0x140000f4 = 0x01010101
    ldr r1, =0x01010101
    str r1, [r0, #0xf4]

//setmem /32 0x140000f8 = 0x00010101
    ldr r1, =0x00010101
    str r1, [r0, #0xf8]

//setmem /32 0x140000fc = 0x00000064
    ldr r1, =0x00000064
    str r1, [r0, #0xfc]

//setmem /32 0x14000100 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x100]

//setmem /32 0x14000104 = 0x02000802
    ldr r1, =0x02000802
    str r1, [r0, #0x104]

//setmem /32 0x14000108 = 0x04080000
    ldr r1, =0x04080000
    str r1, [r0, #0x108]

//setmem /32 0x1400010c = 0x04080408
    ldr r1, =0x04080408
    str r1, [r0, #0x10c]

//setmem /32 0x14000110 = 0x04080408
    ldr r1, =0x04080408
    str r1, [r0, #0x110]

//setmem /32 0x14000114 = 0x03060408
    ldr r1, =0x03060408
    str r1, [r0, #0x114]

//setmem /32 0x14000118 = 0x00010002
    ldr r1, =0x00010002
    str r1, [r0, #0x118]

//setmem /32 0x1400011c = 0x00001000
    ldr r1, =0x00001000
    str r1, [r0, #0x11c]

// PHY setting
//setmem /32 0x14000200 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x200]

//setmem /32 0x14000204 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x204]

//setmem /32 0x14000208 = 0x35003a27
    ldr r1, =0x35003a27
    str r1, [r0, #0x208]

//setmem /32 0x14000210 = 0x35003a27
    str r1, [r0, #0x210]

//setmem /32 0x14000218 = 0x35003a27
    str r1, [r0, #0x218]

//setmem /32 0x14000220 = 0x35003a27
    str r1, [r0, #0x220]

//setmem /32 0x14000228 = 0x35003a27
    str r1, [r0, #0x228]

//setmem /32 0x1400020c = 0x380002e1
    ldr r1, =0x380002e1
    str r1, [r0, #0x20c]

//setmem /32 0x14000214 = 0x380002e1
    str r1, [r0, #0x214]

//setmem /32 0x1400021c = 0x380002e1
    str r1, [r0, #0x21c]

//setmem /32 0x14000224 = 0x380002e1
    str r1, [r0, #0x224]

//setmem /32 0x1400022c = 0x380002e1
    str r1, [r0, #0x22c]

//setmem /32 0x14000230 = 0x00000000
    ldr r1, =0x00000000
    str r1, [r0, #0x230]

//setmem /32 0x14000234 = 0x00810006
    ldr r1, =0x00810006
    str r1, [r0, #0x234]

//setmem /32 0x14000238 = 0x60101014
    ldr r1, =0x60101014
    str r1, [r0, #0x238]

//setmem /32 0x14000240 = 0x60101014
    str r1, [r0, #0x240]

//setmem /32 0x14000248 = 0x60101014
    str r1, [r0, #0x248]

//setmem /32 0x14000250 = 0x60101014
    str r1, [r0, #0x250]

//setmem /32 0x14000258 = 0x60101014
    str r1, [r0, #0x258]

//setmem /32 0x1400023c = 0x00100b01
    ldr r1, =0x00100b01
    str r1, [r0, #0x23c]

//setmem /32 0x14000244 = 0x00100b01
    str r1, [r0, #0x244]

//setmem /32 0x1400024c = 0x00100b01
    str r1, [r0, #0x24c]

//setmem /32 0x14000254 = 0x00100b01
    str r1, [r0, #0x254]

//setmem /32 0x1400025c = 0x00100b01
    str r1, [r0, #0x25c]

/* Start ddr */
/* setmem /32 0x14000000 = 0x00000501  // bit[0]: start */
	ldr r1, =0x00000501
	str r1, [r0, #0x0]
/* poll to make sure it is done */
1:
	ldr r1, [r0, #0xa8]
	ands r1, r1, #0x10
	beq 1b

#elif defined(CONFIG_DDR2)

//#define DDR2_200M

#ifdef DDR2_200M
/* DDR clock setting -- Set DDR to be div 3 to get 266MHz */
/* setmem /32 0x53FD4098 = 0x80000004  //ddr_clkgate = b10 (always on), ddr_div_pll = 3 (800MHz/4=200MHz) */
	ldr r1, =0x80000004
	str r1, [r0, #0x98]
#else
/*  DDR clock setting -- Set DDR to be div 3 to get 266MHz     */
/* setmem /32 0x53FD4094 = 0xA0000043  // SYS_XTAL_CLKGATE = b10 (always on), SYS_PLL_CLKGATE = b10 (always on), SYS_DIV_XTAL = 0001, SYS_DIV_PLL = 3 (800MHz/3=266MHz) */
	ldr	r0, =CCM_BASE_ADDR
	ldr     r1, =0xA0000043
	str     r1, [r0, #0x94]

/*	DDR clock from PLL1 */
/* setmem /32 0x53FD4090 = 0x00000803  // BYPASS MSHC_XMSCKI CLK = 10 (PLL1), BYPASS SYS CLK = 11(PLL1 ) */
	ldr     r1, =0x00000803
	str     r1, [r0, #0x90]
#endif
/* poll to make sure DDR dividers take effect */
1:
    ldr r1, [r0, #0x8c]
    ands r1, r1, #0x4
    bne 1b

/*	---------- IOMUX SETUP ---------- */
/*	setmem /32 0x53fa86ac = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDR_TYPE */
//DDR PAD TYPE: DDR_SEL[26:25] -- b00: LPDDR1/DDR2;  b10: LPDDR2
	ldr r0, =0x53fa8600
	mov r1, #0x02000000
	mov r3, #0x00200000
	mov r2, #0x0
	str r2, [r0, #0xac]
/*	These DSE values seem to make thing work */

//setmem /32 0x53fa868c = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDRMODE
//[9] DDR_INPUT=1 (DATA: differential input type)
#ifdef DDR2_200M
	mov r1, #0x00000200
#else
	mov r1, #0x00000000
#endif
	str r1, [r0, #0x8c] 
//setmem /32 0x53fa866c = 0x00000200  //IOMUXC_SW_PAD_CTL_GRP_DDRMODE_CTL
//[9] DDR_INPUT=1 (DQS: differential input mode)
	mov r1, #0x00000200
	str r1, [r0, #0x6c] 
//setmem /32 0x53fa8670 = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDRPKE
//Drive-Strength: DSE[21:19]
	str r2, [r0, #0x70] 
/*	0x53fa86a4 = 0x00200000  IOMUXC_SW_PAD_CTL_GRP_CTLDS, dse=3'b111 */
	str r3, [r0, #0xa4]
/*	0x53fa8668 = 0x00200000  IOMUXC_SW_PAD_CTL_GRP_ADDDS, dse=3'b111 */
	str r3, [r0, #0x68]
/*	0x53fa8698 = 0x00200000  IOMUXC_SW_PAD_CTL_GRP_B0DS, dse=3'b111 */
	str r3, [r0, #0x98]
/*	0x53fa86a0 = 0x00200000  IOMUXC_SW_PAD_CTL_GRP_B1DS, dse=3'b111 */
	str r3, [r0, #0xa0]
/*	0x53fa86a8 = 0x00200000  IOMUXC_SW_PAD_CTL_GRP_B2DS, dse=3'b111 */
	str r3, [r0, #0xa8]
/*	0x53fa86b4 = 0x00200000  IOMUXC_SW_PAD_CTL_GRP_B3DS, dse=3'b111 */
	str r3, [r0, #0xb4]

	ldr r0, =0x53fa8400
/*	0x53fa8490 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_OPEN */
	str r3, [r0, #0x90]
/*	0x53fa8494 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_OPENFB */
	str r3, [r0, #0x94]
/*	0x53fa8498 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_1 */
	str r3, [r0, #0x98]
/*	0x53fa849c = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_0 */
	str r3, [r0, #0x9c]
/*	0x53fa84f0 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM0, dse=3'b111*/
	str r3, [r0, #0xf0]
/*	0x53fa8500 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM1, dse=3'b111*/
	ldr r0, =0x53fa8500
	str r3, [r0, #0x00]
/*	0x53fa84c8 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM2, dse=3'b111*/
	ldr r0, =0x53fa8400
	str r3, [r0, #0xc8]
/*	0x53fa8528 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM3, dse=3'b111*/
	ldr r0, =0x53fa8500
	str r3, [r0, #0x28]
/*	0x53fa84f4 = 0x00200080  IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS0, dse=3'b111 , pke=1, pue=1 */
//DSE[21:19], PKE[7], PUE[6]
	ldr r0, =0x53fa8400
	orr r3, r3,#0x00000080 
	str r3, [r0, #0xf4]

/*	0x53fa84fc = 0x00200080	IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS1, dse=3'b111 , pke=1, pue=1	*/
	str r3, [r0, #0xfc]

/*	0x53fa84cc = 0x00200080	IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS2, dse=3'b111 , pke=1, pue=1	*/
	str r3, [r0, #0xcc]
	ldr r0, =0x53fa8500

/*	0x53fa8524 = 0x00200080	IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS3, dse=3'b111 , pke=1, pue=1	*/
	str r3, [r0, #0x24]
	
/*	0x53fa84a4 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_SDODT0, dse=3'b111 , pke=1, pue=1 */
	ldr r0, =0x53fa8400
	str r3, [r0, #0xa4]
/*	0x53fa84f8 = 0x00200000  IOMUXC_SW_PAD_CTL_PAD_DRAM_SDODT1, dse=3'b111 , pke=1, pue=1 */
	ldr r0, =0x53fa8400
	str r3, [r0, #0xf8]


/*	---------- DDR SETUP ---------- */
//*===========================================
//   DDR controller setting
//*===========================================
// CTL setting
	ldr r0, =DATABAHN_BASE_ADDR

#if defined(CONFIG_ZQ_CALIB)
	do_zq_calib
#else
// setmem /32 0x1400012C = 0x0000070d  // pd=<<8, pu=<<0
	ldr r1, =0x0000070d
	str r1, [r0, #0x12c]
// setmem /32 0x14000128 = 0x080e0000  // (pd+1)<<24, (pu+1)<<16
	ldr r1, =0x080e0000
	str r1, [r0, #0x128]
// load PU, pu_pd_sel=0
// setmem /32 0x14000124 = 0x00310000  // software load ZQ: 3<<20, 1<<16
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
// setmem /32 0x14000124 = 0x00200000  // clear for next load
	ldr r1, =0x00200000
	str r1, [r0, #0x124]
// load PD, pu_pd_sel=1
// setmem /32 0x14000128 = 0x080e0010  // (pd+1)<<24, (pu+1)<<16, 1<<4
	ldr r1, =0x080e0010
	str r1, [r0, #0x128]
// setmem /32 0x14000124 = 0x00310000  // software load ZQ: 3<<20, 1<<16
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
// setmem /32 0x14000124 = 0x00200000  // clear for next load
	ldr r1, =0x00200000
	str r1, [r0, #0x124]
#endif

//**************************************************************************
//DDR Controller Registers
//**************************************************************************
//Device: Samsung K4T1G164QF
//Density: 1G bits 1xchip-select
//Organization:   8M words, 16 bits, 8 banks
//Row address:    A0 to A12
//Column address: A0 to A9
//**************************************************************************
//Config: CAS=4, BL=4, 266MHz
//**************************************************************************
/*	CTL setting */
/*	setmem /32 0x14000000    = 0x00000400 // DRAM_CLASS = b0100 (DDR2) */
	ldr r1, =0x00000400
	str r1, [r0, #0x0]
/*	setmem /32 0x14000004    = 0x00000000 */
	ldr r1, =0x00000000
	str r1, [r0, #0x4]
/*	setmem /32 0x14000008    = 0x0000d056  // tINIT=53334 (200us*266MHz) */
	ldr r1, =0x0000d056
	str r1, [r0, #0x8]
/*	setmem /32 0x1400000c    = 0x00000000  //  No meaning for LPDDR1/DDR2 */
	ldr r1, =0x00000000
	str r1, [r0, #0xc]
/*	setmem /32 0x14000010    = 0x00000000  //  No meaning for LPDDR1/DDR2 */
	ldr r1, =0x00000000
	str r1, [r0, #0x10]
/*	setmem /32 0x14000014 = 0x02000000  // CTL05: TINITAREF[27:24] = 2 auto-refresh, for DDR2 and LPDDR1 only; TINIT5[23:0] For LPDDR2 only */
	ldr r1, =0x02000000
	str r1, [r0, #0x14]
/*	setmem /32 0x14000018 = 0x03030808  // CTL06: TCCD[28:24] = 3; WRLAT[19:16] = 3; CASLAT_LIN_GATE = 8; CASLAT_LIN = 8 - [4:1] is 1*cycle, [0] is 0.5*cycle */
	ldr r1, =0x03030808
	str r1, [r0, #0x18]
/*	setmem /32 0x1400001c = 0x0d110404  // CTL07: TRAS_MIN[31:24] = 13; TRC[21:16] = 17; TRRD[10:8] = 4; TBST_INT_INTERVAL[2:0] = 4 */
	ldr r1, =0x0d110404
	str r1, [r0, #0x1c]
/*	setmem /32 0x14000020 = 0x03030503  // CTL08: TMRD[28:24] = 3; TRTP[18:16] = 3; TRP[12:8] = 5; TWTR[3:0] = 3 */
	ldr r1, =0x03030503
	str r1, [r0, #0x20]
#ifdef DDR2_200M
/*	setmem /32 0x14000024 = 0x0036a003  // CTL09: WRITEINTERP[24] = 0; TRAS_MAX[23:8] = 14000 (70uS@200MHz); TMOD[7:0] = 3 */
	ldr r1, =0x0036a003
#else
/*	setmem /32 0x14000024 = 0x0036a004  // CTL09: WRITEINTERP[24] = 0; TRAS_MAX[23:8] = 18608 (70uS@266MHz); TMOD[7:0] = 4 */
	ldr r1, =0x0036a004
#endif
	str r1, [r0, #0x24]
/*	setmem /32 0x14000028 = 0x00000606  // CTL10: CONCURRENTAP[24] = 0; AP[16] = 0; TCKESR[12:8] = 6; TCKE[2:0] = 6 */
//Modified: disable CONCURRENTAP feature CONCURRENTAP=0
//Modified: Enlarge the TCKESR & TCKE
	ldr r1, =0x00000606
	str r1, [r0, #0x28]
/*	setmem /32 0x1400002c = 0x09040401  // CTL11: TDAL[28:24] = 9 (TDAL=TWR+TRP); TWR_INT[20:16] = 4; TRCD_INT[15:8] = 5; TRAS_LOCKOUT[0] = 1 */
	ldr r1, =0x09040401
	str r1, [r0, #0x2c]
/*	setmem /32 0x14000030 = 0x000000c8	// CTL12: TMRR[27:24] = 1; NO_CMD_INIT[16] = 0; TDLL[15:0] = 200 */
	ldr r1, =0x000000c8
	str r1, [r0, #0x30]
/*	setmem /32 0x14000034 = 0x006b0c02	// CTL13: TCPD[31:16] = 107 (What is TCPD?); TFAW[13:8] = 12; BSTLEN[2:0] = b010 (4 words) ??? 4 words ??? */
	ldr r1, =0x006b0c02
	str r1, [r0, #0x34]
/*	setmem /32 0x14000038 = 0x00000005	// CTL14: AUTO_REFRESH_MODE[24] = 0; AREFRESH[16] = 0; REG_DIMM_ENABLE[8] = 0; TRP_AB[4:0] = 5 */
	ldr r1, =0x00000005
	str r1, [r0, #0x38]
#ifdef DDR2_200M
/*	setmem /32 0x1400003c = 0x00001A01  // CTL15: TRFC[17:8] = 26 (127.5nS@200MHz=25.5); TREF_ENABLE[0] = 1 */
	ldr r1, =0x00001A01
#else
/*	setmem /32 0x1400003c = 0x00003401  // CTL15: TRFC[17:8] = 34 (127.5nS@266MHz=33.9); TREF_ENABLE[0] = 1 */
	ldr r1, =0x00003401
#endif
	str r1, [r0, #0x3c]
#ifdef DDR2_200M
/*	setmem /32 0x14000040 = 0x00050618  // CTL16: TREF_INTERVAL[29:16] = 5 (What is TREF_INTERVAL?); TREF[15:0] = 1560 (7.8uS@200MHz=1560) */
	ldr r1, =0x00050618
#else
/*	setmem /32 0x14000040 = 0x000505a0  // CTL16: TREF_INTERVAL[29:16] = 5 (What is TREF_INTERVAL?); TREF[15:0] = 2000 (7.8uS@266MHz=2074.8) */
	ldr r1, =0x000505a0
#endif
	str r1, [r0, #0x40]
/*	setmem /32 0x14000044 = 0x00000000	// CTL17: TPDEX[23:8] = 1 (It should be >= 1, Why set as 0?); POWER_DOWN[0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x44]
/*	setmem /32 0x14000048 = 0x003700c8	// CTL18: TXSNR[31:16] = 39; TXSR[15:0] = 200 */
	ldr r1, =0x003700c8
	str r1, [r0, #0x48]
/*	setmem /32 0x1400004c = 0x00010000	// CTL19: CKE_DELAY[26:24] = 0; ENABLE_QUICK_SREFRESH[16] = 1; PWRUP_SREFRESH_EXIT[8] = 0; SREFRESH[0] = 0  */
	ldr r1, =0x00010000
	str r1, [r0, #0x4c]
/*	setmem /32 0x14000050 = 0x00000000  // CTL20: LOWPOWER_POWER_DOWN_CNT[23:8] = 0; LOWPOWER_CONTROL[4:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x50]
/*	setmem /32 0x14000054 = 0x00000000  // CTL21: LOWPOWER_EXTERNAL_CNT[31:16] = 0; LOWPOWER_SELF_REFRESH_CNT[15:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x54]
/*	setmem /32 0x14000058 = 0x00000000  // CTL22: LOWPOWER_AUTO_ENABLE[4:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x58]
/*	setmem /32 0x1400005c = 0x03000000	// CTL23: CKSRE[27:24] = 6; LOWPOWER_REFRESH_ENABLE[17:16] = 0; LOWPOWER_REFRESH_HOLD[15:0] = 0 */
	ldr r1, =0x03000000
	str r1, [r0, #0x5c]
/*	setmem /32 0x14000060 = 0x00000003  // CTL24: WRITE_MODEREG[8] = 0; CKSRX[3:0] = 6 */
	ldr r1, =0x00000003
	str r1, [r0, #0x60]
/*	setmem /32 0x14000064 = 0x00000000  // CTL25: READ_MODEREG[16:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x64]
/*	setmem /32 0x14000068 = 0x06420000  // CTL26: MR0_DATA_0[30:16] = 0x642 (CL=4); PERIPHERAL_MRR_DATA[15:0] = 0 */
	ldr r1, =0x06420000
	str r1, [r0, #0x68]
/*	setmem /32 0x1400006c = 0x00000002  // CTL27: MR2_DATA_0[30:16] = 0; MR1_DATA_0[14:0] = 0 (reduce strength,disable ODT) */
	ldr r1, =0x00000002
	str r1, [r0, #0x6c]
/*	setmem /32 0x14000070 = 0x00000000  // CTL28: MR16_DATA_0[30:16] = 0 (No meaning for DDR2); MR3_DATA_0[14:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x70]
/*	setmem /32 0x14000074 = 0x06420000  // CTL29: MR0_DATA_1[30:16] = 0x642 (CL=4); MR17_DATA_0[14:0] = 0 (No meaning for DDR2) */
	ldr r1, =0x06420000
	str r1, [r0, #0x74]
/*	setmem /32 0x14000078 = 0x00000000  // CTL30: MR2_DATA_1[30:16] = 0 (No meaning for DDR2); MR1_DATA_1[14:0] = 4 (75ohm ODT) */
	ldr r1, =0x00000000
	str r1, [r0, #0x78]
/*	setmem /32 0x1400007c = 0x00000000  // CTL31: MR16_DATA_1[30:16] = 0 (No meaning for DDR2); MR3_DATA_1[14:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x7c]
/*	setmem /32 0x14000080 = 0x02000000  // CTL32: For LPDDR2 only - [27:16] ZQINIT;  [15:0] MR17_DATA_1 */
	ldr r1, =0x02000000
	str r1, [r0, #0x80]
/*	setmem /32 0x14000084 = 0x00000100  // CTL33: For LPDDR2 only - ZQ */
	ldr r1, =0x00000100
	str r1, [r0, #0x84]
/*	setmem /32 0x14000088 = 0x02400040  // CTL34: For LPDDR2 only - ZQ */
	ldr r1, =0x02400040
	str r1, [r0, #0x88]
/*	setmem /32 0x1400008c = 0x01000000  // CTL35: For LPDDR2 only - ZQ */
	ldr r1, =0x01000000
	str r1, [r0, #0x8c]
#ifdef DDR2_200M
/*	setmem /32 0x14000090 = 0x0a000201  // CTL36: APREBIT[27:24] = 10; COLUMN_SIZE[18:16] = 0 (10-10); ADDR_PINS[10:8] = 2 (15-13); EIGHT_BANK_MODE[0] = 1 */
	ldr r1, =0x0a000201
#else
/*	setmem /32 0x14000090 = 0x0a000201  // CTL36: APREBIT[27:24] = 10; COLUMN_SIZE[18:16] = 0 (10-10); ADDR_PINS[10:8] = 2 (15-13); EIGHT_BANK_MODE[0] = 1 */
	ldr r1, =0x0a000201
#endif
	str r1, [r0, #0x90]
/*	setmem /32 0x14000094 = 0x01011f1f		// CTL37: BANK_SPLIT_EN[24] = b1; ADDR_CMP_EN[16] = b1; COMMAND_AGE_COUNT[12:8] = 0x1f; AGE_COUNT[4:0] = 0x1f */
	ldr r1, =0x01011f1f
	str r1, [r0, #0x94]
/*	setmem /32 0x14000098 = 0x01010101	// CTL38: SWAP_EN[24] = b1; RW_SAME_EN[16] = b1; PRIORITY_EN[8] = b1; PLACEMENT_EN[0] = b1 */
	ldr r1, =0x01010101
	str r1, [r0, #0x98]
/*	setmem /32 0x1400009c = 0x01010103  // CTL39: REDUC[24] = 1;CS_MAP[17:16] = b11;SWAP_PORT_RW_SAME_EN[8] = b1;DISABLE_RW_GROUP_W_BNK_CONFLICT[1:0] = b11 */
	ldr r1, =0x01010103
	str r1, [r0, #0x9c]
/*	setmem /32 0x140000a0 = 0x00000000  // CTL40: FAST_WRITE[24] = b0; LPDDR2_S4[16] = b0; WRDATALAT_REDUC_EN[8] = b0; CMDLAT_REDUC_EN[0] = b0 */
	ldr r1, =0x00000000
	str r1, [r0, #0xa0]
/*	setmem /32 0x140000a4 = 0x00010000  // CTL41: RESYNC_DLL_PER_AREF_EN[16] = b1; RESYNC_DLL[8] = b0; Q_FULLNESS[2:0] = 0 */
	ldr r1, =0x00010000
	str r1, [r0, #0xa4]
/*	setmem /32 0x140000a8 = 0x00000000  // CTL42: INT_ACK[25:16] = 0; INT_STATUS[10:0] = 0 */
	ldr r1, =0x00000000
	str r1, [r0, #0xa8]
/*	setmem /32 0x140000ac = 0x0000ffff		// CTL43: INT_MASK[10:0] = 0xffff */
	ldr r1, =0x0000ffff
	str r1, [r0, #0xac]

//HW_DRAM_CTL44~49 (0x140000b0~c4) Read-only, don't write

/*	setmem /32 0x140000c8 = 0x02020101  // CTL50: ODT_WR_MAP_CS1[25:24] = 2; ODT_RD_MAP_CS1[17:16] = 2; ODT_WR_MAP_CS0[9:8] = 1; ODT_RD_MAP_CS0[1:0] = 1  */
	ldr r1, =0x02020101
	str r1, [r0, #0xc8]
/*	setmem /32 0x140000cc = 0x01000000  // CTL51: ADD_ODT_CLK_SAMETYPE_DIFFCS[27:24] = 1; ADD_ODT_CLK_DIFFTYPE_DIFFCS[20:16] = 0; */
	ldr r1, =0x01000000
	str r1, [r0, #0xcc]
/*	setmem /32 0x140000d0 = 0x01010201  // CTL52: W2W_DIFFCS_DLY[26:24] = 1; W2R_DIFFCS_DLY[18:16] = 1; R2W_DIFFCS_DLY[10:8] = 1; R2R_DIFFCS_DLY[2:0] = 1 */
	ldr r1, =0x01010201
	str r1, [r0, #0xd0]
/*	setmem /32 0x140000d4 = 0x00000200  // CTL53: W2W_SAMECS_DLY[26:24] = 0; W2R_SAMECS_DLY[18:16] = 2; R2W_SAMECS_DLY[10:8] = 2; R2R_SAMECS_DLY[2:0] = 0 */
	ldr r1, =0x00000200
	str r1, [r0, #0xd4]
/*	setmem /32 0x140000d8 = 0x00000101	// CTL54: TDQSCK_MIN[9:8] = 1; TDQSCK_MAX[1:0] = 1 */
	ldr r1, =0x00000101
	str r1, [r0, #0xd8]
#ifdef DDR2_200M
/*	setmem /32 0x140000dc = 0x0000ffff		// CTL55: AXI0_FIFO_TYPE_REG[17:16] = 0; AXI0_EN_SIZE_LT_WIDTH_INSTR[15:0] = 0xffff */
	ldr r1, =0x0000ffff
#else
/*	setmem /32 0x140000dc = 0x0003ffff		// CTL55: AXI0_FIFO_TYPE_REG[17:16] = 3; AXI0_EN_SIZE_LT_WIDTH_INSTR[15:0] = 0xffff */
	ldr r1, =0x0003ffff
#endif
	str r1, [r0, #0xdc]
/*	setmem /32 0x140000e0 = 0x0000ffff		// CTL56: WEIGHTED_ROUND_ROBIN_LATENCY_CONTROL[24] = 0; AXI0_FIFO_TYPE_REG[17:16] = 0; AXI0_EN_SIZE_LT_WIDTH_INSTR[15:0] = 0xffff */
	ldr r1, =0x0000ffff
	str r1, [r0, #0xe0]
/*	setmem /32 0x140000e4 = 0x02020000  // CTL57: AXI0_PRIORITY1_RELATIVE_PRIORITY[27:24] = 2; AXI0_PRIORITY0_RELATIVE_PRIORITY[19:16] = 2;
												// WRR_PARAM_VALUE_ERR[11:8] = Read only; WEIGHTED_ROUND_ROBIN_WEIGHT_SHARING[0] = 0 */
	ldr r1, =0x02020000
	str r1, [r0, #0xe4]
/*	setmem /32 0x140000e8 = 0x02020202  // CTL58: AXI0_PRIORITY5_RELATIVE_PRIORITY[27:24] = 2; AXI0_PRIORITY4_RELATIVE_PRIORITY[19:16] = 2;
												// AXI0_PRIORITY3_RELATIVE_PRIORITY[11:8] = 2;  AXI0_PRIORITY2_RELATIVE_PRIORITY[3:0] = 2; */
	ldr r1, =0x02020202
	str r1, [r0, #0xe8]
/*	setmem /32 0x140000ec = 0x00000202  // CTL59: AXI0_PORT_ORDERING[16] = 0; AXI0_PRIORITY7_RELATIVE_PRIORITY[11:8] = 2;  AXI0_PRIORITY6_RELATIVE_PRIORITY[3:0] = 2; */
	ldr r1, =0x00000202
	str r1, [r0, #0xec]
/*	setmem /32 0x140000f0 = 0x01010064	// CTL60: AXI1_PRIORITY1_RELATIVE_PRIORITY[27:24] = 1; AXI1_PRIORITY0_RELATIVE_PRIORITY[19:16] = 1; AXI0_PRIORITY_RELAX[9:0] = 100 */
	ldr r1, =0x01010064
	str r1, [r0, #0xf0]
/*	setmem /32 0x140000f4 = 0x01010101	// CTL61: AXI1_PRIORITY5_RELATIVE_PRIORITY[27:24] = 1; AXI1_PRIORITY4_RELATIVE_PRIORITY[19:16] = 1; AXI1_PRIORITY3_RELATIVE_PRIORITY[11:8] = 1;  AXI1_PRIORITY2_RELATIVE_PRIORITY[3:0] = 1; */
	ldr r1, =0x01010101
	str r1, [r0, #0xf4]
/*	setmem /32 0x140000f8 = 0x00010101	// CTL62: AXI1_PORT_ORDERING[16] = 1; AXI1_PRIORITY7_RELATIVE_PRIORITY[11:8] = 1;  AXI1_PRIORITY6_RELATIVE_PRIORITY[3:0] = 1; */
	ldr r1, =0x00010101
	str r1, [r0, #0xf8]
/*	setmem /32 0x140000fc = 0x00000064	// CTL63: CKE_STATUS[16] = Read only; AXI1_PRIORITY_RELAX[9:0] = 100 */
	ldr r1, =0x00000064
	str r1, [r0, #0xfc]
/*	setmem /32 0x14000100 = 0x00000000	// CTL64: TDFI_PHY_WRLAT[27:24] = Read only; DLL_RST_ADJ_DLY[23:16] = 0; DLL_RST_DELAY[15:0] =0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x100]
/*	setmem /32 0x14000104 = 0x02000702	// CTL65: TDFI_RDDATA_EN_BASE[28:24] = 2; TDFI_PHY_RDLAT[12:8] = 7; TDFI_PHY_WRLAT_BASE[3:0] = 2 */
	ldr r1, =0x02000702
	str r1, [r0, #0x104]
/*	setmem /32 0x14000108 = 0x050e0000	// CTL66: TDFI_CTRLUPD_MAX[31:16] = 0x50e; TDFI_CTRLUPD_MIN[11:8] = Read only; DRAM_CLK_DISABLE[1:0] = b00 */
	ldr r1, =0x050e0000
	str r1, [r0, #0x108]
/*	setmem /32 0x1400010c = 0x050e050e  //	CTL67: TDFI_PHYUPD_TYPE1[31:16] = 0x50e; TDFI_PHYUPD_TYPE0[15:0] = 0x50e; */
	ldr r1, =0x050e050e
	str r1, [r0, #0x10c]
/*	setmem /32 0x14000110 = 0x050e050e  //	CTL68: TDFI_PHYUPD_TYPE3[31:16] = 0x50e; TDFI_PHYUPD_TYPE2[15:0] = 0x50e; */
	ldr r1, =0x050e050e
	str r1, [r0, #0x110]
/*	setmem /32 0x14000114 = 0x0304050e  //	CTL69: WRLAT_ADJ[27:24] = 2; RDLAT_ADJ[20:16] = 3; TDFI_PHYUPD_RESP[15:0] = 0x50e; */
	ldr r1, =0x0304050e
	str r1, [r0, #0x114]
/*	setmem /32 0x14000118 = 0x00010002  // CTL70: ODT_ALT_EN[24] = b1; TDFI_DRAM_CLK_ENABLE[19:16] = 1; TDFI_DRAM_CLK_DISABLE[10:8] = 0; TDFI_CTRL_DELAY[3:0] = 2 */
//Modified: ODT_ALT_EN must be 0
	ldr r1, =0x00010002
	str r1, [r0, #0x118]
/*	setmem /32 0x1400011c = 0x00001000  // CTL71: AXI0_HIDE_BRESP[12] = b1; MDDR_CKE_SEL[8] = b0; AXI0_AWCOBUF[0] = b0 */
//Modified: Recommend AXI0_HIDE_BRESP = 1
	ldr r1, =0x00001000
	str r1, [r0, #0x11c]

/*=============================================================
 *  DDR PHY setting
 *===========================================================*/

/*	setmem /32 0x14000200    = 0x00000000 */
	ldr r1, =0x00000000
	str r1, [r0, #0x200]
/*	setmem /32 0x14000204 = 0x00000000  //  on-chip ODT) */
	ldr r1, =0x00000000
	str r1, [r0, #0x204]

//[26:24] RD_DLY_SEL;  [15:12] DQS_OE_START; [11:8] DQS_OE_END;  [6:4] DATA_OE_START;  [2:0] DATA_OE_END
/* setmem /32 0x14000208 = 0xf4012725  //  data-slice-0: PHY_CTRL_REG_0_B0 */
	ldr r1, =0xf4012725
	str r1, [r0, #0x0208]
/* setmem /32 0x14000210 = 0xf4012725  //  data-slice-1: PHY_CTRL_REG_0_B1 */
	str r1, [r0, #0x210]
/* setmem /32 0x14000218 = 0xf4012725  //  data-slice-2: PHY_CTRL_REG_0_B2 */
	str r1, [r0, #0x218]
/* setmem /32 0x14000220 = 0xf4012725  //  data-slice-3: PHY_CTRL_REG_0_B3 */
	str r1, [r0, #0x0220]
/* setmem /32 0x14000228 = 0xf4012725  //  data-slice-CA: PHY_CTRL_REG_0_CA */
	str r1, [r0, #0x0228]
/* setmem /32 0x1400020c = 0x26c002c0  //  data-slice-0: PHY_CTRL_REG_1_B0 */
//[8:6] GATE_ERR_DELAY;  [5:4] GATE_CLOSE_CFG;  [2:0] GATE_CFG
	ldr r1, =0x26c002c0
	str r1, [r0, #0x020c]
/* setmem /32 0x14000214 = 0x26c002c0  //  data-slice-1: PHY_CTRL_REG_1_B1 */
	str r1, [r0, #0x0214]
/* setmem /32 0x1400021c = 0x26c002c0  //  data-slice-2: PHY_CTRL_REG_1_B2 */
	str r1, [r0, #0x021c]
/* setmem /32 0x14000224 = 0x26c002c0  //  data-slice-3: PHY_CTRL_REG_1_B3 */
	str r1, [r0, #0x0224]
/* setmem /32 0x1400022c = 0x26c002c0  //  data-slice-CA: PHY_CTRL_REG_1_CA */
	str r1, [r0, #0x022c]
	
/*	setmem /32 0x14000230    = 0x00000000  //  RESERVED */
	ldr r1, =0x00000000
	str r1, [r0, #0x230]
/*	setmem /32 0x14000234 = 0x00000005  //  PHY_CTRL_REG_2 */
//[23] DFI_MOBILE_EN=1;  [16] DDR_SEL=1;  [3:0] DFI_RDDATA_VALID >= RD_DLY_SEL + 1
	ldr r1, =0x00000005
	str r1, [r0, #0x234]
	
/*	setmem /32 0x14000238 = 0x60101414  //  data-slice-0: DLL_CTRL_REG_0_B0 */
//[31:29] PHASE_DETECT_SEL;  [28] DLL_BYPASS_MODE;  [23:15] DLL_RD_DELAY_BYPASS;  [14:8] DLL_RD_DELAY;  [7:0] DLL_START_POINT
	ldr r1, =0x60101014
	str r1, [r0, #0x238]
/*	setmem /32 0x14000240 = 0x60101414  //  data-slice-1: DLL_CTRL_REG_0_B1 */
	str r1, [r0, #0x240]
/*	setmem /32 0x14000248 = 0x60101414  //  data-slice-2: DLL_CTRL_REG_0_B2 */
	str r1, [r0, #0x248]
/*	setmem /32 0x14000250 = 0x60101414  //  data-slice-3: DLL_CTRL_REG_0_B3 */
	str r1, [r0, #0x250]
/*	setmem /32 0x14000258 = 0x60101414  //  data-slice-CA: DLL_CTRL_REG_0_CA */
	str r1, [r0, #0x258]
	
/*	setmem /32 0x1400023c = 0x00101001  //  data-slice-0: DLL_CTRL_REG_1_B0 */
//[23:15] DLL_WR_DELAY_BYPASS;  [14:8] DLL_WR_DELAY;  [7:0] DLL_INCR
	ldr r1, =0x00100C01
	str r1, [r0, #0x23c]
/*	setmem /32 0x14000244 = 0x00101001  //  data-slice-1: DLL_CTRL_REG_1_B1 */
	str r1, [r0, #0x244]
/*	setmem /32 0x1400024c = 0x00101001  //  data-slice-2: DLL_CTRL_REG_1_B2 */
	str r1, [r0, #0x24c]
/*	setmem /32 0x14000254 = 0x00101001  //  data-slice-3: DLL_CTRL_REG_1_B3 */
	str r1, [r0, #0x254]
/*	setmem /32 0x1400025c = 0x00101001  //  data-slice-CA: DLL_CTRL_REG_1_CA */
	str r1, [r0, #0x25c]

/*	Start ddr */
/*	setmem /32 0x14000000 = 0x00000401  // bit[0]: start */
	ldr	r1, =0x00000401
	str	r1, [r0, #0x0]

/*	poll to make sure it is done */
1:
	ldr	r1, [r0, #0xa8]
	ands	r1, r1, #0x10
	beq	1b

#else

/*==================================================================
 * lpddr1-mddr
 *=================================================================*/

#if (PHYS_SDRAM_1_SIZE==(128*1024*1024))
	#define RAM_SIZE_128M
	#define RAM_CLK_160M
#endif
// #define RAM_CLK_133M
// #define RAM_CLK_100M 

#ifdef RAM_CLK_160M
/* DDR clock setting -- Set DDR to be div 5 to get 160MHz */
/* setmem /32 0x53FD4098 = 0x80000005 */
	ldr r1, =0x80000005
#elif defined RAM_CLK_133M
/* DDR clock setting -- Set DDR to be div 6 to get 133MHz */
/* setmem /32 0x53FD4098 = 0x80000006 */
	ldr r1, =0x80000006
#elif defined RAM_CLK_100M
/* DDR clock setting -- Set DDR to be div 8 to get 100MHz */
/* setmem /32 0x53FD4098 = 0x80000008 */
	ldr r1, =0x80000008
#else
/* DDR clock setting -- Set DDR to be div 4 to get 200MHz */
/* setmem /32 0x53FD4098 = 0x80000004 */
	ldr r1, =0x80000004
#endif
	str r1, [r0, #0x98]

/* poll to make sure DDR dividers take effect */
1:
    ldr r1, [r0, #0x8c]
    ands r1, r1, #0x4
    bne 1b

/*==================================================================
 * IOMUX
 *=================================================================*/
	ldr r0, =0x53fa8600
	mov r1, #0x00000000
	mov r3, #0x00300000
	mov r2, #0x00100000
//setmem /32 0x53fa86ac = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDR_TYPE, ddr_sel=2'b00 (LPDDR1)
	str r1, [r0, #0xac] 
//setmem /32 0x53fa866c = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDRMODE_CTL, ddr_input=0 (LPDDR1)
	str r1, [r0, #0x6c] 
//setmem /32 0x53fa868c = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDRMODE, non-differential input
	str r1, [r0, #0x8c] 
//setmem /32 0x53fa8670 = 0x00000000  //IOMUXC_SW_PAD_CTL_GRP_DDRPKE
	str r1, [r0, #0x70] 
//setmem /32 0x53fa86a4 = 0x00300000  //IOMUXC_SW_PAD_CTL_GRP_CTLDS, dse=3'b110 (LPDDR1 - 50ohm, LPDDR2 - 40ohm)
	str r3, [r0, #0xa4] 
//setmem /32 0x53fa8668 = 0x00300000  //IOMUXC_SW_PAD_CTL_GRP_ADDDS, dse=3'b110 (impedance setting same as GRP_CTLDS)
	str r2, [r0, #0x68]	
//setmem /32 0x53fa8698 = 0x00300000  //IOMUXC_SW_PAD_CTL_GRP_B0DS, dse=3'b110 (impedance setting same as GRP_CTLDS)
	str r2, [r0, #0x98] 
//setmem /32 0x53fa86a0 = 0x00300000  //IOMUXC_SW_PAD_CTL_GRP_B1DS, dse=3'b110
	str r2, [r0, #0xa0] 
//setmem /32 0x53fa86a8 = 0x00300000  //IOMUXC_SW_PAD_CTL_GRP_B2DS, dse=3'b110
	str r2, [r0, #0xa8] 
//setmem /32 0x53fa86b4 = 0x00300000  //IOMUXC_SW_PAD_CTL_GRP_B3DS, dse=3'b110
	str r2, [r0, #0xb4] 
                        
	ldr r0, =0x53fa8400
	str r3, [r0, #0x90] /* IOMUXC_SW_PAD_CTL_PAD_DRAM_OPEN */ 
	str r3, [r0, #0x94] /* IOMUXC_SW_PAD_CTL_PAD_DRAM_OPENFB */ 
//setmem /32 0x53fa8498 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_1 (impedance setting same as GRP_CTLDS)
	str r3, [r0, #0x98] 
//setmem /32 0x53fa849c = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_0
	str r3, [r0, #0x9c] 
//setmem /32 0x53fa84f0 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM0, dse=3'b110
	str r3, [r0, #0xf0] 
//setmem /32 0x53fa8500 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM1, dse=3'b110 
	str r3, [r0, #0x100]
//setmem /32 0x53fa84c8 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM2, dse=3'b110
	str r3, [r0, #0xc8] 
//setmem /32 0x53fa8528 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM3, dse=3'b110 
	str r3, [r0, #0x128]

	/* orr r3, r3,#0x00000080 */
//setmem /32 0x53fa84f4 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS0, dse=3'b110  
	str r3, [r0, #0xf4] 
//setmem /32 0x53fa84fc = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS1, dse=3'b110  
	str r3, [r0, #0xfc] 
//setmem /32 0x53fa84cc = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS2, dse=3'b110  
	str r3, [r0, #0xcc] 
//setmem /32 0x53fa8524 = 0x00300000  //IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS3, dse=3'b110  
	str r3, [r0, #0x124]

//*===========================================
//   DDR controller setting
//*===========================================
// CTL setting
	ldr r0, =DATABAHN_BASE_ADDR

#if defined(CONFIG_ZQ_CALIB)
	do_zq_calib
#else
// setmem /32 0x1400012C = 0x0000070d  // pd<<8, pu<<0
	ldr r1, =0x0000070d
	str r1, [r0, #0x12c]
// setmem /32 0x14000128 = 0x080e0000  // (pd+1)<<24, (pu+1)<<16
	ldr r1, =0x080e0000
	str r1, [r0, #0x128]
// load PU, pu_pd_sel=0
// setmem /32 0x14000124 = 0x00310000  // software load ZQ: 3<<20, 1<<16
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
// setmem /32 0x14000124 = 0x00200000  // clear for next load
	ldr r1, =0x00200000
	str r1, [r0, #0x124]
// load PD, pu_pd_sel=1
// setmem /32 0x14000128 = 0x080e0010  // (pd+1)<<24, (pu+1)<<16, 1<<4
	ldr r1, =0x080e0010
	str r1, [r0, #0x128]
// setmem /32 0x14000124 = 0x00310000  // software load ZQ: 3<<20, 1<<16
	ldr r1, =0x00310000
	str r1, [r0, #0x124]
// setmem /32 0x14000124 = 0x00200000  // clear for next load
	ldr r1, =0x00200000
	str r1, [r0, #0x124]
#endif

/*==============================================================
 *  DDR setting
 *=============================================================*/
/* setmem /32 0x14000000 = 0x00000100 //DRAM_CLASS = b0001 (LPDDR1) */
	ldr r1, =0x00000100
	str r1, [r0, #0x0]
/* setmem /32 0x14000008 = 0x00009c40 // tINIT=40000 (200us*200MHz) */
	ldr r1, =0x00009c40
	str r1, [r0, #0x8]
/* setmem /32 0x14000014 = 0x02000000 // INITAREF=2; Need to set tINIT5 for LPDDR2 */
	ldr r1, =0x02000000
	str r1, [r0, #0x14]
/* setmem /32 0x14000018 = 0x01010606 // TCCD=1; WRLAT = 1; CASLAT_LIN_GATE = 3.5; CASLAT_LIN = 3 */
	ldr r1, =0x01010706
	str r1, [r0, #0x018]
/* setmem /32 0x1400001c = 0x080b0201 // TRAS_MIN=8; TRC=11; TRRD=2; TBST_INT_INTERVAL=4 */
	ldr r1, =0x080b0204
	str r1, [r0, #0x01c]
/* setmem /32 0x14000020 = 0x02010303 // TMRD=2; TRTP=1; TRP=3; TWTR=3 */
	ldr r1, =0x02010303
	str r1, [r0, #0x020]
#ifdef RAM_CLK_160M
/* setmem /32 0x14000024 = 0x012d9302 // WRITEINTERP=1; TRAS_MAX=11667; TMOD=2 */
	ldr r1, =0x012d9302
#elif defined RAM_CLK_133M
/* setmem /32 0x14000024 = 0x01247502 // WRITEINTERP=1; TRAS_MAX=9333; TMOD=2 */
	ldr r1, =0x012d9302
#else
/* setmem /32 0x14000024 = 0x0136b002 // WRITEINTERP=1; TRAS_MAX=14000; TMOD=2 */
	ldr r1, =0x0136b002
#endif
	str r1, [r0, #0x024]
/* setmem /32 0x14000028 = 0x00000602 // CONCURRENTAP=0; AP=0; TCKESR=6; TCKE=2 */
	ldr r1, =0x00000606
	str r1, [r0, #0x028]
/* setmem /32 0x1400002c = 0x06030301 // TDAL=6 (TDAL=TWR+TRP); TWR_INT=3; TRCD_INT=3; TRAS_LOCKOUT=1 */
	ldr r1, =0x06030401
	str r1, [r0, #0x02c]
/* setmem /32 0x14000030 = 0x00000000 // TMRR=0; NO_CMD_INIT=0; TDLL=0 */
	ldr r1, =0x00000000
	str r1, [r0, #0x030]
/* setmem /32 0x14000034 = 0x00000a03 // TCPD=0; TFAW=10; BSTLEN=b011 (8 words) */
	ldr r1, =0x00000a02
	str r1, [r0, #0x034]
/* setmem /32 0x14000038 = 0x00000003 // AUTO_REFRESH_MODE=0; AREFRESH=0; REG_DIMM_ENABLE=0; TRP_AB=3 */
	ldr r1, =0x00000003
	str r1, [r0, #0x038]
/* setmem /32 0x1400003c = 0x00001C01  // TRFC=28; TREF_ENABLE = 1*/
	ldr r1, =0x00001801
	str r1, [r0, #0x03c]
/* setmem /32 0x14000040 = 0x0005050e // TREF_INTERVAL=5; TREF=1554 */
	ldr r1, =0x00050612
	str r1, [r0, #0x040]
/* setmem /32 0x14000044 = 0x00000100 // TPDEX=1; POWER_DOWN=0 */
	ldr r1, =0x00000200
	str r1, [r0, #0x044]
/* setmem /32 0x14000048 = 0x00140017 // TXSNR=20; TXSR=23 */
	ldr r1, =0x00180018
	str r1, [r0, #0x048]
/* setmem /32 0x1400004c = 0x00010000 // CKE_DELAY=0; ENABLE_QUICK_SREFRESH=1; PWRUP_SREFRESH_EXIT=0; SREFRESH=0  */
	ldr r1, =0x00010000
	str r1, [r0, #0x04c]
/* setmem /32 0x1400005c = 0x06000000 // CKSRE=6 */
	ldr r1, =0x06000000
	str r1, [r0, #0x05c]
/* setmem /32 0x14000060 = 0x00000006 // CKSRX=6 */
	ldr r1, =0x00000006
	str r1, [r0, #0x060]
/* setmem /32 0x14000064 = 0x00000000 */
	ldr r1, =0x00000000
	str r1, [r0, #0x064]
/* setmem /32 0x14000068 = 0x00330000 // Mode Register for CS0 (CAS Latency=3, Burst Type=Sequential, Burst Len=4) */
	ldr r1, =0x00320000
	str r1, [r0, #0x068]
	
// Original value:
//   setmem /32 0x1400006c	= 0x00000000
// updated the mDDR extended mode register for 1/2 drive strength - 55ohm.
/* setmem /32 0x1400006c = 0x00000000 // Extended Mode Register for CS0 (Driver Strength=Full, Fresh Area=Full)  */
	ldr r1, =0x00800000
	str r1, [r0, #0x06c]
/* setmem /32 0x14000070 = 0x00000000 // No meaning for LPDDR1 */
	ldr r1, =0x00000000
	str r1, [r0, #0x070]
/* setmem /32 0x14000074 = 0x00330000 // Upper 16-bit - Mode Register for CS1 */
	ldr r1, =0x00320000
	str r1, [r0, #0x074]
/* setmem /32 0x14000078 = 0x00000000 // Extended Mode Register for CS1 for LPDDR1 */
	ldr r1, =0x00800000
	str r1, [r0, #0x078]
/* setmem /32 0x14000080 = 0x02000000 */
	ldr r1, =0x02000000
	str r1, [r0, #0x080]
/* setmem /32 0x14000084 = 0x00000100 */
	ldr r1, =0x00000100
	str r1, [r0, #0x084]
/* setmem /32 0x14000088 = 0x02400040 */
	ldr r1, =0x02400040
	str r1, [r0, #0x088]
/* setmem /32 0x1400008c = 0x01000000 */
	ldr r1, =0x01000000
	str r1, [r0, #0x08c]
#ifdef RAM_SIZE_128M
/* setmem /32 0x14000090 = 0x0a000200 // APREBIT=10; COLUMN_SIZE=0 (10-10); ADDR_PINS=2 (15-13); EIGHT_BANK_MODE=0 */
	ldr r1, =0x0a000200
#else
/* setmem /32 0x14000090 = 0x0a000100 // APREBIT=10; COLUMN_SIZE=0 (10-10); ADDR_PINS=1 (15-14); EIGHT_BANK_MODE=0 */
	ldr r1, =0x0a000100
#endif
	str r1, [r0, #0x090]
/* setmem /32 0x14000094 = 0x01011f1f */
	ldr r1, =0x01011f1f
	str r1, [r0, #0x094]
/* setmem /32 0x14000098 = 0x01010101 */
	ldr r1, =0x01010101
	str r1, [r0, #0x098]
/* setmem /32 0x1400009c = 0x00010101 // REDUC=0; CS_MAP=3 */
	ldr r1, =0x00030101
	str r1, [r0, #0x09c]
/* setmem /32 0x140000a4 = 0x00010000 */
	ldr r1, =0x00010000
	str r1, [r0, #0x0a4]
/* setmem /32 0x140000ac = 0x0000ffff */
	ldr r1, =0x0000ffff
	str r1, [r0, #0x0ac]
/* setmem /32 0x140000c8 = 0x02020101 // No ODT in mDDR, should be all "0" */
	ldr r1, =0x02020101
	str r1, [r0, #0x0c8]
/* setmem /32 0x140000cc = 0x00000000 */
	ldr r1, =0x00000000
	str r1, [r0, #0x0cc]
/* setmem /32 0x140000d0 = 0x01010101 // W2W_DIFFCS_DLY=1; W2R_DIFFCS_DLY=1 (min should be 1?); R2W_DIFFCS_DLY=1; R2R_DIFFCS_DLY=1 */
	ldr r1, =0x01000202
	str r1, [r0, #0x0d0]
/* setmem /32 0x140000d4 = 0x00010100 // W2W_SAMECS_DLY=0; W2R_SAMECS_DLY=1; R2W_SAMECS_DLY=1; R2R_SAMECS_DLY=0; */
	ldr r1, =0x00000200
	str r1, [r0, #0x0d4]
/*  setmem /32 0x140000d8 = 0x00000001 // TDQSCK_MIN=1; TDQSCK_MAX=1 */
	ldr r1, =0x00000001
	str r1, [r0, #0x0d8]
/* setmem /32 0x140000dc = 0x0000ffff */
	ldr r1, =0x0000ffff
	str r1, [r0, #0x0dc]
/* setmem /32 0x140000e0 = 0x0000ffff */
	ldr r1, =0x0000ffff
	str r1, [r0, #0x0e0]
/* setmem /32 0x140000e4 = 0x02020000 */
	ldr r1, =0x02020000
	str r1, [r0, #0x0e4]
/* setmem /32 0x140000e8 = 0x02020202 */
	ldr r1, =0x02020202
	str r1, [r0, #0x0e8]
/* setmem /32 0x140000ec = 0x00000202 */
	ldr r1, =0x00000202
	str r1, [r0, #0x0ec]
/* setmem /32 0x140000f0 = 0x01010064 */
	ldr r1, =0x01010064
	str r1, [r0, #0x0f0]
/* setmem /32 0x140000f4 = 0x01010101 */
	ldr r1, =0x01010101
	str r1, [r0, #0x0f4]
/* setmem /32 0x140000f8 = 0x00010101 */
	ldr r1, =0x00010101
	str r1, [r0, #0x0f8]
/* setmem /32 0x140000fc = 0x00000064 */
	ldr r1, =0x00000064
	str r1, [r0, #0x0fc]
/* setmem /32 0x14000104 = 0x02000602 // TDFI_RDDATA_EN_BASE=2; TDFI_PHY_RDLAT=6; TDFI_PHY_WRLAT_BASE=2 */
	ldr r1, =0x02000602
	str r1, [r0, #0x0104]
/* setmem /32 0x14000108 = 0x050e0000 // TDFI_CTRLUPD_MAX=0x50e; TDFI_CTRLUPD_MIN=0 */
	ldr r1, =0x06120000
	str r1, [r0, #0x0108]
/* setmem /32 0x1400010c = 0x050e050e */
	ldr r1, =0x06120612
	str r1, [r0, #0x010c]
/* setmem /32 0x14000110 = 0x050e050e */
	ldr r1, =0x06120612
	str r1, [r0, #0x0110]
/* setmem /32 0x14000114 = 0x0103050e // WRLAT_ADJ=1; RDLAT_ADJ=3; TDFI_PHYUPD_RESP=0x50e */
	ldr r1, =0x01030612
	str r1, [r0, #0x0114]
/* setmem /32 0x14000118 = 0x00010002 */
	ldr r1, =0x00010002
	str r1, [r0, #0x0118]
/* setmem /32 0x1400011c  = 0x00001000 // set bit[12]=1 improve AXI write effeciency */
	ldr r1, =0x00001000
	str r1, [r0, #0x11c]

/*=============================================================
 *  DDR PHY setting
 *===========================================================*/

/* setmem /32 0x14000200 = 0x00000000 */
	ldr r1, =0x00000000
	str r1, [r0, #0x200]
/* setmem /32 0x14000204 = 0x00000000 */
	str r1, [r0, #0x0204]
/* setmem /32 0x14000208 = 0x35002725 //RD_DLY_SEL=5; DQS_OE_START=3; DQS_OE_END=10; DATA_OE_START=2; DATA_OE_END=7 */
	ldr r1, =0x35002725
	str r1, [r0, #0x0208]
/* setmem /32 0x14000210 = 0x35002725 */
	str r1, [r0, #0x210]
/* setmem /32 0x14000218 = 0x35002725 */
	str r1, [r0, #0x218]
/* setmem /32 0x14000220 = 0x35002725 */
	str r1, [r0, #0x0220]
/* setmem /32 0x14000228 = 0x35002725 */
	str r1, [r0, #0x0228]
/* setmem /32 0x14000234 = 0x00800006 // DFI_MOBILE_EN=1; DDR_SEL=0; LPBK_RD_EN=0; LPBK_WR_EN=0; DFI_RDDATA_VALID=6*/
	ldr r1, =0x00800006
	str r1, [r0, #0x0234]
/* setmem /32 0x1400020c = 0x380002d0 // TSEL_EN=0 */
	ldr r1, =0x380002d0
	str r1, [r0, #0x020c]
/* setmem /32 0x14000214 = 0x380002d0 */
	str r1, [r0, #0x0214]
/* setmem /32 0x1400021c = 0x380002d0 */
	str r1, [r0, #0x021c]
/* setmem /32 0x14000224 = 0x380002d0 */
	str r1, [r0, #0x0224]
/* setmem /32 0x1400022c = 0x380002d0 */
	str r1, [r0, #0x022c]
/* setmem /32 0x14000230 = 0x00000000 */
	ldr r1, =0x00000000
	str r1, [r0, #0x0230]

// Bits[31:29]-DLL phase detect select
// Bit 28 - DLL bypass
// Bits[23:15] - holds read DQS delay when DLL bypass
// Bits[14:8] - holds read DQS delay when DLL normal mode
// Bits[7:0] - DLL start point
/* setmem /32 0x14000238 = 0x60101414 //DLL_CTRL_REG_0_B0 - PHASE_DETECT_SEL=b001 (2 delay elements); DLL_BYPASS_MODE=0; DLL_RD_DELAY=20; DLL_START_POINT=20 */
	ldr r1, =0x60101414
	str r1, [r0, #0x0238]
/* setmem /32 0x14000240 = 0x60101414 */
	str r1, [r0, #0x0240]
/* setmem /32 0x14000248 = 0x60101414 */
	str r1, [r0, #0x0248]
/* setmem /32 0x14000250 = 0x60101414 */
	str r1, [r0, #0x0250]
/* setmem /32 0x14000258 = 0x60101414 */
	str r1, [r0, #0x0258]
/* setmem /32 0x1400023c = 0x00101001 //DLL_CTRL_REG_1_B0 - DLL_WR_DELAY_BYPASS=16; DLL_WR_DELAY=16;DLL_INCR=1 */
	ldr r1, =0x00101001
	str r1, [r0, #0x023c]
/* setmem /32 0x14000244 = 0x00101001 */
	str r1, [r0, #0x0244]
/* setmem /32 0x1400024c = 0x00101001 */
	str r1, [r0, #0x024c]
/* setmem /32 0x14000254 = 0x00101001 */
	str r1, [r0, #0x0254]
/* setmem /32 0x1400025c = 0x00101001 */
	str r1, [r0, #0x025c]

/* Start ddr */
/* setmem /32 0x14000000 = 0x00000101  // bit[0]: start */
	ldr r1, =0x00000101
	str r1, [r0, #0x0]
/* poll to make sure it is done */
1:
	ldr r1, [r0, #0xa8]
	ands r1, r1, #0x10
	beq 1b

#endif

/*
 * The following is to fill in those arguments for this ROM function
 * pu_irom_hwcnfg_setup(void **start, size_t *bytes, const void *boot_data)
 *
 * This function is used to copy data from the storage media into DDR.

 * start - Initial (possibly partial) image load address on entry.
 * Final image load address on exit.
 * bytes - Initial (possibly partial) image size on entry.
 * Final image size on exit.
 * boot_data - Initial @ref ivt Boot Data load address.
 */

	ldr r0, =ROM_SI_REV
	ldr r1, [r0]
	cmp r1, #0x11

	adr r0, DDR_DEST_ADDR
	adr r1, COPY_SIZE
	adr r2, BOOT_DATA

before_calling_rom___pu_irom_hwcnfg_setup:
	mov r3, #0x2a00
	/* Different ROM address for TO 1.0 & TO 1.1.1 */
	addeq r3, r3, #0xad
	addne r3, r3, #0x19
	cmp r4, #0xf8000000
	blxhi r3
after_calling_rom___pu_irom_hwcnfg_setup:

	pop {r0-r6, r8, lr}
	mov r0, #1
	bx lr          /* return back to ROM code */

/* Data be copied by pu_irom_hwcnfg_setup() */
DDR_DEST_ADDR:		.word TEXT_BASE
COPY_SIZE:		.word _end - TEXT_BASE
BOOT_DATA:		.word TEXT_BASE
			.word _end - TEXT_BASE
			.word 0
IVT2_HDR_OFS:           .word ivt2_header - TEXT_BASE

#endif
