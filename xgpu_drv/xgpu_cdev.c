
#include "xgpu_cdev.h"
#include "cdev_ctrl.h"
#include "cdev_config.h"
#include "cdev_register.h"

static struct class *g_xgpu_class;
struct kmem_cache *cdev_cache;

static int config_kobject(struct xgpu_cdev *xcdev, int devId)
{
	struct xgpu_dev *xdev = xcdev->xdev;
    int rv   = 0;

    if (devId < item_config_max)
        rv = kobject_set_name(&xcdev->cdev.kobj, config_items[devId].devnode_name, xdev->idx);
    else if (devId < item_register_max)
        rv = kobject_set_name(&xcdev->cdev.kobj, register_items[devId - item_config_max].devnode_name, xdev->idx);
    else if (devId < item_ifwi_max)
        rv = kobject_set_name(&xcdev->cdev.kobj, ifwi_items[devId - item_register_max].devnode_name, xdev->idx);

    if (rv)
        pr_err("%s: devId 0x%x, failed %d.\n", __func__, devId, rv);

    return rv;
}

int xcdev_check(const char *fname, struct xgpu_cdev *xcdev, bool check_engine)
{
    if (!xcdev) {
        pr_info("%s, xcdev 0x%p.\n", fname, xcdev);
        return -EINVAL;
    }

    struct xgpu_dev *xdev = xcdev->xdev;
    if (!xdev) {
        pr_info("%s, xdev 0x%p.\n", fname, xdev);
        return -EINVAL;
    }

    return 0;
}

int char_open(struct inode *inode, struct file *file)
{
    struct xgpu_cdev *xcdev;

    xcdev = container_of(inode->i_cdev, struct xgpu_cdev, cdev);

    /* create a reference to our char device in the opened file */
    file->private_data = xcdev;

    return 0;
}

/*
 * Called when the device goes from used to unused.
 */
int char_close(struct inode *inode, struct file *file)
{
	struct xgpu_cdev *xcdev = (struct xgpu_cdev *)file->private_data;
    if (!xcdev) {
        pr_err("char device with inode 0x%lx xcdev NULL\n", inode->i_ino);
        return -EINVAL;
    }

    /* fetch device specific data stored earlier during open */
    struct xgpu_dev *xdev = xcdev->xdev;
    if (!xdev) {
        pr_err("char device with inode 0x%lx xdev NULL\n", inode->i_ino);
        return -EINVAL;
    }

    return 0;
}

static int create_sys_device(struct xgpu_cdev *xcdev, int devId)
{
	struct xgpu_dev *xdev = xcdev->xdev;

    if (devId < item_config_max) {
	    xcdev->sys_device = device_create(g_xgpu_class, &xdev->pdev->dev,
            xcdev->cdevno, NULL, config_items[devId].devnode_name, xdev->idx, xcdev->bar);
        if (!xcdev->sys_device) {
            pr_err("device_create(%s) failed\n", config_items[devId].devnode_name);
            return -1;
        }
    } else if (devId < item_register_max) {
        xcdev->sys_device = device_create(g_xgpu_class, &xdev->pdev->dev,
            xcdev->cdevno, NULL, register_items[devId - item_config_max].devnode_name, xdev->idx, xcdev->bar);
        if (!xcdev->sys_device) {
            pr_err("device_create(%s) failed\n", register_items[devId - item_config_max].devnode_name);
            return -1;
        }
    } else if (devId < item_ifwi_max) {
        xcdev->sys_device = device_create(g_xgpu_class, &xdev->pdev->dev,
            xcdev->cdevno, NULL, ifwi_items[devId - item_register_max].devnode_name, xdev->idx, xcdev->bar);
        if (!xcdev->sys_device) {
            pr_err("device_create(%s) failed\n", ifwi_items[devId - item_register_max].devnode_name);
            return -1;
        }
    }
    return 0;
}

static int destroy_xcdev(struct xgpu_cdev *cdev)
{
    if (!cdev) {
        pr_warn("%s: cdev NULL.\n", __func__);
        return -EINVAL;
    }

    if (!cdev->xdev) {
        pr_err("%s: xdev NULL\n", __func__);
        return -EINVAL;
    }

    if (!g_xgpu_class) {
        pr_err("%s: g_xgpu_class NULL\n", __func__);
        return -EINVAL;
    }

    if (!cdev->sys_device) {
        pr_err("%s: cdev sys_device NULL\n", __func__);
        return -EINVAL;
    }

    if (cdev->sys_device)
        device_destroy(g_xgpu_class, cdev->cdevno);

    cdev_del(&cdev->cdev);

    return 0;
}

static int create_xcdev(struct xgpu_pci_dev *xpdev, struct xgpu_cdev *xcdev,
			int bar, int devId)
{
	int rv;
    struct xgpu_dev *xdev = xpdev->xdev;
    dev_t dev;

    spin_lock_init(&xcdev->lock);
	if (!xpdev->major) { /* new instance? */
        /* allocate a dynamically allocated char device node */
        rv = alloc_chrdev_region(&dev, 0, XGPU_MINOR_COUNT, AMD_GPU_DEV_NAME);
        if (rv) {
            pr_err("unable to allocate cdev region %d.\n", rv);
            return rv;
        }
        xpdev->major = MAJOR(dev);
    }

    // do not register yet, create kobjects and name them,
    xcdev->cdev.owner = THIS_MODULE;
    xcdev->xpdev = xpdev;
    xcdev->xdev = xdev;
    xcdev->bar = bar;

    rv = config_kobject(xcdev, devId);
    if (rv < 0)
        return rv;

    if (devId < item_config_max)
        cdev_config_init(xcdev);
    else if (devId < item_register_max)
        cdev_register_init(xcdev);
    else if (devId < item_ifwi_max)
        cdev_ctrl_init(xcdev);

    xcdev->cdevno = MKDEV(xpdev->major, devId);

    /* bring character device live */
    rv = cdev_add(&xcdev->cdev, xcdev->cdevno, 1);
    if (rv < 0) {
        pr_err("cdev_add failed %d, devId 0x%x.\n", rv, devId);
        goto unregister_region;
    }

    pr_info("xcdev 0x%p, %u:%u, %s, devId 0x%x.\n",
        xcdev, xpdev->major, devId, xcdev->cdev.kobj.name, devId);

    /* create device on our class */
    if (g_xgpu_class) {
        rv = create_sys_device(xcdev, devId);
        if (rv < 0)
            goto del_cdev;
    }

    return 0;

del_cdev:
    cdev_del(&xcdev->cdev);
unregister_region:
    unregister_chrdev_region(xcdev->cdevno, XGPU_MINOR_COUNT);
    return rv;
}

static void file_destroy_interfaces(struct xgpu_pci_dev *xpdev, int type)
{
    struct list_head *cursor;

    if (list_empty(&xpdev->listHead[type])) {
        pr_info("file interface is empty");
        return;
    }

    list_for_each(cursor, &xpdev->listHead[type]) {
        struct xcdev_member *member = list_entry(cursor, struct xcdev_member, entry);
        int32_t devId = member->xcdev.cdevno;
        if ((devId >= item_config_max) && (devId < item_register_max)) {
            cdev_register_exit(&member->xcdev);
        }

        if (member && destroy_xcdev(&member->xcdev) < 0) {
            pr_err("%s: failed to destroy device\n", __func__);
            return;
        }

        list_del(cursor);
        kfree(cursor);
    }
}

static int file_create_interface(struct xgpu_pci_dev *xpdev, const xgpu_cdev_node *pItem, int size, int type)
{
    struct xgpu_dev *xdev = xpdev->xdev;
    if (!xdev) {
        pr_err("%s: xdev is not initialized", __func__);
        return -1;
    }

    for (int ii = 0; ii < size; ii++) {
        struct xcdev_member *member = kmalloc(sizeof(struct xcdev_member), GFP_KERNEL);
        if (!member) {
            pr_err("%s: failed to kmalloc xcdev_member\n", __func__);
            goto __fail_config;
        }

        int rv = create_xcdev(xpdev, &member->xcdev, xdev->config_bar_idx + ii, (pItem + ii)->devId);
        if (rv < 0) {
            pr_err("%s: create_xcdev failed to device %s\n", __func__, (pItem + ii)->devnode_name);
            goto __fail_config;
        }

        list_add_tail(&member->entry, &xpdev->listHead[type]);
    }

    return 0;

__fail_config:
    file_destroy_interfaces(xpdev, type);
    return -1;
}

void xpdev_destroy_interfaces(struct xgpu_pci_dev *xpdev)
{
    file_destroy_interfaces(xpdev, 2);
    file_destroy_interfaces(xpdev, 1);
    file_destroy_interfaces(xpdev, 0);

    if (xpdev->major) {
        unregister_chrdev_region(MKDEV(xpdev->major, 0), XGPU_MINOR_COUNT);
    }
}

int xpdev_create_interfaces(struct xgpu_pci_dev *xpdev)
{
    int ret = file_create_interface(xpdev, config_items, sizeof(config_items)/sizeof(xgpu_cdev_node), 0);
    if (ret) {
        goto __fail_entry;
    }
    ret = file_create_interface(xpdev, register_items, sizeof(register_items)/sizeof(xgpu_cdev_node), 1);
    if (ret) {
        goto __fail_entry;
    }
    ret = file_create_interface(xpdev, ifwi_items, sizeof(ifwi_items)/sizeof(xgpu_cdev_node), 2);
    if (ret) {
        goto __fail_entry;
    }
    return ret;

__fail_entry:
    xpdev_destroy_interfaces(xpdev);
    return -1;
}

int xgpu_cdev_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    g_xgpu_class = class_create(THIS_MODULE, AMD_GPU_DEV_NAME);
#else
    g_xgpu_class = class_create(AMD_GPU_DEV_NAME);
#endif
    if (IS_ERR(g_xgpu_class)) {
        pr_err("%s: failed to create class", AMD_GPU_DEV_NAME);
        return -EINVAL;
    }

    /* using kmem_cache_create to enable sequential cleanup */
    cdev_cache = kmem_cache_create("cdev_cache",
                                   100, 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!cdev_cache) {
        pr_info("memory allocation for cdev_cache failed. OOM\n");
        return -ENOMEM;
    }

    return 0;
}

void xgpu_cdev_cleanup(void)
{
	if (cdev_cache)
        kmem_cache_destroy(cdev_cache);

    if (g_xgpu_class)
		class_destroy(g_xgpu_class);
}
