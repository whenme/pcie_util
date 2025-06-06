/*
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/ioctl.h>

#include "xgpu_cdev.h"
#include "cdev_ctrl.h"
#include "libxgpu.h"

#if ACCESS_OK_2_ARGS
#define xlx_access_ok(X, Y, Z) access_ok(Y, Z)
#else
#define xlx_access_ok(X, Y, Z) access_ok(X, Y, Z)
#endif

/*
 * character device file operations for control bus (through control bridge)
 */
static ssize_t char_ctrl_read(struct file *fp, char __user *buf, size_t count,
                              loff_t *pos)
{
    struct xgpu_cdev *xcdev = (struct xgpu_cdev *)fp->private_data;
    void __iomem *reg;

	int rv = xcdev_check(__func__, xcdev, 0);
	if (rv < 0)
		return rv;

	struct xgpu_dev *xdev = xcdev->xdev;

	/* only 32-bit aligned and 32-bit multiples */
	if (*pos & 3)
		return -EPROTO;
	/* first address is BAR base plus file position offset */
	reg = xdev->bar[xcdev->bar] + *pos;

	u32 val = ioread32(reg);
	dbg_sg("%s(@%p, count=%ld, pos=%d) value = 0x%08x\n",
			__func__, reg, (long)count, (int)*pos, val);
	rv = copy_to_user(buf, &val, 4);
	if (rv)
		dbg_sg("Copy to userspace failed but continuing\n");

	*pos += 4;
	return 4;
}

static ssize_t char_ctrl_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *pos)
{
	struct xgpu_cdev *xcdev = (struct xgpu_cdev *)file->private_data;
	void __iomem *reg;
	u32 val;

    int rv = xcdev_check(__func__, xcdev, 0);
    if (rv < 0) {
        return rv;
    }

	struct xgpu_dev *xdev = xcdev->xdev;

	/* only 32-bit aligned and 32-bit multiples */
	if (*pos & 3)
		return -EPROTO;

	/* first address is BAR base plus file position offset */
	reg = xdev->bar[xcdev->bar] + *pos;
	rv = copy_from_user(&val, buf, 4);
	if (rv)
		pr_info("copy from user failed %d/4, but continuing.\n", rv);

	dbg_sg("%s(0x%08x @%p, count=%ld, pos=%d)\n",
			__func__, val, reg, (long)count, (int)*pos);
	//write_register(val, reg);
	iowrite32(val, reg);
	*pos += 4;
	return 4;
}

static long version_ioctl(struct xgpu_cdev *xcdev, void __user *arg)
{
	struct xgpu_ioc_info obj;
	struct xgpu_dev *xdev = xcdev->xdev;

	int rv = copy_from_user((void *)&obj, arg, sizeof(struct xgpu_ioc_info));
	if (rv) {
		pr_info("copy from user failed %d/%ld.\n",
			rv, sizeof(struct xgpu_ioc_info));
		return -EFAULT;
	}
	memset(&obj, 0, sizeof(obj));
	obj.vendor = xdev->pdev->vendor;
	obj.device = xdev->pdev->device;
	obj.subsystem_vendor = xdev->pdev->subsystem_vendor;
	obj.subsystem_device = xdev->pdev->subsystem_device;
	obj.driver_version = DRV_MOD_VERSION_NUMBER;
	obj.domain = 0;
	obj.bus = PCI_BUS_NUM(xdev->pdev->devfn);
	obj.dev = PCI_SLOT(xdev->pdev->devfn);
	obj.func = PCI_FUNC(xdev->pdev->devfn);
	if (copy_to_user(arg, &obj, sizeof(struct xgpu_ioc_info)))
		return -EFAULT;
	return 0;
}

static long char_ctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xgpu_ioc_base ioctl_obj;
	long   result = 0;
	struct xgpu_cdev* xcdev = (struct xgpu_cdev *)filp->private_data;
    int rv = xcdev_check(__func__, xcdev, 0);
    if (rv < 0) {
        return rv;
    }

	struct xgpu_dev* xdev = xcdev->xdev;
	if (!xdev) {
		pr_info("cmd %u, xdev NULL.\n", cmd);
		return -EINVAL;
	}
	pr_info("cmd 0x%x, xdev 0x%p, pdev 0x%p.\n", cmd, xdev, xdev->pdev);

	if (_IOC_TYPE(cmd) != XDMA_IOC_MAGIC) {
		pr_err("cmd %u, bad magic 0x%x/0x%x.\n",
			 cmd, _IOC_TYPE(cmd), XDMA_IOC_MAGIC);
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		result = !xlx_access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		result = !xlx_access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (result) {
		pr_err("bad access %ld.\n", result);
		return -EFAULT;
	}

	switch (cmd) {
	case XDMA_IOCINFO:
		if (copy_from_user((void *)&ioctl_obj, (void __user *) arg,
			 sizeof(struct xgpu_ioc_base))) {
			pr_err("copy_from_user failed.\n");
			return -EFAULT;
		}

		return version_ioctl(xcdev, (void __user *)arg);
	case XDMA_IOCOFFLINE:
		xgpu_device_offline(xdev->pdev, xdev);
		break;
	case XDMA_IOCONLINE:
		xgpu_device_online(xdev->pdev, xdev);
		break;
	default:
		pr_err("UNKNOWN ioctl cmd 0x%x.\n", cmd);
		return -ENOTTY;
	}
	return 0;
}

/* maps the PCIe BAR into user space for memory-like access using mmap() */
static int bridge_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct xgpu_cdev *xcdev = (struct xgpu_cdev *)file->private_data;
	unsigned long off, phys;
	unsigned long vsize, psize;

    int rv = xcdev_check(__func__, xcdev, 0);
    if (rv < 0)
        return rv;

    struct xgpu_dev *xdev = xcdev->xdev;

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = pci_resource_start(xdev->pdev, xcdev->bar) + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = pci_resource_end(xdev->pdev, xcdev->bar) -
		pci_resource_start(xdev->pdev, xcdev->bar) + 1 - off;

	dbg_sg("mmap(): xcdev = 0x%08lx\n", (unsigned long)xcdev);
	dbg_sg("mmap(): cdev->bar = %d\n", xcdev->bar);
	dbg_sg("mmap(): xdev = 0x%p\n", xdev);
	dbg_sg("mmap(): pci_dev = 0x%08lx\n", (unsigned long)xdev->pdev);
	dbg_sg("off = 0x%lx, vsize 0x%lu, psize 0x%lu.\n", off, vsize, psize);
	dbg_sg("start = 0x%llx\n", (u64)pci_resource_start(xdev->pdev, xcdev->bar));
	dbg_sg("phys = 0x%lx\n", phys);

	if (vsize > psize)
		return -EINVAL;
	/*
	 * pages must not be cached as this would result in cache line sized
	 * accesses to the end point
	 */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/*
	 * prevent touching the pages (byte access) for swap-in,
	 * and prevent the pages from being swapped out
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    vm_flags_set(vma, VMEM_FLAGS);
#elif defined(RHEL_RELEASE_CODE)
    #if (RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(9, 4))
        vm_flags_set(vma, VMEM_FLAGS);
    #else
        vma->vm_flags |= VMEM_FLAGS;
    #endif
#else
    vma->vm_flags |= VMEM_FLAGS;
#endif
	/* make MMIO accessible to user space */
	rv = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
			vsize, vma->vm_page_prot);
	dbg_sg("vma=0x%p, vma->vm_start=0x%lx, phys=0x%lx, size=%lu = %d\n",
		vma, vma->vm_start, phys >> PAGE_SHIFT, vsize, rv);

	if (rv)
		return -EAGAIN;
	return 0;
}

/*
 * character device file operations for control bus (through control bridge)
 */
static const struct file_operations ctrl_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.read = char_ctrl_read,
	.write = char_ctrl_write,
	.mmap = bridge_mmap,
	.unlocked_ioctl = char_ctrl_ioctl,
};

void cdev_ctrl_init(struct xgpu_cdev *xcdev)
{
	cdev_init(&xcdev->cdev, &ctrl_fops);
}
