

#include "xgpu_cdev.h"

static struct class *g_xgpu_class;
struct kmem_cache *cdev_cache;

static int config_kobject(struct xgpu_cdev *xcdev, enum cdev_type type)
{
	struct xgpu_dev *xdev = xcdev->xdev;
    int rv = kobject_set_name(&xcdev->cdev.kobj, devnode_name[type], xdev->idx);
    if (rv)
        pr_err("%s: type 0x%x, failed %d.\n", __func__, type, rv);

    return rv;
}

int xgpu_open(struct inode *node, struct file *file)
{
    struct xgpu_cdev *xcdev;

    xcdev = container_of(node->i_cdev, struct xgpu_cdev, cdev);

    /* create a reference to our char device in the opened file */
    file->private_data = xcdev;

    return 0;
}

/*
 * Called when the device goes from used to unused.
 */
int xgpu_close(struct inode *inode, struct file *file)
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

static int create_sys_device(struct xgpu_cdev *xcdev, enum cdev_type type)
{
	struct xgpu_dev *xdev = xcdev->xdev;

	xcdev->sys_device = device_create(g_xgpu_class, &xdev->pdev->dev,
		xcdev->cdevno, NULL, devnode_name[type], xdev->idx, xcdev->bar);
	if (!xcdev->sys_device) {
		pr_err("device_create(%s) failed\n", devnode_name[type]);
		return -1;
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
			int bar, enum cdev_type type)
{
	int rv;
    struct xgpu_dev *xdev = xpdev->xdev;
    dev_t dev;

    spin_lock_init(&xcdev->lock);
	if (!xpdev->major) { /* new instance? */
        /* allocate a dynamically allocated char device node */
        rv = alloc_chrdev_region(&dev, XGPU_MINOR_BASE,
                     XGPU_MINOR_COUNT, AMD_GPU_DEV_NAME);
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

    rv = config_kobject(xcdev, type);
    if (rv < 0)
        return rv;

    cdev_ctrl_init(xcdev);
    xcdev->cdevno = MKDEV(xpdev->major, type);

    /* bring character device live */
    rv = cdev_add(&xcdev->cdev, xcdev->cdevno, 1);
    if (rv < 0) {
        pr_err("cdev_add failed %d, type 0x%x.\n", rv, type);
        goto unregister_region;
    }

    pr_info("xcdev 0x%p, %u:%u, %s, type 0x%x.\n",
        xcdev, xpdev->major, type, xcdev->cdev.kobj.name, type);

    /* create device on our class */
    if (g_xgpu_class) {
        rv = create_sys_device(xcdev, type);
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

void xpdev_destroy_interfaces(struct xgpu_pci_dev *xpdev)
{
    for (enum cdev_type type = 0; type < CHAR_MAX; type++) {
        int rv = destroy_xcdev(&xpdev->ctrl_cdev[type]);
	    if (rv < 0)
            pr_err("failed to destroy device\n");
    }

    if (xpdev->major) {
        unregister_chrdev_region(MKDEV(xpdev->major, XGPU_MINOR_BASE), XGPU_MINOR_COUNT);
    }
}

int xpdev_create_interfaces(struct xgpu_pci_dev *xpdev)
{
    if (!xpdev->xdev) {
        pr_err("xdev is not initialized");
        return -1;
    }
    pr_warn("%s %d\n", __func__, __LINE__);
	struct xgpu_dev *xdev = xpdev->xdev;
	for (enum cdev_type type = 0; type < CHAR_MAX; type++) {
	    int rv = create_xcdev(xpdev, &xpdev->ctrl_cdev[type], xdev->config_bar_idx+type, type);
	    if (rv < 0) {
            pr_err("create_char(ctrl_cdev) failed\n");
            goto fail;
        }
    }

    return 0;

fail:
    xpdev_destroy_interfaces(xpdev);
    return -1;
}

int xgpu_cdev_init(void)
{
    g_xgpu_class = class_create(AMD_GPU_DEV_NAME);
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
