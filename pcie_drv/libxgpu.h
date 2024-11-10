
#ifndef __XGPU_LIB_H__
#define __XGPU_LIB_H__

#include <linux/version.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/workqueue.h>

/* Add compatibility checking for RHEL versions */
#if defined(RHEL_RELEASE_CODE)
#	define ACCESS_OK_2_ARGS (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0))
#else
#	define ACCESS_OK_2_ARGS (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#endif

/* maximum amount of register space to map */
#define XGPU_BAR_SIZE (0x8000UL)

#ifndef VM_RESERVED
	#define VMEM_FLAGS (VM_IO | VM_DONTEXPAND | VM_DONTDUMP)
#else
	#define VMEM_FLAGS (VM_IO | VM_RESERVED)
#endif

#ifdef __LIBXDMA_DEBUG__
#define dbg_io		pr_err
#define dbg_fops	pr_err
#define dbg_perf	pr_err
#define dbg_sg		pr_err
#define dbg_tfr		pr_err
#define dbg_irq		pr_err
#define dbg_init	pr_err
#define dbg_desc	pr_err
#else
/* disable debugging */
#define dbg_io(...)
#define dbg_fops(...)
#define dbg_perf(...)
#define dbg_sg(...)
#define dbg_tfr(...)
#define dbg_irq(...)
#define dbg_init(...)
#define dbg_desc(...)
#endif

static inline void xgpu_device_flag_set(struct xgpu_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	xdev->flags |= f;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

static inline void xgpu_device_flag_clear(struct xgpu_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	xdev->flags &= ~f;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

struct xgpu_dev *xdev_find_by_pdev(struct pci_dev *pdev);

void xgpu_device_offline(struct pci_dev *pdev, void *dev_handle);
void xgpu_device_online(struct pci_dev *pdev, void *dev_handle);

void *xgpu_device_open(const char *mname, struct pci_dev *pdev, int *user_max);
void xgpu_device_close(struct pci_dev *pdev, void *dev_hndl);

#endif /* __XGPU_LIB_H__ */