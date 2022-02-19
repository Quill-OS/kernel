/*
 * Copyright 2008 - 2009 Windriver, <www.windriver.com>
 * Author: Tom Rix <Tom.Rix@windriver.com>
 *
 * (C) Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <console.h>
#include <g_dnl.h>
#include <usb.h>

#include <timer.h>

static int (*gpfn_fastboot_abort_check_fn)(void) ; // return 1 if user want to abort fastboot loop .
int fastboot_connection_abortchk_setup(int (*fastboot_abort_check_fn)(void))
{
	gpfn_fastboot_abort_check_fn = fastboot_abort_check_fn;
	return 0;
}


static int (*gpfn_fastboot_abort_at_usb_remove_chk_fn)(void) ; // return 1 if user want to abort fastboot loop .
int fastboot_connection_abort_at_usb_remove_chk_setup(int (*fastboot_abort_at_usb_remove_chk_fn)(void))
{
	gpfn_fastboot_abort_at_usb_remove_chk_fn = fastboot_abort_at_usb_remove_chk_fn;
	return 0;
}


unsigned long gdwFastboot_connection_timeout_us=0;
unsigned long long gu64_Fastboot_connection_timeout_tick;

unsigned long fastboot_connection_timeout_us_set(unsigned long dwTimeoutSetUS)
{
	unsigned long dwFBTimeoutOld=gdwFastboot_connection_timeout_us;
	uint64_t _Ticks;
	extern uint64_t usec_to_tick(unsigned long usec);

	if(0==dwTimeoutSetUS) {
		gdwFastboot_connection_timeout_us = 0;
		gu64_Fastboot_connection_timeout_tick=0;
		//printf("%s() reset timeout !!\n",__FUNCTION__);
	}
	else {
		_Ticks = usec_to_tick(dwTimeoutSetUS);
		gu64_Fastboot_connection_timeout_tick = get_ticks() + _Ticks;
		gdwFastboot_connection_timeout_us = dwTimeoutSetUS;
		//printf("%s() timeout tick =%llu!!\n",__FUNCTION__,gu64_Fastboot_connection_timeout_tick);
	}

	return dwFBTimeoutOld;
}

int fastboot_connection_check_timeouted(void)
{
	unsigned long long u64_current_tick;
	if(0==gu64_Fastboot_connection_timeout_tick) {
		//printf("%s() timeout tick=0\n",__FUNCTION__);
		return 0;
	}

	u64_current_tick = get_ticks();
	if(u64_current_tick>=gu64_Fastboot_connection_timeout_tick) {
		printf("%s() timeout occured !!\n",__FUNCTION__);
		return 1;
	}
	else {
		//printf("%s() current tick=%llu<%llu !!\n",__FUNCTION__,u64_current_tick,gu64_Fastboot_connection_timeout_tick);
		return 0;
	}
}


extern int RC5T619_watchdog(int I_iCmd,int I_iTimeoutsecs);

static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int controller_index;
	char *usb_controller;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	RC5T619_watchdog(0,0);

	fastboot_flash_dump_ptn();

	usb_controller = argv[1];
	controller_index = simple_strtoul(usb_controller, NULL, 0);

	ret = board_usb_init(controller_index, USB_INIT_DEVICE);
	if (ret) {
		error("USB init failed: %d", ret);
		return CMD_RET_FAILURE;
	}

	g_dnl_clear_detach();
	ret = g_dnl_register("usb_dnl_fastboot");
	if (ret)
		return ret;

	if (!g_dnl_board_usb_cable_connected()) {
		puts("\rUSB cable not detected.\n" \
		     "Command exit.\n");
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	while (1) {
		if (g_dnl_detach())
			break;
		if (ctrlc())
			break;

		if(gpfn_fastboot_abort_check_fn && gpfn_fastboot_abort_check_fn()) {
			printf("fastboot abort !\n");
			break;
		}
		if(fastboot_connection_check_timeouted()) {
			printf("fastboot connection timeouted !!!\n");
			break;
		}

		usb_gadget_handle_interrupts(controller_index);
	}

	ret = CMD_RET_SUCCESS;

exit:
	g_dnl_unregister();
	g_dnl_clear_detach();
	board_usb_cleanup(controller_index, USB_INIT_DEVICE);

	return ret;
}

U_BOOT_CMD(
	fastboot, 2, 1, do_fastboot,
	"use USB Fastboot protocol",
	"<USB_controller>\n"
	"    - run as a fastboot usb device"
);
