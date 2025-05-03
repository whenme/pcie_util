
#include "cdev_config.h"
#include "cdev_ctrl.h"
#include "xgpu_cdev.h"
#include "libxgpu.h"

#define PCI_CONFIG_SIZE    256
uint8_t config_data[PCI_CONFIG_SIZE];

void config_destroy_interfaces(struct xgpu_pci_dev *xpdev)
{
    struct list_head *cursor;

    if (list_empty(&xpdev->listHeadConfig)) {
        pr_info("ifwi interface is empty");
        return;
    }

    list_for_each(cursor, &xpdev->listHeadConfig) {
        struct xcdev_member *member = list_entry(cursor, struct xcdev_member, entry);
        if (member && destroy_xcdev(&member->xcdev) < 0) {
            pr_err("%s: failed to destroy device\n", __func__);
            return;
        }

        list_del(cursor);
        kfree(cursor);
    }
}

int config_create_interface(struct xgpu_pci_dev *xpdev)
{
    struct xgpu_dev *xdev = xpdev->xdev;
    if (!xdev) {
        pr_err("%s: xdev is not initialized", __func__);
        return -1;
    }

    for (int ii = 0; ii < sizeof(config_items)/sizeof(xgpu_cdev_node); ii++) {
        struct xcdev_member *member = kmalloc(sizeof(struct xcdev_member), GFP_KERNEL);
        if (!member) {
            pr_err("%s: failed to kmalloc xcdev_member\n", __func__);
		    goto __fail_config;
        }

        int rv = create_xcdev(xpdev, &member->xcdev, xdev->config_bar_idx + ii, config_items[ii].devId);
        if (rv < 0) {
            pr_err("%s: create_xcdev failed to device %s\n", __func__, config_items[ii].devnode_name);
            goto __fail_config;
        }

        list_add_tail(&member->entry, &xpdev->listHeadConfig);
    }

    return 0;

__fail_config:
    config_destroy_interfaces(xpdev);
    return -1;
}

static ssize_t char_config_read(struct file *fp, char __user *buf, size_t count,
                                loff_t *pos)
{
    int ret;

    ret = copy_to_user(buf, config_data, 4);
    if (ret < 0) {
        pr_err("%s: failed to copy to user. ret=%d", __func__, ret);
    } else {
        pr_info("%s: success to copy %d byte to user!", __func__, ret);
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
    char *pbuf = config_data;

	cdev_init(&xcdev->cdev, &config_fops);

    for (int ii = 0; ii < PCI_CONFIG_SIZE/4; ii++) {
        pci_read_config_dword(pdev, ii, (uint32_t *)pbuf);
        pbuf += 4;
    }

    pbuf = config_data;
    for (int ii = 0; ii < PCI_CONFIG_SIZE/16; ii += 16) {
        printk("%08x %08x %08x %08x \n", (uint32_t)(*(pbuf+ii)), (uint32_t)(*(pbuf+ii+4)),
               (uint32_t)(*(pbuf+ii+8)), (uint32_t)(*(pbuf+ii+12)));
    }
}
