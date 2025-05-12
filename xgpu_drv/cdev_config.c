
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
    uint32_t val = 0, length = 0;
    int ret;
    char bufStr[16] = {};

    //length <= 4, return ascii hex string
    //length > 4, return binary value
    for (int ii = 0; ii < item_config_max; ii++) {
        if (MINOR(xcdev->cdevno) == config_items[ii].devId) {
            length = config_items[ii].length;
            switch (length) {
            case 1:
                pci_read_config_byte(pdev, config_items[ii].offset, (u8*)&val);
                break;
            case 2:
                pci_read_config_word(pdev, config_items[ii].offset, (u16*)&val);
                break;
            case 3:
                pci_read_config_byte(pdev, config_items[ii].offset, (u8*)&val);
                pci_read_config_byte(pdev, config_items[ii].offset+1, (u8*)(&val+1));
                pci_read_config_byte(pdev, config_items[ii].offset+2, (u8*)(&val+2));
                break;
            case 4:
                pci_read_config_dword(pdev, config_items[ii].offset, &val);
                break;
            default:
                if (*pos >= PCI_CONFIG_SIZE) {
                    return 0;
                }
                if (*pos + count > PCI_CONFIG_SIZE) {
                    int size = PCI_CONFIG_SIZE - *pos;
                    count = (size > 0) ? size : 0;
                }
                break;
            }
        }
    }

    if (length <= 4) {
        if (*pos > 0)
            return 0;

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
        if (!count || *pos >= PCI_CONFIG_SIZE)
            return 0;

        pr_info("%s: read %ld bytes", __func__, count);
        ret = copy_to_user(buf, config_data+*pos, count);
        if (ret) {
            pr_info("%s: fail copy_to_user %d", __func__, ret);
            return -EIO;
        } else {
            return count;
        }
    }
    return ret;
}

static ssize_t char_config_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *pos)
{
    return 0;
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
