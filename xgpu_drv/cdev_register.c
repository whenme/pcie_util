
#include "cdev_register.h"
#include "xgpu_cdev.h"
#include "libxgpu.h"

static void __iomem *pcimap_addr = NULL;

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

    int loc = MINOR(xcdev->cdevno) - item_config_max;
    if (loc <= 0 || loc > sizeof(register_items)/sizeof(xgpu_cdev_node)) {
        pr_err("%s: invalid index of device file %d", __func__, loc);
        return -EINVAL;
    }

    uint32_t val = ioread32(pcimap_addr + register_items[loc].offset);
    if (copy_to_user(buf, &val, 4)) {
        pr_warn("%s: failed copy_to_user", __func__);
        return -EIO;
    } else {
        *pos += 4;
        return 4;
    }
}

static ssize_t char_register_write(struct file *fp, const char __user *buf,
                                   size_t count, loff_t *pos)
{
    if (*pos >= 4)
        return 0;

    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    int ret = xcdev_check(__func__, xcdev, 0);
    if (ret < 0) {
        pr_warn("%s: xgpu_cdev check failed %d", __func__, ret);
        return ret;
    }

    int loc = MINOR(xcdev->cdevno) - item_config_max;
    if (loc <= 0 || loc > sizeof(register_items)/sizeof(xgpu_cdev_node)) {
        pr_err("%s: invalid index of device file %d", __func__, loc);
        return -EINVAL;
    }

    uint32_t val = 0;
    if (copy_from_user(&val, buf, 4)) {
        pr_err("%s: fail copy_from_user for 1 byte", __func__);
        return -EINVAL;
    }

    iowrite32(val, pcimap_addr + register_items[loc].offset);
    *pos += 4;
    return 4;
}

static long char_register_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    register_read_write reg;
    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    int ret = xcdev_check(__func__, xcdev, 0);
    if (ret < 0) {
        pr_warn("%s: xgpu_cdev check failed %d", __func__, ret);
        return ret;
    }

    switch(cmd) {
    case XGPU_IOCTL_REGISTER_READ:
        if (copy_from_user(&reg, (void __user *)arg, sizeof(register_read_write))) {
            pr_err("%s: copy_from_user failed.\n", __func__);
            return -EFAULT;
        }
        reg.value = ioread32(pcimap_addr + reg.address);
        if (copy_to_user((void __user *)arg, &reg, sizeof(register_read_write))) {
            pr_err("%s: copy_to_user failed.\n", __func__);
            return -EFAULT;
        }
        break;
    case XGPU_IOCTL_REGISTER_WRITE:
        if (copy_from_user(&reg, (void __user *)arg, sizeof(register_read_write))) {
            pr_err("%s: copy_from_user failed.\n", __func__);
            return -EFAULT;
        }
        iowrite32(reg.value, pcimap_addr + reg.address);
        break;
    default:
        pr_err("UNKNOWN ioctl cmd 0x%x.\n", cmd);
        return -ENOTTY;
    }
    return 0;
}

/*
 * character device file operations
 */
static const struct file_operations register_fops = {
    .owner = THIS_MODULE,
    .open = char_open,
    .release = char_close,
    .read = char_register_read,
    .write = char_register_write,
    .unlocked_ioctl = char_register_ioctl,
};

void cdev_register_init(struct xgpu_cdev *xcdev)
{
    cdev_init(&xcdev->cdev, &register_fops);

    // AMD registers are in BAR5
    pcimap_addr = pci_iomap(xcdev->xdev->pdev, 5, 0);
}

void cdev_register_exit(struct xgpu_cdev *xcdev)
{
    if (pcimap_addr) {
        pci_iounmap(xcdev->xdev->pdev, pcimap_addr);
        pcimap_addr = NULL;
    }
}
