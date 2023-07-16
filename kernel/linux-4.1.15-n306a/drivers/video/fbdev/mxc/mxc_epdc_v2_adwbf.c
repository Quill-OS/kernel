/*
 * Copyright 2016-2017 NXP Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup Advanced waveform buffer processing routines.
 */

/*!
 * @file mxc_epdc_v2_adwbf.c
 *
 * @brief MXC EPDC V2 driver extension
 *
 * @ingroup Framebuffer
 */

/* Algorithm control data version defines */
#define WFM_ACD_VER_REAGL				1
#define WFM_ACD_VER_REAGLD				2
#define WFM_ACD_VER_ECLIPSE				6


static u8 wbf_checksum(u8 * data, u32 length)
{
	u8 cs = 0;
	int i = 0;

	for (i = 0; i < length; i++) {
		cs += data[i];
	}
	return cs;
}

static int epdc_parse_awf_data(struct mxc_epdc_fb_data *fb_data,
					struct waveform_data_header *wv_header,
					u8 * wf_buffer)
{
	int wfm_mode_count;

	u64 longOffset;		// Address of the first waveform
	u64 xwiOffset = 0;
	u64 avcOffset = 0;
	u64 tempOffset = 0;

	wfm_mode_count = wv_header->mc + 1;

	longOffset = ((uint64_t *) wf_buffer)[0];
	if (longOffset <= (sizeof(u64) * wfm_mode_count))
		goto error;

	if (wv_header->awf > 3)
		goto error;

	if (wv_header->xwia > 0)
		xwiOffset =
		    ((uint64_t *) wf_buffer)[wfm_mode_count + wv_header->awf];
	else
		xwiOffset = fb_data->waveform_buffer_size;

	dev_info(fb_data->dev, "long %llx xswi %llx\n", longOffset, xwiOffset);

	switch (wv_header->awf) {
	case 0:
		/* No voltage control information */
		break;
	case 1:
		/* Voltage control format V2 */
		avcOffset = ((uint64_t *) wf_buffer)[wfm_mode_count];
		tempOffset = ((uint64_t *) wf_buffer)[wfm_mode_count + 1];
		if (!fb_data->waveform_vcd_buffer)
			fb_data->waveform_vcd_buffer =
			    kmalloc((tempOffset - avcOffset), GFP_KERNEL);

		if (!fb_data->waveform_vcd_buffer) {
			dev_err(fb_data->dev,
				"No memory to allocate VCD buffer\n");
			goto error;
		}

		memcpy((void *)fb_data->waveform_vcd_buffer,
		       &wf_buffer[avcOffset], (tempOffset - avcOffset));
		break;
	case 2:
		/* Algorithm Control information only */
		tempOffset = ((uint64_t *) wf_buffer)[wfm_mode_count];
		fb_data->waveform_magic_number =
		    ((uint64_t *) wf_buffer)[wfm_mode_count + 1];
		/* algorithm control data */
		if (!fb_data->waveform_acd_buffer)
			fb_data->waveform_acd_buffer =
			    kmalloc((xwiOffset - tempOffset), GFP_KERNEL);

		if (!fb_data->waveform_acd_buffer) {
			dev_err(fb_data->dev,
				"No memory to allocate ACD buffer\n");
			goto error;
		}

		memcpy(fb_data->waveform_acd_buffer, &wf_buffer[tempOffset],
		       (xwiOffset - tempOffset));

		break;
	case 3:
		/* Voltage Control and Algorithm Control */
		avcOffset = ((uint64_t *) wf_buffer)[wfm_mode_count];
		tempOffset = ((uint64_t *) wf_buffer)[wfm_mode_count + 1];
		fb_data->waveform_magic_number =
		    ((uint64_t *) wf_buffer)[wfm_mode_count + 2];
		/* algorithm control data */
		if (!fb_data->waveform_acd_buffer)
			fb_data->waveform_acd_buffer =
			    kmalloc((xwiOffset - tempOffset), GFP_KERNEL);

		if (!fb_data->waveform_acd_buffer) {
			dev_err(fb_data->dev,
				"No memory to allocate ACD buffer\n");
			goto error;
		}

		memcpy(fb_data->waveform_acd_buffer, &wf_buffer[tempOffset],
		       (xwiOffset - tempOffset));
		dev_info(fb_data->dev,
			 "copy (%llx - %llx) byte @ offset %llx to acd buffer\n",
			 xwiOffset, tempOffset, tempOffset);

		if (!fb_data->waveform_vcd_buffer)
			fb_data->waveform_vcd_buffer =
			    kmalloc((tempOffset - avcOffset), GFP_KERNEL);

		if (!fb_data->waveform_vcd_buffer) {
			dev_err(fb_data->dev,
				"No memory to allocate VCD buffer\n");
			goto error;
		}

		memcpy((void *)fb_data->waveform_vcd_buffer,
		       &wf_buffer[avcOffset], (tempOffset - avcOffset));
		dev_info(fb_data->dev,
			 "copy (%llx - %llx) byte @ offset %llx to vcd buffer\n",
			 tempOffset, avcOffset, avcOffset);
		break;
	default:
		dev_err(fb_data->dev,
			"Illegal advanced_wfm_flag value %d in waveform\n",
			wv_header->awf);
		goto error;
	}

	fb_data->waveform_mc = wfm_mode_count;
	fb_data->waveform_trc = wv_header->trc + 1;

	if (fb_data->waveform_vcd_buffer) {
		int size0, size1;

		size0 = wfm_mode_count * (wv_header->trc + 1) * 16;
		size1 = tempOffset - avcOffset;
		if (size0 != size1)
			dev_err(fb_data->dev,
				"vcd buffer size does not match 0x%x - 0x%x\n",
				size0, size1);

		dev_info(fb_data->dev,
			 "Retrieved voltage control information (0x%x)\n",
			 size1);
	}
	return 0;
error:
	if (fb_data->waveform_acd_buffer != NULL)
		kfree(fb_data->waveform_acd_buffer);
	if (fb_data->waveform_vcd_buffer != NULL)
		kfree(fb_data->waveform_vcd_buffer);
	return -1;
}

static int epdc_get_adc_version(struct mxc_epdc_fb_data *fb_data, u32 w_mode,
				  u32 w_temp_range, u32* adth_addr)
{
	unsigned ret = 0;

	struct wbf_addr {
		unsigned int addr:24;
		unsigned int cs:8;
	} *adta;

	/* algorithm data Table header */
	struct wbf_adth {
		unsigned int version:16;
		unsigned int np:8;
		unsigned int cs:8;
	} *adth;

	if ((w_mode >= fb_data->waveform_mc)
	    || (w_temp_range >= fb_data->waveform_trc)) {
		return -1;
	}
	/* check algorithm control data */
	adta =
	    (struct wbf_addr *)(fb_data->waveform_acd_buffer +
				(4 *
				 (w_mode * fb_data->waveform_trc +
				  w_temp_range)));

	/* advance algorithm data defined */
	if (adta->addr) {
		u8 *algorithmDataTable =
		    fb_data->waveform_acd_buffer +
		    (4 * fb_data->waveform_mc * fb_data->waveform_trc);

		adth =
		    (struct wbf_adth *)(algorithmDataTable + adta->addr -
					fb_data->waveform_magic_number);

		if (wbf_checksum((u8 *) adth, 3) != adth->cs)
			return -4;

		ret = adth->version;
		*adth_addr = (u32) (algorithmDataTable + adta->addr -
					fb_data->waveform_magic_number);


	}

	return ret;
}

static int fetch_epdc_adc_data(struct mxc_epdc_fb_data *fb_data,
				u32 w_mode,
				u32 w_temp_range,
				u32 adth_addr)
{
	unsigned ret = 0;
	struct pxp_proc_data *proc_data = &fb_data->pxp_conf.proc_data;
	struct wbf_addr {
		unsigned int addr:24;
		unsigned int cs:8;
	} *adp;

	/* algorithm data Table header */
	struct wbf_adth {
		unsigned int version:16;
		unsigned int np:8;
		unsigned int cs:8;
	} *adth;
	u8* algorithmDataTable =
		    fb_data->waveform_acd_buffer +
		    (4 * fb_data->waveform_mc * fb_data->waveform_trc);
	u32 adte = 0;

	adth = (struct wbf_adth *)adth_addr;
	/* init ASV & SFT with default values */
	proc_data->reagl_sft = 2;
	proc_data->reagl_asv = 0;

	if ((adth->version == WFM_ACD_VER_REAGL)
		|| (adth->version == WFM_ACD_VER_ECLIPSE)) {

		while (adte++ < adth->np) {
			adp =
			    (struct wbf_addr *)(adth_addr +
						4 * adte);
			/* run checksum test */
			if (wbf_checksum((u8 *) adp, 3) != adp->cs)
				return -EFAULT;

			/* get the algorithm data */
			{
				u8 *algorithmData = (u8*) (algorithmDataTable +
						       adp->addr -
						       fb_data->waveform_magic_number);
				u16 nd = *((u16 *)algorithmData);


				/* run checksum */
				if (wbf_checksum(algorithmData, nd + 2)
				    != algorithmData[nd + 2])
					return -EFAULT;

				/* parse algorithm data get SFT & ASV needed for Eclipse (version 6) */
				switch (adte) {
				case 1:
					proc_data->reagl_sft = algorithmData[2];
					break;
				case 2:
					proc_data->reagl_asv = algorithmData[2];
					break;
				default:	/* invalid */
					break;
				}
			}	/* end of get algorithm data .. */
		}	/* end of while ( ... ) */
	}

	return ret;
}

