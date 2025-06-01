
#ifndef _XGPU_CDEV_CTRL_H_
#define _XGPU_CDEV_CTRL_H_

#include <linux/ioctl.h>

#include "xgpu_mod.h"
#include "cdev_register.h"

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
#define XDMA_IOCINFO    _IOWR(XDMA_IOC_MAGIC, XDMA_IOC_INFO, struct xgpu_ioc_info)
#define XDMA_IOCOFFLINE _IO(XDMA_IOC_MAGIC, XDMA_IOC_OFFLINE)
#define XDMA_IOCONLINE  _IO(XDMA_IOC_MAGIC, XDMA_IOC_ONLINE)

#define AMD_IFWI_DEV_NAME    "ifwi"

static const xgpu_cdev_node ifwi_items[] = {
    {item_register_max, 0,  10, AMD_GPU_DEV_NAME "%d/" AMD_IFWI_DEV_NAME "/asic"},
    {item_register_max+1, 10, 6,  AMD_GPU_DEV_NAME "%d/" AMD_IFWI_DEV_NAME "/version"},
    {item_register_max+2, 16, 8,  AMD_GPU_DEV_NAME "%d/" AMD_IFWI_DEV_NAME "/sku"}
};

static const int item_ifwi_max = item_register_max + sizeof(ifwi_items)/sizeof(xgpu_cdev_node);

#endif /* _XGPU_IOCALLS_POSIX_H_ */
