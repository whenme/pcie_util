#ifndef __XGPU_CDEV_CONFIG__
#define __XGPU_CDEV_CONFIG__

#include <linux/ioctl.h>

#include "xgpu_mod.h"

static const xgpu_cdev_node config_items[] = {
    {0, 0, 256, AMD_GPU_DEV_NAME "%d/config"},
    {1, PCI_VENDOR_ID, 2, AMD_GPU_DEV_NAME "%d/vendor"},
    {2, PCI_DEVICE_ID, 2, AMD_GPU_DEV_NAME "%d/device"},
    {3, PCI_COMMAND, 2, AMD_GPU_DEV_NAME "%d/command"},
    {4, PCI_STATUS, 2, AMD_GPU_DEV_NAME "%d/status"},
    {5, PCI_REVISION_ID, 1, AMD_GPU_DEV_NAME "%d/revision"},
    {6, PCI_CLASS_PROG, 3, AMD_GPU_DEV_NAME "%d/class"},
};

static const int item_config_max = sizeof(config_items)/sizeof(xgpu_cdev_node);

void cdev_config_init(struct xgpu_cdev *xcdev);

void config_destroy_interfaces(struct xgpu_pci_dev *xpdev);
int config_create_interface(struct xgpu_pci_dev *xpdev);

#endif
