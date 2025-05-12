#ifndef __XGPU_CDEV_REGISTER__
#define __XGPU_CDEV_REGISTER__

#include <linux/ioctl.h>

#include "xgpu_mod.h"
#include "cdev_ctrl.h"

#define AMD_REG_DEV_NAME    "reg"

static const xgpu_cdev_node register_items[] = {
    {item_ifwi_max, 0, 4, AMD_GPU_DEV_NAME "%d/" AMD_REG_DEV_NAME "/register"},
    {item_ifwi_max+1, 0x3788, 4, AMD_GPU_DEV_NAME "%d/" AMD_REG_DEV_NAME "/fbBaseLo"},
    {item_ifwi_max+2, 0x378c, 4, AMD_GPU_DEV_NAME "%d/" AMD_REG_DEV_NAME "/fbBaseHi"},
};

static const int item_register_max = item_ifwi_max + sizeof(register_items)/sizeof(xgpu_cdev_node);

void cdev_register_init(struct xgpu_cdev *xcdev);

#endif
