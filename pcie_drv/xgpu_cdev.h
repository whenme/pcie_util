
#ifndef __GPU_CDEV_H__
#define __GPU_CDEV_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#include "xgpu_mod.h"

#define XGPU_MINOR_BASE  (0)
#define XGPU_MINOR_COUNT (255)

int  xgpu_cdev_init(void);
void xgpu_cdev_cleanup(void);

int  xgpu_open(struct inode *node, struct file *file);
int  xgpu_close(struct inode *inode, struct file *file);
void cdev_ctrl_init(struct xgpu_cdev *xcdev);

int  xpdev_create_interfaces(struct xgpu_pci_dev *xpdev);
void xpdev_destroy_interfaces(struct xgpu_pci_dev *xpdev);

#endif