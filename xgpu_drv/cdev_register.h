#ifndef __XGPU_CDEV_REGISTER__
#define __XGPU_CDEV_REGISTER__

#include <linux/ioctl.h>

#include "xgpu_mod.h"
#include "cdev_config.h"

#define AMD_REG_DEV_NAME    "reg"
#define AMD_REG_PATH_NAME   AMD_GPU_DEV_NAME "%d/" AMD_REG_DEV_NAME

static const xgpu_cdev_node register_items[] = {
    {item_config_max, 0, 4, AMD_GPU_DEV_NAME "%d/register"},
    {item_config_max+1, 0x378c, 4, AMD_REG_PATH_NAME "/CfgMemSize"},
    {item_config_max+2, 0xA370, 4, AMD_REG_PATH_NAME "/VmFbLocBase"},
    {item_config_max+3, 0xA374, 4, AMD_REG_PATH_NAME "/VmFbLocTop"},
    {item_config_max+4, 0xA10C, 4, AMD_REG_PATH_NAME "/VmFbOffset"},
    {item_config_max+5, 0xA048, 4, AMD_REG_PATH_NAME "/VmLfbAddrStart"},
    {item_config_max+6, 0xA04C, 4, AMD_REG_PATH_NAME "/VmLfbAddrEnd"},
};

static const int item_register_max = item_config_max + sizeof(register_items)/sizeof(xgpu_cdev_node);

void cdev_register_init(struct xgpu_cdev *xcdev);
void cdev_register_exit(struct xgpu_cdev *xcdev);

#endif
