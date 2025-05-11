
#ifndef __XGPU_CDEV_H__
#define __XGPU_CDEV_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#include "xgpu_mod.h"

#define XGPU_MINOR_COUNT (1024)

int  xgpu_cdev_init(void);
void xgpu_cdev_cleanup(void);

int  char_open(struct inode *inode, struct file *file);
int  char_close(struct inode *inode, struct file *file);
int xcdev_check(const char *fname, struct xgpu_cdev *xcdev, bool check_engine);
void cdev_ctrl_init(struct xgpu_cdev *xcdev);

int  xpdev_create_interfaces(struct xgpu_pci_dev *xpdev);
void xpdev_destroy_interfaces(struct xgpu_pci_dev *xpdev);

int create_xcdev(struct xgpu_pci_dev *xpdev, struct xgpu_cdev *xcdev,
                 int bar, int type);
int destroy_xcdev(struct xgpu_cdev *cdev);

#endif   /* __XGPU_CDEV_H__ */