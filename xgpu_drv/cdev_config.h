#ifndef __XGPU_CDEV_CONFIG__
#define __XGPU_CDEV_CONFIG__

#include <linux/ioctl.h>

#include "xgpu_mod.h"

#define BASE_DEV_CONFIG    0

static const xgpu_cdev_node config_items[] = {
    {(BASE_DEV_CONFIG<<16)+0, 0, 256, AMD_GPU_DEV_NAME "%d/config"},
    {(BASE_DEV_CONFIG<<16)+1, 0, 2,   AMD_GPU_DEV_NAME "%d/vendor"},
    {(BASE_DEV_CONFIG<<16)+2, 2, 2,   AMD_GPU_DEV_NAME "%d/device"},
    {(BASE_DEV_CONFIG<<16)+3, 4, 2,   AMD_GPU_DEV_NAME "%d/command"},
    {(BASE_DEV_CONFIG<<16)+4, 6, 2,   AMD_GPU_DEV_NAME "%d/status"},
    {(BASE_DEV_CONFIG<<16)+5, 8, 1,   AMD_GPU_DEV_NAME "%d/revision"},
    {(BASE_DEV_CONFIG<<16)+6, 9, 3,   AMD_GPU_DEV_NAME "%d/class"},
};


void cdev_config_init(struct xgpu_cdev *xcdev);

void config_destroy_interfaces(struct xgpu_pci_dev *xpdev);
int config_create_interface(struct xgpu_pci_dev *xpdev);

#endif
