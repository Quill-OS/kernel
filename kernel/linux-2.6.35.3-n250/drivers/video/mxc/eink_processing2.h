/*
 * Copyright (C) 2012 - 2013 Freescale Semiconductor, Inc. All Rights Reserved.
 * THIS SOURCE CODE IS CONFIDENTIAL AND PROPRIETARY AND MAY NOT
 * BE USED OR DISTRIBUTED WITHOUT THE WRITTEN PERMISSION OF
 * Freescale Semiconductor, Inc.
 */

#ifndef __EINK_PROCESSING_H__
#define __EINK_PROCESSING_H__

#include <linux/mxcfb.h>

#define WAVEFORM_SET_ALGO_DATA	1
#define WAVEFORM_RESTORE_DEFAULT_PMM	2

/*
 * select a version according to SoCs on the platform
 * i.MX6SL has Cortex A9 core
 * i.MX508 has Cortex A8 core
 * The algorithms are not compatible
 * so use care to select proper lib.1_shipped for the correct core
 * for the target platform.
 */
#undef IMX6SL_AA
#define IMX508_AA

/*
 * E-ink Gen II waveform algorithm aa implementation - version 2.2.1
 *
 * Function Name:
 *	int do_aa_processing ( .. )
 * input parameters:
 * 	unsigned short *working_buffer_ptr_in,
 *	unsigned short *working_buffer_ptr_out,
 *	struct mxcfb_rect *update_region,
 *	long		working_buffer_width
 *	long		working_buffer_height
 * return:
 *	0 - normal exit
 *	-1 - no advance algorithm
 */

int do_aa_processing_v2_2_1(
	unsigned short *working_buffer_ptr_in,
	unsigned short *working_buffer_ptr_out,
	struct mxcfb_rect *update_region,
	long working_buffer_width,
	long working_buffer_height);

/*
 * E-ink Gen II waveform algorithm AA-D implementation version 2.1.1
 * for i.MX50 (508: 86ms for 1024x768, 52ms for 800x600)
 * Function Name:
 *	int do_aad_processing ( .. )
 * input parameters:
 * 	unsigned short *working_buffer_ptr_in,
 *	unsigned short *working_buffer_ptr_out,
 *	struct mxcfb_rect *update_region,
 *	long	working_buffer_width
 *	long	working_buffer_height
 * return:
 *	0 - normal exit
 *	-1 - no advance algorithm
 */

int do_aad_processing_v2_1_1(
	unsigned short *working_buffer_ptr_in,
	unsigned short *working_buffer_ptr_out,
	struct mxcfb_rect *update_region,
	long working_buffer_width,
	long working_buffer_height);
/*
 * E-ink Gen II waveform algorithm aa implementation - version 2.2.0
 *
 * Function Name:
 *	int do_aa_processing( .. )
 * input parameters:
 *	unsigned char*	update_region_ptr
 *	struct mxcfb_rect*	update_region
 *	unsigned long		update_region_stride
 *	unsigned short*	working_buffer_ptr
 *	long			working_buffer_width
 *	long			working_buffer_height
 * output:
 *	unsigned char*	update_region_ptr
 * return:
 *	0 - normal exit
 *	-1 - no advance algorithm
 */

int do_aa_processing_v2_2_0(
	unsigned char *update_region_ptr, struct mxcfb_rect *update_region,
	unsigned long update_region_stride, unsigned short *working_buffer_ptr,
	long working_buffer_width, long working_buffer_height);


/*
 * E-ink Gen II waveform algorithm AA-D implementation version 2.1.0
 * Function Name:
 *	int do_aad_processing( .. )
 * input parameters:
 *	unsigned char*	update_region_ptr
 *	struct mxcfb_rect*	update_region
 *	unsigned long		update_region_stride
 *	unsigned short*	working_buffer_ptr
 *	long			working_buffer_width
 *	long			working_buffer_height
 * output parameters:
 *	unsigned char*	update_region_ptr
 * return:
 *	0 - normal exit
 *	-1 - no advance algorithm
 */

int do_aad_processing_v2_1_0(
	unsigned char *update_region_ptr, struct mxcfb_rect *update_region,
	unsigned long update_region_stride, unsigned short *working_buffer_ptr,
	long working_buffer_width, long working_buffer_height);

/* 
 * API for setting the update counter
 * Function Name:
 *	void set_aad_update_counter( .. )
 * input parameters:
 *	unsigned long	value
 * return:
 *	none
 */
void set_aad_update_counter(unsigned long value);

/* 
 * Function for fetching the voltage control data for EPDC PMIC
 * name: int mxc_epdc_fb_fetch_vc_data ( .. )
 * return: 0 - normal exit, -1 - invalide waveform mode or invalid temperature range, -2 - data checksum error
 * parameters: *waveform_vcd_buffer holds the voltage control data table for waveform modes, typically fb_data->waveform_vcd_buffer
 * 		 wMode - waveform mode for the interested vc data
 *		 wTempRange - specific temperature range of the waveform mode for vc data
 *		 waveform_mc - the number of waveform modes in the waveform file, typically it is fb_data-waveform_mc
 *		 waveform_trc - the number of temperature ranges in each waveform mode, typically fb_data->waveform_trc
 *		 *VoltageCtrlData - 16-byte data array for returning vc data
 */
int mxc_epdc_fb_fetch_vc_data( unsigned char *waveform_vcd_buffer, 
		unsigned wMode,
		unsigned wTempRange,
		unsigned waveform_mc,
		unsigned waveform_trc,
		unsigned char *VoltageCtrlData);

/*
 * function for extracting the extra waveform information data (usually the waveform name)
 * name: int mxc_epdc_fb_fetch_wxi_data(( .. )
 * return: number of bytes in the extra information
 * parameters: *waveform_xwi_buffer holds the extra waveform data, typically fb_data->waveform_xwi_buffer
 *		 *ExtraWaveformString - NULL if no buffer allocated, the function will return the number of bytes in string
 *			and it can be used for allocating the string buffer
 * Note: typically fetching the extra waveform information requires two function calls.
 * The first call with NULL pointer to get the length of the information then allocate a string buffer with one byte larger than the size of the xwi
 * The second call with the allocated buffer to get the data then set the null termination for completing the string.
 */
int mxc_epdc_fb_fetch_wxi_data( unsigned char *waveform_xwi_buffer, 
		char *ExtraWaveformString);
		
/* 
 * Function for preparing the advance algorithm control data
 * name: int mxc_epdc_fb_prep_algorithm_data( ( .. )
 * return: 	0 - no advance algorithm data
 *		1 - version 1 algorithm data (AA algorithm)
 *		2 - version 2 algorithm data (AA/AA-D)
 *		-1 - no advance waveform support algorithms in this version of library
 *		-2 - invalid waveform mode or invalid temperature range
 *		< -2 values - data checksum errors
 * parameters: *waveform_acd_buffer holds the algorithm control data table for waveform modes, typically fb_data->waveform_acd_buffer
 * 		 wMode - waveform mode for the interested vc data
 *		 wTempRange - specific temperature range of the waveform mode for vc data
 *		 waveform_mc - the number of waveform modes in the waveform file, typically it is fb_data-waveform_mc
 *		 waveform_trc - the number of temperature ranges in each waveform mode, typically fb_data->waveform_trc
 *		 waveform_magic_number - algorithm data code from the waveform file, typically fb_data->waveform_magic_number
 *		 ctrlFlags -	0 just look up the version of the algorithm data, 
 *				1 - transfer the data to be used by algorithm if defined,
 *				3 - same as 1 but restore the default data if there is no algorithm data.
 */
int mxc_epdc_fb_prep_algorithm_data( unsigned char *waveform_acd_buffer, 
		unsigned wMode,
		unsigned wTempRange,
		unsigned waveform_mc,
		unsigned waveform_trc,
		unsigned waveform_magic_number,
		unsigned ctrlFlags);	

/* Function about advance algoritms in this device driver
 * name: int mxc_epdc_fb_aa_info( void )
 * return: number of advance algorithms built into this device driver
 * parameters: none
 */
int mxc_epdc_fb_aa_info( void );

#endif
