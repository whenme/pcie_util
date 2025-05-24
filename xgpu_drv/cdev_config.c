
#include "cdev_config.h"
#include "cdev_ctrl.h"
#include "xgpu_cdev.h"
#include "libxgpu.h"

uint8_t config_data[PCI_CONFIG_SIZE];

static ssize_t char_config_read(struct file *fp, char __user *buf, size_t count,
                                loff_t *pos)
{
    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    struct pci_dev *pdev = xcdev->xdev->pdev;
    int ret;
    uint8_t dat[3];

    //length <= 4, return ascii hex string
    //length > 4, return binary value
    int loc = MINOR(xcdev->cdevno);
    if (loc >= item_config_max) {
        pr_err("%s: invalid index of device file %d", __func__, loc);
        return -EINVAL;
    }

    uint32_t val = 0;
    uint32_t length = config_items[loc].length;
    switch (length) {
    case 1:
        pci_read_config_byte(pdev, config_items[loc].offset, (u8*)&val);
        break;
    case 2:
        pci_read_config_word(pdev, config_items[loc].offset, (u16*)&val);
        break;
    case 3:
        for (int ii = 0; ii < 3; ii++) {
            pci_read_config_byte(pdev, config_items[loc].offset+ii, dat+ii);
        }
        val = (dat[2]<<16) + (dat[1]<<8) + dat[0];
        break;
    case 4:
        pci_read_config_dword(pdev, config_items[loc].offset, &val);
        break;
    default:
        break;
    }

    if (length <= 4) {
        if (*pos > 0)
            return 0;

        char bufStr[16] = {};
        sprintf(bufStr, "0x%x\n", val);
        length = strlen(bufStr);
        ret = copy_to_user(buf, bufStr, length);
        if (ret) {
            pr_info("%s: fail copy_to_user %d", __func__, ret);
            return -EIO;
        } else {
            *pos += length;
            return length;
        }
    } else if (length > 4) {
        if (*pos + count > PCI_CONFIG_SIZE) {
            int size = PCI_CONFIG_SIZE - *pos;
            count = (size > 0) ? size : 0;
        }
        if (!count || *pos >= count)
            return 0;

        pr_info("%s: read %ld bytes pos %lld", __func__, count, *pos);
        ret = copy_to_user(buf, config_data+*pos, count);
        if (ret) {
            pr_info("%s: fail copy_to_user %d", __func__, ret);
            return -EIO;
        } else {
            *pos += count;
            return count;
        }
    }
    return ret;
}

static ssize_t char_config_write(struct file *fp, const char __user *buf,
                                 size_t count, loff_t *pos)
{
    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    struct pci_dev *pdev = xcdev->xdev->pdev;
    //int ret;

    //length <= 4, return ascii hex string
    //length > 4, return binary value
    int loc = MINOR(xcdev->cdevno);
    if (loc >= item_config_max) {
        pr_err("%s: invalid index of device file %d", __func__, loc);
        return -EINVAL;
    }

    uint32_t val = 0;
    uint32_t length = config_items[loc].length;
    if (*pos >= length)
        return 0;

    switch (length) {
    case 1:
        if (copy_from_user(&val, buf, 1)) {
            pr_err("%s: fail copy_from_user for 1 byte", __func__);
            return -EINVAL;
        }
        pci_write_config_byte(pdev, config_items[loc].offset, (uint8_t)val);
        break;
    case 2:
        if (copy_from_user(&val, buf, 2)) {
            pr_err("%s: fail copy_from_user for 2 byte", __func__);
            return -EINVAL;
        }
        pci_write_config_word(pdev, config_items[loc].offset, (uint16_t)val);
        break;
    case 3:
        if (copy_from_user(&val, buf, 3)) {
            pr_err("%s: fail copy_from_user for 3 byte", __func__);
            return -EINVAL;
        }
        for (int ii = 0; ii < 3; ii++) {
            pci_write_config_byte(pdev, config_items[loc].offset+ii, (uint8_t)((val>>(ii*8))&0xFF));
        }
        break;
    case 4:
        if (copy_from_user(&val, buf, 4)) {
            pr_err("%s: fail copy_from_user for 4 byte", __func__);
            return -EINVAL;
        }
        pci_read_config_dword(pdev, config_items[loc].offset, &val);
        break;
    default:
        break;
    }

    *pos += length;
    return length;
}

/*
 * character device file operations for control bus (through control bridge)
 */
static const struct file_operations config_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.read = char_config_read,
	.write = char_config_write,
};

void cdev_config_init(struct xgpu_cdev *xcdev)
{
    struct pci_dev* pdev = xcdev->xdev->pdev;

	cdev_init(&xcdev->cdev, &config_fops);

    char *pbuf = config_data;
    for (int ii = 0; ii < PCI_CONFIG_SIZE; ii++)
        pci_read_config_byte(pdev, ii, pbuf++);
}
