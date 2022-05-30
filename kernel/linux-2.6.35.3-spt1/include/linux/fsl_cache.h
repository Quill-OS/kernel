/*
 * fsl_cache.h  --  freescale cache driver header file
 *
 * Copyright 2009 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_FSL_CACHE_H
#define _LINUX_FSL_CACHE_H

#include <asm-generic/ioctl.h>

#define FSLCACHE_IOCINV		_IOR('c', 0, struct fsl_cache_addr)
#define FSLCACHE_IOCCLEAN 	_IOR('c', 1, struct fsl_cache_addr)
#define FSLCACHE_IOCFLUSH 	_IOR('c', 2, struct fsl_cache_addr)

struct fsl_cache_addr {
	void *start;
	void *end;
};

#endif
