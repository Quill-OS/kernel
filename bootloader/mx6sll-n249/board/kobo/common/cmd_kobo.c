// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2021
 * Andreas Kemnade, andreas@kemnade.info
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <mapmem.h>

#include <mmc.h>
#include <env.h>
#include <blk.h>

static struct mmc *init_mmc_device(int dev, bool force_init)
{
        struct mmc *mmc;
        mmc = find_mmc_device(dev);
        if (!mmc) {
                printf("no mmc device at slot %x\n", dev);
                return NULL;
        }

        if (!mmc_getcd(mmc))
                force_init = true;

        if (force_init)
                mmc->has_init = 0;
        if (mmc_init(mmc))
                return NULL;

#ifdef CONFIG_BLOCK_CACHE
        struct blk_desc *bd = mmc_get_blk_desc(mmc);
        blkcache_invalidate(bd->if_type, bd->devnum);
#endif

        return mmc;
}

static int do_kobohiddenread(struct cmd_tbl *cmdtp, int flag,
			     int argc, char * const argv[])
{
	struct mmc *mmc;
	u32 blk, cnt, devnum, n;
	u8 *addr;
	const u8 magic[4] = {0xff, 0xf5, 0xaf, 0xff};

	if ((argc != 4) && (argc != 5))
		return CMD_RET_USAGE;

	devnum = simple_strtoul(argv[1], NULL, 16);
	addr = (void *)simple_strtoul(argv[2], NULL, 16);
	blk = simple_strtoul(argv[3], NULL, 16);
	
	mmc = init_mmc_device(devnum, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (blk < 1)
		return CMD_RET_FAILURE;

	n = blk_dread(mmc_get_blk_desc(mmc), blk - 1, 1, addr);
	if (n < 1)
		return CMD_RET_FAILURE;
	
	if (memcmp(addr + 512 -16, magic, sizeof(magic))) {
		printf("invalid magic at blk - 1\n");
		return CMD_RET_FAILURE;
	}
	
	cnt = addr[512 - 5];
	cnt <<= 8;
	cnt |= addr[512 - 6];
	cnt <<= 8;
	cnt |= addr[512 - 7];
	cnt <<= 8;
	cnt |= addr[512 - 8];
	
	if (argc == 5) {
		env_set_ulong(argv[4], cnt);
	}
	cnt = (cnt + 511) / 512;
	printf("reading %x blocks from %x\n", cnt, blk);
	n = blk_dread(mmc_get_blk_desc(mmc), blk, cnt, addr);
	if (n != cnt) {
		printf("got only %x blocks\n", n);
		return CMD_RET_FAILURE;
	}

	return 0;
}

U_BOOT_CMD(
	kobohiddenread, 5, 0, do_kobohiddenread,
	"read kobo hidden partitions",
	"kobo <mmcnum> <addr> <blk> [lengthname]"
);

static int do_download_mode(struct cmd_tbl *cmdtp, int flag, int argc,
                     char *const argv[])

{
	uint32_t *start = map_sysmem(0x20d8040, 8);
	start[0] = 0x30;
	start[1] = 0x10000000;
	unmap_sysmem(start);

	do_reset(NULL, 0, 0, NULL);

	return 0;
}

U_BOOT_CMD(
        download_mode, 1, 0,    do_download_mode,
        "Go to mfg download mode",
        ""
);
