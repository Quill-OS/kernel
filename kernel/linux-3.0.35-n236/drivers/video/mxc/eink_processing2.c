

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mxcfb.h>
#include <linux/mxcfb_epdc_kernel.h>


int do_aa_processing_v2_2_1(
	unsigned short *working_buffer_ptr_in,
	unsigned short *working_buffer_ptr_out,
	struct mxcfb_rect *update_region,
	long working_buffer_width,
	long working_buffer_height)
{
	return -1;
}
int do_aad_processing_v2_1_1(
	unsigned short *working_buffer_ptr_in,
	unsigned short *working_buffer_ptr_out,
	struct mxcfb_rect *update_region,
	long working_buffer_width,
	long working_buffer_height)
{
	return -1;
}
int do_aa_processing_v2_2_0(
	unsigned char *update_region_ptr, struct mxcfb_rect *update_region,
	unsigned long update_region_stride, unsigned short *working_buffer_ptr,
	long working_buffer_width, long working_buffer_height)
{
	return -1;
}
int do_aad_processing_v2_1_0(
	unsigned char *update_region_ptr, struct mxcfb_rect *update_region,
	unsigned long update_region_stride, unsigned short *working_buffer_ptr,
	long working_buffer_width, long working_buffer_height)
{
	return -1;
}
void set_aad_update_counter(unsigned long value)
{
	return ;
}

int mxc_epdc_fb_prep_algorithm_data( unsigned char *waveform_acd_buffer, 
		unsigned wMode,
		unsigned wTempRange,
		unsigned waveform_mc,
		unsigned waveform_trc,
		unsigned waveform_magic_number,
		unsigned ctrlFlags)
{
	return 0;
}

/* function for handing the extra waveform information */
int mxc_epdc_fb_fetch_wxi_data( unsigned char *waveform_xwi_buffer, char *xwiBuffer)
{
	if (xwiBuffer != NULL) {
		memcpy(xwiBuffer, &waveform_xwi_buffer[1], (size_t) waveform_xwi_buffer[0]);
	}
	return (int) waveform_xwi_buffer[0];
}

/* function for copying the voltage control data to the allocated buffer */
int mxc_epdc_fb_fetch_vc_data( unsigned char *waveform_vcd_buffer,
	unsigned wMode,
	unsigned wTempRange,
	unsigned waveform_mc,
	unsigned waveform_trc,
	unsigned char *VoltageCtrlData)
{
	if ((wMode >= waveform_mc) || (wTempRange >= waveform_trc))
		return -1;
	/* copy the waveform voltage control data */
	memcpy(VoltageCtrlData, waveform_vcd_buffer + 16 * (wMode * waveform_trc + wTempRange),16);
	{
		unsigned i;
		unsigned char cs = 0;
		for (i=0; i< 15; i++)
			cs += VoltageCtrlData[i];
		if (cs != VoltageCtrlData[15])  return -1;
	}
	return 0;
}

 

/* About advance algorithms in this device driver */

int mxc_epdc_fb_aa_info( void )
{
#if REAGL_ENABLED
	printk(" This version contains GLR16 and GLD16 advance algorithms.\n");
	return 2;
#else
	printk(" This version contains no advance algorithm.\n");
	return 0;
#endif
}

