
#include "cdev_register.h"
#include "xgpu_cdev.h"
#include "libxgpu.h"

static ssize_t char_register_read(struct file *fp, char __user *buf, size_t count,
                                  loff_t *pos)
{
    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    struct pci_dev *pdev = xcdev->xdev->pdev;
    uint32_t val = 0;
    int ret = 0;

    //length <= 4, return ascii hex string
    //length > 4, return binary value
    for (int ii = 0; ii < item_config_max; ii++) {
        if (MINOR(xcdev->cdevno) == register_items[ii].devId) {
            pci_read_config_dword(pdev, register_items[ii].offset, &val);
        }
    }

    return ret;
}

static ssize_t char_register_write(struct file *file, const char __user *buf,
                                   size_t count, loff_t *pos)
{
    return 0;
}

/*
 * character device file operations for control bus (through control bridge)
 */
static const struct file_operations register_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.read = char_register_read,
	.write = char_register_write,
};

void cdev_register_init(struct xgpu_cdev *xcdev)
{
	cdev_init(&xcdev->cdev, &register_fops);
}
