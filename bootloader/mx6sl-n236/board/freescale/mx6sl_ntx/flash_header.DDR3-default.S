
/* !!!! Need update Len field after adding ddr script !!!!!!!!!!!!!!!!!*/
dcd_hdr:          .word 0x400802D2 /* Tag=0xD2, Len=64*8 + 4 + 4, Ver=0x40 */
write_dcd_cmd:    .word 0x040402CC /* Tag=0xCC, Len=64*8 + 4, Param=0x04 */

/*###################put the ddr script here ######################*/
/*========================================================================*/
/*init script for i.MX6SL LVDDR3*/
/*========================================================================*/
/* Revision History*/
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
MXC_DCD_ITEM(2, IOMUXC_BASE_ADDR + 0x344, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS1*/
MXC_DCD_ITEM(3, IOMUXC_BASE_ADDR + 0x348, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS2*/
MXC_DCD_ITEM(4, IOMUXC_BASE_ADDR + 0x34c, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDQS3*/
MXC_DCD_ITEM(5, IOMUXC_BASE_ADDR + 0x350, 0x00000038)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM0*/
MXC_DCD_ITEM(6, IOMUXC_BASE_ADDR + 0x30c, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM1*/
MXC_DCD_ITEM(7, IOMUXC_BASE_ADDR + 0x310, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM2*/
MXC_DCD_ITEM(8, IOMUXC_BASE_ADDR + 0x314, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_DQM3*/
MXC_DCD_ITEM(9, IOMUXC_BASE_ADDR + 0x318, 0x00000038)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_CAS*/
MXC_DCD_ITEM(10, IOMUXC_BASE_ADDR + 0x300, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_RAS*/
MXC_DCD_ITEM(11, IOMUXC_BASE_ADDR + 0x31c, 0x00000038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDCLK_0*/
MXC_DCD_ITEM(12, IOMUXC_BASE_ADDR + 0x338, 0x00000038)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_RESET*/
MXC_DCD_ITEM(13, IOMUXC_BASE_ADDR + 0x320, 0x00000038)

/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDBA2 - DSE can be configured using Group Control
  Register: IOMUXC_SW_PAD_CTL_GRP_CTLDS*/
MXC_DCD_ITEM(14, IOMUXC_BASE_ADDR + 0x32c, 0x00000000)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDODT0*/
MXC_DCD_ITEM(15, IOMUXC_BASE_ADDR + 0x33c, 0x00003038)
/*IOMUXC_SW_PAD_CTL_PAD_DRAM_SDODT1*/
MXC_DCD_ITEM(16, IOMUXC_BASE_ADDR + 0x340, 0x00003038)
/*IOMUXC_SW_PAD_CTL_GRP_B0DS*/
MXC_DCD_ITEM(17, IOMUXC_BASE_ADDR + 0x5c4, 0x00000038)
/*IOMUXC_SW_PAD_CTL_GRP_B1DS*/
MXC_DCD_ITEM(18, IOMUXC_BASE_ADDR + 0x5cc, 0x00000038)
/*IOMUXC_SW_PAD_CTL_GRP_B2DS*/
MXC_DCD_ITEM(19, IOMUXC_BASE_ADDR + 0x5d4, 0x00000038)
/*IOMUXC_SW_PAD_CTL_GRP_B3DS*/
MXC_DCD_ITEM(20, IOMUXC_BASE_ADDR + 0x5d8, 0x00000038)

/*IOMUXC_SW_PAD_CTL_GRP_ADDDS*/
MXC_DCD_ITEM(21, IOMUXC_BASE_ADDR + 0x5ac, 0x00000038)
/*IOMUXC_SW_PAD_CTL_GRP_CTLDS*/
MXC_DCD_ITEM(22, IOMUXC_BASE_ADDR + 0x5c8, 0x00000038)
/*IOMUXC_SW_PAD_CTL_GRP_DDRMODE_CTL*/
MXC_DCD_ITEM(23, IOMUXC_BASE_ADDR + 0x5b0, 0x00020000)
/*IOMUXC_SW_PAD_CTL_GRP_DDRPKE*/
MXC_DCD_ITEM(24, IOMUXC_BASE_ADDR + 0x5b4, 0x00000000)
/*IOMUXC_SW_PAD_CTL_GRP_DDRMODE*/
MXC_DCD_ITEM(25, IOMUXC_BASE_ADDR + 0x5c0, 0x00020000)
/*IOMUXC_SW_PAD_CTL_GRP_DDR_TYPE*/
MXC_DCD_ITEM(26, IOMUXC_BASE_ADDR + 0x5d0, 0x000C0000)

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
/* Data bus width	16*/
/*========================================================================*/
/*MMDC_P0_BASE_ADDR = 0x021b0000*/

/*========================================================================*/
/* Calibration setup.*/
/**/
/*========================================================================*/

/*DDR_PHY_P0_MPZQHWCTRL, enable on time ZQ calibration*/
MXC_DCD_ITEM(27, MMDC_P0_BASE_ADDR + 0x800, 0xa1390003)

/* write leveling*/
MXC_DCD_ITEM(28, MMDC_P0_BASE_ADDR + 0x80C, 0x001F001F)
MXC_DCD_ITEM(29, MMDC_P0_BASE_ADDR + 0x810, 0x001F001F)

/*dqs gating dis*/
MXC_DCD_ITEM(30, MMDC_P0_BASE_ADDR + 0x83c, 0x413C0138)
MXC_DCD_ITEM(31, MMDC_P0_BASE_ADDR + 0x840, 0x00000000)

/*Read calibration*/
MXC_DCD_ITEM(32, MMDC_P0_BASE_ADDR + 0x848, 0x4040464A)

/*Write calibration*/
MXC_DCD_ITEM(33, MMDC_P0_BASE_ADDR + 0x850, 0x40403636)

/*DDR_PHY_P0_MPREDQBY0DL3*/
MXC_DCD_ITEM(34, MMDC_P0_BASE_ADDR + 0x81c, 0x33333333)
/*DDR_PHY_P0_MPREDQBY1DL3*/
MXC_DCD_ITEM(35, MMDC_P0_BASE_ADDR + 0x820, 0x33333333)
/*DDR_PHY_P0_MPREDQBY2DL3*/
MXC_DCD_ITEM(36, MMDC_P0_BASE_ADDR + 0x824, 0x33333333)
/*DDR_PHY_P0_MPREDQBY3DL3*/
MXC_DCD_ITEM(37, MMDC_P0_BASE_ADDR + 0x828, 0x33333333)

/*write delayes:*/
/*all byte 0 data & dm delayed by 3*/
MXC_DCD_ITEM(38, MMDC_P0_BASE_ADDR + 0x82c, 0x00000000)
/*all byte 1 data & dm delayed by 3*/
MXC_DCD_ITEM(39, MMDC_P0_BASE_ADDR + 0x830, 0x00000000)
/*all byte 2 data & dm delayed by 3*/
MXC_DCD_ITEM(40, MMDC_P0_BASE_ADDR + 0x834, 0x00000000)
/*all byte 3 data & dm delayed by 3*/
MXC_DCD_ITEM(41, MMDC_P0_BASE_ADDR + 0x838, 0x00000000)


/* For i.mx6qd parts of versions A & B (v1.0, v1.1), uncomment the following lines. For version C (v1.2), keep commented */
/*setmem /32 0x021b0858 = 0xa00*/
/*clk delay*/

/*fine tune duty cyc to low*/
MXC_DCD_ITEM(42, MMDC_P0_BASE_ADDR + 0x8c0, 0x24911492)

/*frc_msr*/
MXC_DCD_ITEM(43, MMDC_P0_BASE_ADDR + 0x8b8, 0x00000800)

/*========================================================================*/
/* Calibration setup end*/
/*========================================================================*/

/* Channel0 - startng address 0x80000000*/

/*setmem /32	0x021b000c =	0x3f436133*/
/* MMDC0_MDCFG0*/
/*MMDC0_MDPDC - where is tCKSRX and tCKSRE defined in LPDDR2 data sheet?????*/
MXC_DCD_ITEM(44, MMDC_P0_BASE_ADDR + 0x004, 0x00020024)
/*MMDC0_MDOTC*/
MXC_DCD_ITEM(45, MMDC_P0_BASE_ADDR + 0x008, 0x1B333040)
/*MMDC0_MDCFG0*/
MXC_DCD_ITEM(46, MMDC_P0_BASE_ADDR + 0x00c, 0x69435313)
/*MMDC0_MDCFG1*/
MXC_DCD_ITEM(47, MMDC_P0_BASE_ADDR + 0x010, 0xB68E0B63)
/*MMDC0_MDCFG2*/
MXC_DCD_ITEM(48, MMDC_P0_BASE_ADDR + 0x014, 0x01FF00DB)

/*MMDC0_MDMISC. RALAT=3. Try increasing RALAT if case of failures at higher DDR
  freq*/
MXC_DCD_ITEM(49, MMDC_P0_BASE_ADDR + 0x018, 0x00001780)
/*MRW: BA=0 CS=0 MR_ADDR=63 MR_OP=0  //reset*/
MXC_DCD_ITEM(50, MMDC_P0_BASE_ADDR + 0x01c, 0x00008000)
/*MMDC0_MDRWD;*/
MXC_DCD_ITEM(51, MMDC_P0_BASE_ADDR + 0x02c, 0x000026d2)
/*MMDC0_MDOR*/
MXC_DCD_ITEM(52, MMDC_P0_BASE_ADDR + 0x030, 0x00431023)

/*CS0_END = 0x8fffffff*/
MXC_DCD_ITEM(53, MMDC_P0_BASE_ADDR + 0x040, 0x0000004F)

/*MMDC0_MAARCR ADOPT optimized priorities. Dyn jump disabled*/
MXC_DCD_ITEM(54, MMDC_P0_BASE_ADDR + 0x404, 0x00011006)

MXC_DCD_ITEM(55, MMDC_P0_BASE_ADDR + 0x000, 0x84180000)

/* Channel0 : Configure DDR device:*/
/* Megrez note: Device drive strength change might help, consult device/JEDEC
   for the values.*/

/*MRW: BA=0 CS=0 MR_ADDR=63 MR_OP=0  //reset*/
MXC_DCD_ITEM(56, MMDC_P0_BASE_ADDR + 0x01c, 0x04408032)
/*MRW: BA=0 CS=0 MR_ADDR=10 MR_OP=ff /zq*/
MXC_DCD_ITEM(57, MMDC_P0_BASE_ADDR + 0x01c, 0x00008033)
/*MRW: BA=0 CS=0 MR_ADDR=1  MR_OP=c2*/
MXC_DCD_ITEM(58, MMDC_P0_BASE_ADDR + 0x01c, 0x00048031)
/*MRW: BA=0 CS=0 MR_ADDR=2  MR_OP=4. tcl=6, tcwl=3*/
MXC_DCD_ITEM(59, MMDC_P0_BASE_ADDR + 0x01c, 0x05208030)
/*MRW: BA=0 CS=0 MR_ADDR=3  MR_OP=2.drive=240/6*/
MXC_DCD_ITEM(60, MMDC_P0_BASE_ADDR + 0x01c, 0x04008040)

/*######################################################*/
/*final DDR setup, before operation start:*/

/*MMDC0_MDREF*/
MXC_DCD_ITEM(61, MMDC_P0_BASE_ADDR + 0x020, 0x00005800)

/*DDR_PHY_P0_MPODTCTRL*/
/*setmem /32	0x021b0818 =	0*/
MXC_DCD_ITEM(62, MMDC_P0_BASE_ADDR + 0x818, 0x00011117)

/*MMDC0_MDPDC now SDCTL power down enabled*/
MXC_DCD_ITEM(63, MMDC_P0_BASE_ADDR + 0x004, 0x00025564)

/*MMDC0_MDSCR, clear this register (especially the configuration bit as
  initialization is complete)*/
MXC_DCD_ITEM(64, MMDC_P0_BASE_ADDR + 0x01c, 0x00000000)


