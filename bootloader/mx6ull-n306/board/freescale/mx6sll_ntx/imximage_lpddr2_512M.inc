//=============================================================================			
//init script for i.MX6SLL LPDDR2			
//=============================================================================			
// Revision History			
// v0.1			
//=============================================================================			
			
//wait = on			// comment out this line if not using a debugger
//=============================================================================			
// Disable	WDOG		
//=============================================================================			
//setmem /16	0x020bc000 =	0x30	// comment out this line if not using a debugger
			
//=============================================================================			
// Enable all clocks (they are disabled by ROM code)			
//=============================================================================			
setmem /32	0x020c4068 =	0xffffffff	
setmem /32	0x020c406c =	0xffffffff	
setmem /32	0x020c4070 =	0xffffffff	
setmem /32	0x020c4074 =	0xffffffff	
setmem /32	0x020c4078 =	0xffffffff	
setmem /32	0x020c407c =	0xffffffff	
setmem /32	0x020c4080 =	0xffffffff	
			
//=============================================================================			
// IOMUX			
//=============================================================================			
//DDR IO TYPE:			
setmem /32	0x020E0550 =	0x00080000	// IOMUXC_SW_PAD_CTL_GRP_DDR_TYPE 
setmem /32	0x020E0534 =	0x00000000	// IOMUXC_SW_PAD_CTL_GRP_DDRPKE 

//CLOCK:			
setmem /32	0x020E02AC =	0x00000028	// IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_0

//Control:			
setmem /32	0x020E0548 =	0x00000030	// IOMUXC_SW_PAD_CTL_GRP_CTLDS 
setmem /32	0x020E052C =	0x00000030	// IOMUXC_SW_PAD_CTL_GRP_ADDDS 
setmem /32	0x020E0530 =	0x00020000	// IOMUXC_SW_PAD_CTL_GRP_DDRMODE_CTL 

//Data Strobes:			
setmem /32	0x020E02B0 =	0x00003030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS0 
setmem /32	0x020E02B4 =	0x00003030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS1 
setmem /32	0x020E02B8 =	0x00003038	// IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS2 
setmem /32	0x020E02BC =	0x00003030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS3 

setmem /32	0x020E0540 =	0x00020000	// IOMUXC_SW_PAD_CTL_GRP_DDRMODE


//Data:			
setmem /32	0x020E0544 =	0x00000030	// IOMUXC_SW_PAD_CTL_GRP_B0DS 
setmem /32	0x020E054C =	0x00000030	// IOMUXC_SW_PAD_CTL_GRP_B1DS 
setmem /32	0x020E0554 =	0x00000038	// IOMUXC_SW_PAD_CTL_GRP_B2DS 
setmem /32	0x020E0558 =	0x00000030	// IOMUXC_SW_PAD_CTL_GRP_B3DS 

setmem /32	0x020E0294 =	0x00000030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM0
setmem /32	0x020E0298 =	0x00000030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM1
setmem /32	0x020E029C =	0x00000030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM2
setmem /32	0x020E02A0 =	0x00000030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM3

setmem /32	0x020E02C0 =	0x00082030	// IOMUXC_SW_PAD_CTL_PAD_DRAM_ZQPAD

//=============================================================================			
// DDR Controller Registers			
//=============================================================================			
// Manufacturer:	Nanya		
// Device Part Number:	NT6TL128M32		
// Clock Freq.: 	400MHz		
// Density per CS in Gb: 	4		
// Chip Selects used:	1		
// Total DRAM density (Gb)	4		
// Number of Banks:	8		
// Row address:    	14		
// Column address: 	10		
// Data bus width	32		
//=============================================================================			
			
// MMDC0_MDSCR, set the Configuration request bit during MMDC set up			
setmem /32	0x021b001c =	0x00008000	// Chan 0
			
			
//=============================================================================			
// Calibration setup.			
// 			
//=============================================================================			
setmem /32	0x021b0800 =	0xa1390003	// DDR_PHY_P0_MPZQHWCTRL, enable one time ZQ calibration
setmem /32	0x021b085c = 	0x1b4700c7	//LPDDR2 ZQ params
			
setmem /32 	0x021b0890 = 	0x00300000 	// [MMDC_MPPDCMPR2] ca bus abs delay 
			
//setmem /32	0x021b08b8 = 	0x00000800	//frc_msr.
setmem /32	0x021b0848 =	0x48444a4e	// MPRDDLCTL PHY0
			
setmem /32	0x021b0850 =	0x34363930	// MPWRDLCTL PHY0

// read delays, settings recommended by design to remain constant			
setmem /32	0x021b081c =	0x33333333	// DDR_PHY_P0_MPREDQBY0DL3
setmem /32	0x021b0820 =	0x33333333	// DDR_PHY_P0_MPREDQBY1DL3
setmem /32	0x021b0824 =	0x33333333	// DDR_PHY_P0_MPREDQBY2DL3
setmem /32	0x021b0828 =	0x33333333	// DDR_PHY_P0_MPREDQBY3DL3
			
// write delays, settings recommended by design to remain constant			
setmem /32	0x021b082c =	0xf3333333	//DDR_PHY_P0 all byte 0 data & dm delayed by 3
setmem /32	0x021b0830 =	0xf3333333	//DDR_PHY_P0 all byte 0 data & dm delayed by 3
setmem /32	0x021b0834 =	0xf3333333	//DDR_PHY_P0 all byte 0 data & dm delayed by 3
setmem /32	0x021b0838 =	0xf3333333	//DDR_PHY_P0 all byte 0 data & dm delayed by 3
			
// Read and write data delay, per byte. 			
// For optimized DDR operation it is recommended to run mmdc_calibration on your board, and replace 4 delay register assigns with resulted values 			
// Note:			
// a. DQS gating is not relevant for LPDDR2. DSQ gating calibration section should be skipped, or the following write/read calibration will stall			
// b. The calibration code that runs for both MMDC0 & MMDC1 should be used.			
			
setmem /32	0x021B08C0 =	0x24922492	// [MMDC_MPDCCR] MMDC Duty Cycle Control Register
setmem /32	0x021b08b8 = 	0x00000800	//frc_msr.

			
//=============================================================================			
// Calibration setup end			
//=============================================================================			
			
// Channel0 - starting address 0x80000000			
setmem /32	0x021b0004 =	0x00020024	// MMDC0_MDPDC
setmem /32	0x021b000c =	0x33374133	// MMDC0_MDCFG0
setmem /32	0x021b0010 =	0x00100A82	// MMDC0_MDCFG1
setmem /32	0x021b0038 =	0x00190778	// MMDC0_MDCFG3LP
setmem /32	0x021b0014 =	0x00000093	// MMDC0_MDCFG2
setmem /32	0x021b0018 =	0x00201688	// MMDC0_MDMISC
setmem /32	0x021b002c =	0x0F9F26D2	// MMDC0_MDRWD
setmem /32	0x021b0030 =	0x009f0e10	// MMDC0_MDOR
setmem /32	0x021b0040 =	0x0000004F	// Chan0 CS0_END 
setmem /32	0x021b0000 =	0xC3110000	// MMDC0_MDCTL
			

setmem /32	0x021b083c =	0x20008000	// Reset read FIFO .

// Precharge all command per JEDEC:			
// The memory controller may optionally issue a Precharge-All command 			
// prior to the MRW Reset command.			
// This is strongly recommended to ensure a robust DRAM initialization			
setmem /32	0x021b001c =	0x00008050	// precharge-all command CS0
//setmem /32	0x021b001c =	0x00008018	// precharge-all command CS1
			
//=============================================================================			
// LPDDR2 Mode Register Writes			
//=============================================================================			
// Channel 0 CS0			
setmem /32	0x021b001c =	0x003F8030	// MRW: BA=0 CS=0 MR_ADDR=63 MR_OP=0 (Reset)
setmem /32	0x021b001c =	0xFF0A8030	// MRW: BA=0 CS=0 MR_ADDR=10 MR_OP=0xff (IO calibration, calibration code)
setmem /32	0x021b001c =	0x04028030	// MRW: BA=0 CS=0 MR_ADDR=1  MR_OP=see Register Configuration
setmem /32	0x021b001c =	0x82018030	// MRW: BA=0 CS=0 MR_ADDR=2  MR_OP=see Register Configuration
setmem /32	0x021b001c =	0x02038030	// MRW: BA=0 CS=0 MR_ADDR=3  MR_OP=see Register Configuration
// Channel 0 CS1			
//setmem /32	0x021b001c =	0x003F8038	// MRW: BA=0 CS=1 MR_ADDR=63 MR_OP=0 (Reset)
//setmem /32	0x021b001c =	0xFF0A8038	// MRW: BA=0 CS=1 MR_ADDR=10 MR_OP=0xff (IO calibration, calibration code)
//setmem /32	0x021b001c =	0x82018038	// MRW: BA=0 CS=1 MR_ADDR=1  MR_OP=see Register Configuration
//setmem /32	0x021b001c =	0x06028038	// MRW: BA=0 CS=1 MR_ADDR=2  MR_OP=see Register Configuration
//setmem /32	0x021b001c =	0x03038038	// MRW: BA=0 CS=1 MR_ADDR=3  MR_OP=see Register Configuration
			
			
//######################################################			
//final DDR setup, before operation start:			
setmem /32	0x021b0020 =	0x00001800	// MMDC0_MDREF
setmem /32	0x021b0800 =	0xa1390003	// DDR_PHY_P0_MPZQHWCTRL, enable automatic ZQ calibration
			
setmem /32	0x021b0004 =	0x00020064	// MMDC0_MDPDC now SDCTL power down enabled
			
setmem /32	0x021b0404 =	0x00011006 	//MMDC0_MAPSR ADOPT power down enabled
			
setmem /32	0x021b001c =	0x00000000	// MMDC0_MDSCR, clear this register (especially the configuration bit as initialization is complete)

