/*
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

/* 2011/2/24 FY11 : Modified EPD timing. */
static struct fb_videomode c032_mode = {
	.name = "C032",
	.refresh = 85,
	.xres = 800,
	.yres = 600,
	.pixclock = 30000000,
	.left_margin = 8,
	.right_margin = 164,
	.upper_margin = 4,
	.lower_margin = 8,
	.hsync_len = 4,
	.vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};


static struct mxc_epdc_fb_mode panel_modes[] = {
	{
		&c032_mode,
		4,	/* vscan_holdoff */
		10,	/* sdoed_width */
		20,	/* sdoed_delay */
		10,	/* sdoez_width */
		20,	/* sdoez_delay */
		465,	/* gdclk_hp_offs */
		20,	/* gdsp_offs */
		0,	/* gdoe_offs */
		9,	/* gdclk_offs */
		1,	/* num_ce */
	}
};


