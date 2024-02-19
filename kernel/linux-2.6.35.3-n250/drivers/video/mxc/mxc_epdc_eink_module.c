/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL FREESCALE SEMICONDUCTOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/version.h>
#include "eink_processing2.h"

/*#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,31))*/

EXPORT_SYMBOL(do_aa_processing_v2_2_1);
EXPORT_SYMBOL(do_aa_processing_v2_2_0);
EXPORT_SYMBOL(do_aad_processing_v2_1_1);
EXPORT_SYMBOL(do_aad_processing_v2_1_0);
EXPORT_SYMBOL(set_aad_update_counter);
EXPORT_SYMBOL(mxc_epdc_fb_prep_algorithm_data);
EXPORT_SYMBOL(mxc_epdc_fb_fetch_vc_data);
EXPORT_SYMBOL(mxc_epdc_fb_fetch_wxi_data);
EXPORT_SYMBOL(mxc_epdc_fb_aa_info);
/*#endif*/

MODULE_LICENSE("Proprietary");
