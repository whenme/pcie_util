/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef _XGPU_IOCALLS_POSIX_H_
#define _XGPU_IOCALLS_POSIX_H_

#include <linux/ioctl.h>

#include "xgpu_mod.h"

/* Use 'x' as magic number */
#define XDMA_IOC_MAGIC	'x'

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 *
 * _IO(type,nr)		    no arguments
 * _IOR(type,nr,datatype)   read data from driver
 * _IOW(type,nr.datatype)   write data to driver
 * _IORW(type,nr,datatype)  read/write data
 *
 * _IOC_DIR(nr)		    returns direction
 * _IOC_TYPE(nr)	    returns magic
 * _IOC_NR(nr)		    returns number
 * _IOC_SIZE(nr)	    returns size
 */

enum XDMA_IOC_TYPES {
	XDMA_IOC_NOP,
	XDMA_IOC_INFO,
	XDMA_IOC_OFFLINE,
	XDMA_IOC_ONLINE,
	XDMA_IOC_MAX
};

struct xgpu_ioc_base {
	unsigned int magic;
	unsigned int command;
};

struct xgpu_ioc_info {
	struct xgpu_ioc_base	base;
	unsigned short		vendor;
	unsigned short		device;
	unsigned short		subsystem_vendor;
	unsigned short		subsystem_device;
	unsigned int		dma_engine_version;
	unsigned int		driver_version;
	unsigned long long	feature_id;
	unsigned short		domain;
	unsigned char		bus;
	unsigned char		dev;
	unsigned char		func;
};

/* IOCTL codes */
#define XDMA_IOCINFO		_IOWR(XDMA_IOC_MAGIC, XDMA_IOC_INFO, \
					struct xgpu_ioc_info)
#define XDMA_IOCOFFLINE		_IO(XDMA_IOC_MAGIC, XDMA_IOC_OFFLINE)
#define XDMA_IOCONLINE		_IO(XDMA_IOC_MAGIC, XDMA_IOC_ONLINE)

#define IOCTL_XDMA_ADDRMODE_SET	_IOW('q', 4, int)
#define IOCTL_XDMA_ADDRMODE_GET	_IOR('q', 5, int)
#define IOCTL_XDMA_ALIGN_GET	_IOR('q', 6, int)

#define IFWI_MAX_SIZE           64

typedef struct _ifwi_cdev_node_ {
    enum cdev_type dev_type;
    uint32_t       offset, length;
    char           devnode_name[32];
}ifwi_cdev_node;

static const ifwi_cdev_node ifwi_items[] = {
    {CHAR_ASIC,    0,  10, AMD_GPU_DEV_NAME "%d/" AMD_IFWI_DEV_NAME "/asic"},
    {CHAR_VERSION, 10, 6,  AMD_GPU_DEV_NAME "%d/" AMD_IFWI_DEV_NAME "/version"},
    {CHAR_SKU,     16, 8,  AMD_GPU_DEV_NAME "%d/" AMD_IFWI_DEV_NAME "/sku"}
};

void ifwi_destroy_interfaces(struct xgpu_pci_dev *xpdev);
int ifwi_create_interface(struct xgpu_pci_dev *xpdev);

#endif /* _XGPU_IOCALLS_POSIX_H_ */
