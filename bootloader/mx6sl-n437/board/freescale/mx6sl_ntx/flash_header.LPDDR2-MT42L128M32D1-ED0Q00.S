
/* !!!! Need update Len field after adding ddr script !!!!!!!!!!!!!!!!!*/
dcd_hdr:          .word 0x402002D2 /* Tag=0xD2, Len=67*8 + 4 + 4, Ver=0x40 */
write_dcd_cmd:    .word 0x041C02CC /* Tag=0xCC, Len=67*8 + 4, Param=0x04 */

/*###################put the ddr script here ######################*/
/*========================================================================*/
/*init script for i.MX6SL LPDDR2*/
/*========================================================================*/
/* Revision History*/
/* v0.1 : Init pre-silicon version for Samsung K4P8G304EB-AGC1 on CPU LPDDR2
          board. It's currently soldered, not PoPed.*/
/* v0.2 : CCM, IO, LPDDR_2ch config fixed*/

/* If someone is playing this init on different DDR device, or on PoPed board,
   please feedback me with result.*/
/* boaz.perlman@freescale.com*/

/* v0.9 : RALAT 5->3 (improved operation at low freq). CK_FT0_DCC=CK_FT1_DCC=1
          (Improved SDCLK signal shape)*/
/* v0.91 : IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCKE0/1 programming is removed, as this
           got no effect on SDCKE drive strength, pull activiticy.*/
/* v0.93 : DRAM_RESET/DDR_SEL=00, as a workaround to SDCLK cross point and
           duty cycle offsets.*/
/*========================================================================*/

/*wait = on*/
/*========================================================================*/
/* Disable	WDOG*/
/*========================================================================*/
/*setmem /16	0x020bc000 =	0x30*/

/*========================================================================*/
/* Enable all clocks (they are disabled by ROM code)*/
/*========================================================================*/
/*setmem /32	0x020c4068 =	0xffffffff*/
/*setmem /32	0x020c406c =	0xffffffff*/
/*setmem /32	0x020c4070 =	0xffffffff*/
/*setmem /32	0x020c4074 =	0xffffffff*/
/*setmem /32	0x020c4078 =	0xffffffff*/
/*setmem /32	0x020c407c =	0xffffffff*/
/*setmem /32	0x020c4080 =	0xffffffff*/
/*setmem /32	0x020c4084 =	0xffffffff*/

/*DDR clk to 400MHz*/
/*CCM_BASE_ADDR = 0x020c4000*/
MXC_DCD_ITEM(1, CCM_BASE_ADDR + 0x018, 0x00260324)
/*MXC_DCD_ITEM(1, CCM_BASE_ADDR + 0x018, 0x00660324)*/ /* @Sam 200MHz */
/*MXC_DCD_ITEM(1, CCM_BASE_ADDR + 0x018, 0x00260324)*/ /* @Sam 400MHz */

/*========================================================================*/
/* IOMUX*/
/*========================================================================*/
/* Megrez note: IOMUX configs specify absolute addr in Arik IOMUXC. Changes to
   Megrez addr.*/
/* Megrez note: Good chance that drive strength change is required. to change
   them all by editing the LSB value "38"-> ""30" or "28"*/
/* Megrez note: Timing also can be tweaked by drive strength values. It is
   mainly by giving SDCLk and SDQS different values than the sampled signals*/

/*IOMUXC_BASE_ADDR = 0x020e0000*/
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS0*/
MXC_DCD_ITEM(2, IOMUXC_BASE_ADDR + 0x344, 0x00003028)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS1*/
MXC_DCD_ITEM(3, IOMUXC_BASE_ADDR + 0x348, 0x00003028)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS2*/
MXC_DCD_ITEM(4, IOMUXC_BASE_ADDR + 0x34c, 0x00003028)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS3*/
MXC_DCD_ITEM(5, IOMUXC_BASE_ADDR + 0x350, 0x00003028)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM0*/
MXC_DCD_ITEM(6, IOMUXC_BASE_ADDR + 0x30c, 0x00000028)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM1*/
MXC_DCD_ITEM(7, IOMUXC_BASE_ADDR + 0x310, 0x00000028)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM2*/
MXC_DCD_ITEM(8, IOMUXC_BASE_ADDR + 0x314, 0x00000028)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM3*/
MXC_DCD_ITEM(9, IOMUXC_BASE_ADDR + 0x318, 0x00000028)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_CAS*/
MXC_DCD_ITEM(10, IOMUXC_BASE_ADDR + 0x300, 0x00000030)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_RAS*/
MXC_DCD_ITEM(11, IOMUXC_BASE_ADDR + 0x31c, 0x00000030)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_0*/
MXC_DCD_ITEM(12, IOMUXC_BASE_ADDR + 0x338, 0x00000030)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_RESET*/
MXC_DCD_ITEM(13, IOMUXC_BASE_ADDR + 0x320, 0x00000030)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDBA2 - DSE can be configured using Group Control
  Register: IOMUXC_SW_PAD_CTL_GRP_CTLDS*/
MXC_DCD_ITEM(14, IOMUXC_BASE_ADDR + 0x32c, 0x00000000)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDODT0*/
/*MXC_DCD_ITEM(15, IOMUXC_BASE_ADDR + 0x33c, 0x00000008)*/
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDODT1*/
/*MXC_DCD_ITEM(16, IOMUXC_BASE_ADDR + 0x340, 0x00000008)*/
/*IOMUXC_SW_PAD_CTL_GRP_B0DS*/
MXC_DCD_ITEM(15, IOMUXC_BASE_ADDR + 0x5c4, 0x00000028)
/*IOMUXC_SW_PAD_CTL_GRP_B1DS*/
MXC_DCD_ITEM(16, IOMUXC_BASE_ADDR + 0x5cc, 0x00000028)
/*IOMUXC_SW_PAD_CTL_GRP_B2DS*/
MXC_DCD_ITEM(17, IOMUXC_BASE_ADDR + 0x5d4, 0x00000028)
/*IOMUXC_SW_PAD_CTL_GRP_B3DS*/
MXC_DCD_ITEM(18, IOMUXC_BASE_ADDR + 0x5d8, 0x00000028)

/*IOMUXC_SW_PAD_CTL_GRP_ADDDS*/
MXC_DCD_ITEM(19, IOMUXC_BASE_ADDR + 0x5ac, 0x00000030)
/*IOMUXC_SW_PAD_CTL_GRP_CTLDS*/
MXC_DCD_ITEM(20, IOMUXC_BASE_ADDR + 0x5c8, 0x00000030)
/*IOMUXC_SW_PAD_CTL_GRP_DDRMODE_CTL*/
MXC_DCD_ITEM(21, IOMUXC_BASE_ADDR + 0x5b0, 0x00020000)
/*IOMUXC_SW_PAD_CTL_GRP_DDRPKE*/
MXC_DCD_ITEM(22, IOMUXC_BASE_ADDR + 0x5b4, 0x00000000)
/*IOMUXC_SW_PAD_CTL_GRP_DDRMODE*/
MXC_DCD_ITEM(23, IOMUXC_BASE_ADDR + 0x5c0, 0x00020000)
/*IOMUXC_SW_PAD_CTL_GRP_DDR_TYPE*/
MXC_DCD_ITEM(24, IOMUXC_BASE_ADDR + 0x5d0, 0x00080000)
/*Hystersis*/
MXC_DCD_ITEM(25, IOMUXC_BASE_ADDR + 0x5bc, 0x00001000)

/*========================================================================*/
/* DDR Controller Registers*/
/*========================================================================*/
/* Manufacturer:	Samsung*/
/* Device Part Number:	K4P8G304EB-AGC1*/
/* Clock Freq.: 	400MMHz*/
/* MMDC channels:       MMDC0*/
/* Density per CS in Gb: 	512M*/
/* Chip Selects used:	2*/
/* Number of Banks:	8*/
/* Row address:    	14*/
/* Column address: 	10*/
/* Data bus width	32*/
/*========================================================================*/
/*MMDC_P0_BASE_ADDR = 0x021b0000*/
/*MMDC0_MDSCR, set the Configuration request bit during MMDC set up*/
MXC_DCD_ITEM(26, MMDC_P0_BASE_ADDR + 0x01c, 0x00008000)

/*setmem /32      0x021b085c = 0x1b5f01ff*/
/*LPDDR2 ZQ params*/
/*LPDDR2 ZQ params*/
MXC_DCD_ITEM(27, MMDC_P0_BASE_ADDR + 0x85c, 0x1b4700c7)

/*========================================================================*/
/* Calibration setup.*/
/**/
/*========================================================================*/

/*DDR_PHY_P0_MPZQHWCTRL, enable on time ZQ calibration*/
MXC_DCD_ITEM(28, MMDC_P0_BASE_ADDR + 0x800, 0xa1390003)

/* Megrez note: If entire word fails, CA bus might be involved. Try changing
   this:*/
/*ca bus abs delay*/
MXC_DCD_ITEM(29, MMDC_P0_BASE_ADDR + 0x890, 0x00480000)

/* values of 20,40,50,60,7f tried. no difference seen*/

/* Megrez note: This is also for CA bus. A bit-bit fine tuning.*/
/*setmem /32	0x021b48bc =	0x00055555*/
/*DDR_PHY_P1_MPWRCADL*/

/*frc_msr.*/
MXC_DCD_ITEM(30, MMDC_P0_BASE_ADDR + 0x8b8, 0x00000800)

/*DDR_PHY_P0_MPREDQBY0DL3*/
MXC_DCD_ITEM(31, MMDC_P0_BASE_ADDR + 0x81c, 0x33333333)
/*DDR_PHY_P0_MPREDQBY1DL3*/
MXC_DCD_ITEM(32, MMDC_P0_BASE_ADDR + 0x820, 0x33333333)
/*DDR_PHY_P0_MPREDQBY2DL3*/
MXC_DCD_ITEM(33, MMDC_P0_BASE_ADDR + 0x824, 0x33333333)
/*DDR_PHY_P0_MPREDQBY3DL3*/
MXC_DCD_ITEM(34, MMDC_P0_BASE_ADDR + 0x828, 0x33333333)

/*write delayes:*/
/*all byte 0 data & dm delayed by 3*/
MXC_DCD_ITEM(35, MMDC_P0_BASE_ADDR + 0x82c, 0xf3333333)
/*all byte 1 data & dm delayed by 3*/
MXC_DCD_ITEM(36, MMDC_P0_BASE_ADDR + 0x830, 0xf3333333)
/*all byte 2 data & dm delayed by 3*/
MXC_DCD_ITEM(37, MMDC_P0_BASE_ADDR + 0x834, 0xf3333333)
/*all byte 3 data & dm delayed by 3*/
MXC_DCD_ITEM(38, MMDC_P0_BASE_ADDR + 0x838, 0xf3333333)

/* Read and write data delay, per byte.*/
/* For optimized DDR operation it is recommended to run mmdc_calibration on your
   board, and replace 4 delay register assigns with resulted values*/
/* Note:*/
/* a. DQS gating is not relevant for LPDDR2. DSQ gating calibration section
   should be skipped, or the write/read calibration comming after that will
   stall*/
/* b. The calibration code that runs for both MMDC0 & MMDC1 should be used.*/

/*it is strongly recommended to run calibration on your board, and replace
  bellow values:*/

/* Megrez note: New set of values is required for the following 2 delay
   registers. Try running calibration code as in Arik APN.*/

/*Read calibration*/
MXC_DCD_ITEM(39, MMDC_P0_BASE_ADDR + 0x848, 0x44444848)

/*Write calibration*/
MXC_DCD_ITEM(40, MMDC_P0_BASE_ADDR + 0x850, 0x38363832)

/*dqs gating dis*/
MXC_DCD_ITEM(41, MMDC_P0_BASE_ADDR + 0x83c, 0x20000000)
MXC_DCD_ITEM(42, MMDC_P0_BASE_ADDR + 0x840, 0x00000000)

/* Megrez note: Try enabling and changing the clock delay, as part of the
   calibration:*/
/*setmem /32 0x021b0858 = 0xa00*/
/*clk delay*/

/*fine tune duty cyc to low*/
MXC_DCD_ITEM(43, MMDC_P0_BASE_ADDR + 0x8c0, 0x24911492)

/*frc_msr*/
MXC_DCD_ITEM(44, MMDC_P0_BASE_ADDR + 0x8b8, 0x00000800)

/*========================================================================*/
/* Calibration setup end*/
/*========================================================================*/

/* Channel0 - startng address 0x80000000*/

/*setmem /32	0x021b000c =	0x3f436133*/
/* MMDC0_MDCFG0*/
/*MMDC0_MDCFG0*/
MXC_DCD_ITEM(45, MMDC_P0_BASE_ADDR + 0x00c, 0x33374135)
/*MMDC0_MDPDC - where is tCKSRX and tCKSRE defined in LPDDR2 data sheet?????*/
MXC_DCD_ITEM(46, MMDC_P0_BASE_ADDR + 0x004, 0x00020024)
/*MMDC0_MDCFG1*/
MXC_DCD_ITEM(47, MMDC_P0_BASE_ADDR + 0x010, 0x00100A83)
/*MMDC0_MDCFG2*/
MXC_DCD_ITEM(48, MMDC_P0_BASE_ADDR + 0x014, 0x00000093)

/*MMDC0_MDMISC. RALAT=3. Try increasing RALAT if case of failures at higher DDR
  freq*/
MXC_DCD_ITEM(49, MMDC_P0_BASE_ADDR + 0x018, 0x00001688)
/*MMDC0_MDRWD;*/
MXC_DCD_ITEM(50, MMDC_P0_BASE_ADDR + 0x02c, 0x0F9F26D2)
/*MMDC0_MDOR*/
MXC_DCD_ITEM(51, MMDC_P0_BASE_ADDR + 0x030, 0x0000020E)

/*setmem /32	0x021b0038 =	0x001a099a*/
/* MMDC0_MDCFG3LP*/
/*MMDC0_MDCFG3LP*/
MXC_DCD_ITEM(52, MMDC_P0_BASE_ADDR + 0x038, 0x00190778)
/*MMDC0_MDOTC*/
MXC_DCD_ITEM(53, MMDC_P0_BASE_ADDR + 0x008, 0x00000000)

/*CS0_END = 0x8fffffff*/
MXC_DCD_ITEM(54, MMDC_P0_BASE_ADDR + 0x040, 0x0000004F)

#if (PHYS_SDRAM_1_SIZE==(512 * 1024 * 1024)) //[
/*MMDC0_MDCTL*/
MXC_DCD_ITEM(55, MMDC_P0_BASE_ADDR + 0x000, 0xC3110000)
#elif (PHYS_SDRAM_1_SIZE==(256 * 1024 * 1024)) //][ 256MB
MXC_DCD_ITEM(55, MMDC_P0_BASE_ADDR + 0x000, 0xC3010000)
#else //][
	#error "Unkown ram size !!!"
#endif //]

/* Channel0 : Configure DDR device:*/
/* Megrez note: Device drive strength change might help, consult device/JEDEC
   for the values.*/

/*MRW: BA=0 CS=0 MR_ADDR=63 MR_OP=0  //reset*/
MXC_DCD_ITEM(56, MMDC_P0_BASE_ADDR + 0x01c, 0x003f8030)
/*MRW: BA=0 CS=0 MR_ADDR=10 MR_OP=ff /zq*/
MXC_DCD_ITEM(57, MMDC_P0_BASE_ADDR + 0x01c, 0xff0a8030)
/*MRW: BA=0 CS=0 MR_ADDR=1  MR_OP=c2*/
MXC_DCD_ITEM(58, MMDC_P0_BASE_ADDR + 0x01c, 0x82018030)
/*MRW: BA=0 CS=0 MR_ADDR=2  MR_OP=4. tcl=6, tcwl=3*/
MXC_DCD_ITEM(59, MMDC_P0_BASE_ADDR + 0x01c, 0x06028030)
/*MRW: BA=0 CS=0 MR_ADDR=3  MR_OP=2.drive=240/6*/
MXC_DCD_ITEM(60, MMDC_P0_BASE_ADDR + 0x01c, 0x03038030)

/*MRW: BA=0 CS=1 MR_ADDR=63 MR_OP=0  //reset*/
/*MXC_DCD_ITEM(57, MMDC_P0_BASE_ADDR + 0x01c, 0x003f8030)*/
/*MRW: BA=0 CS=1 MR_ADDR=10 MR_OP=ff /zq*/
/*MXC_DCD_ITEM(58, MMDC_P0_BASE_ADDR + 0x01c, 0xff0a8030)*/
/*MRW: BA=0 CS=1 MR_ADDR=1  MR_OP=c2*/
/*MXC_DCD_ITEM(59, MMDC_P0_BASE_ADDR + 0x01c, 0x82018030)*/
/*MRW: BA=0 CS=1 MR_ADDR=2  MR_OP=4. tcl=6, tcwl=3*/
/*MXC_DCD_ITEM(60, MMDC_P0_BASE_ADDR + 0x01c, 0x04028030)*/
/*MRW: BA=0 CS=1 MR_ADDR=3  MR_OP=2.drive=240/6*/
/*MXC_DCD_ITEM(61, MMDC_P0_BASE_ADDR + 0x01c, 0x04038030)*/


/*######################################################*/
/*final DDR setup, before operation start:*/
/*DDR_PHY_P0_MPZQHWCTRL, enable automatic ZQ calibration*/
MXC_DCD_ITEM(61, MMDC_P0_BASE_ADDR + 0x800, 0xa1310003)

/*MMDC0_MDREF*/
MXC_DCD_ITEM(62, MMDC_P0_BASE_ADDR + 0x020, 0x00001800)

/*DDR_PHY_P0_MPODTCTRL*/
/*setmem /32	0x021b0818 =	0*/
MXC_DCD_ITEM(63, MMDC_P0_BASE_ADDR + 0x818, 0x00000000)

/*DDR_PHY_P0_MPMUR0, frc_msr*/
MXC_DCD_ITEM(64, MMDC_P0_BASE_ADDR + 0x8b8, 0x00000800)

/*MMDC0_MDPDC now SDCTL power down enabled*/
MXC_DCD_ITEM(65, MMDC_P0_BASE_ADDR + 0x004, 0x00025564)

/*MMDC0_MAPSR ADOPT power down enabled*/
MXC_DCD_ITEM(66, MMDC_P0_BASE_ADDR + 0x404, 0x00011006)

/*MMDC0_MDSCR, clear this register (especially the configuration bit as
  initialization is complete)*/
MXC_DCD_ITEM(67, MMDC_P0_BASE_ADDR + 0x01c, 0x00000000)


