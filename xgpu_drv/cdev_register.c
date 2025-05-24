
#include "cdev_register.h"
#include "xgpu_cdev.h"
#include "libxgpu.h"

static ssize_t char_register_read(struct file *fp, char __user *buf, size_t count,
                                  loff_t *pos)
{
    if (*pos >= 4)
        return 0;

    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    int ret = xcdev_check(__func__, xcdev, 0);
    if (ret < 0) {
        pr_warn("%s: xgpu_cdev check failed %d", __func__, ret);
        return ret;
    }

    struct xgpu_dev *xdev = xcdev->xdev;
    int loc = MINOR(xcdev->cdevno);
    if (loc < item_ifwi_max || loc > item_register_max) {
        pr_err("%s: invalid index of device file %d", __func__, loc);
        return -EINVAL;
    }

    loc -= item_ifwi_max;
    void __iomem *reg = xdev->bar[xcdev->bar] + register_items[loc].offset;
pr_warn("%s: bar %d base addr %x", __func__, xcdev->bar, xdev->bar[xcdev->bar]);
    if (!reg) {
        pr_warn("%s: not found devId %d", __func__, xcdev->cdevno);
        return -EINVAL;
    }
    uint32_t val = ioread32(reg);
    pr_warn("%s: register value %x", __func__, val);
    ret = copy_from_user(&val, buf, 4);
    if (ret) {
        pr_warn("%s: fail to copy_from_user %d", __func__, ret);
    }

    *pos += 4;
    return 4;
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
