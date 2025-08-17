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

/* Target internal components on XDMA control BAR */
#define XDMA_OFS_INT_CTRL	(0x2000UL)
#define XDMA_OFS_CONFIG		(0x3000UL)
#ifndef VM_RESERVED
	#define VMEM_FLAGS (VM_IO | VM_DONTEXPAND | VM_DONTDUMP)
#else
	#define VMEM_FLAGS (VM_IO | VM_RESERVED)
#endif

#ifdef __LIBXGPU_DEBUG__
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

struct interrupt_regs {
	u32 identifier;
	u32 user_int_enable;
	u32 user_int_enable_w1s;
	u32 user_int_enable_w1c;
	u32 channel_int_enable;
	u32 channel_int_enable_w1s;
	u32 channel_int_enable_w1c;
	u32 reserved_1[9];	/* padding */

	u32 user_int_request;
	u32 channel_int_request;
	u32 user_int_pending;
	u32 channel_int_pending;
	u32 reserved_2[12];	/* padding */

	u32 user_msi_vector[8];
	u32 channel_msi_vector[8];
} __packed;

struct xgpu_user_irq {
	struct xgpu_dev *xdev;		/* parent device */
	u8 user_idx;			/* 0 ~ 15 */
	u8 events_irq;			/* accumulated IRQs */
	spinlock_t events_lock;		/* lock to safely update events_irq */
	wait_queue_head_t events_wq;	/* wait queue to sync waiting threads */
	irq_handler_t handler;

	void *dev;
};

/* PCIe device specific book-keeping */
#define XDEV_FLAG_OFFLINE	0x1
struct xgpu_dev {
	struct list_head list_head;
	struct list_head rcu_node;

	struct pci_dev* pdev;		/* pci device struct from probe() */
	int             idx;		/* dev index */
	const char*     mod_name;	/* name of module owning the dev */
	spinlock_t      lock;		/* protects concurrent access */
    unsigned int    flags;

    /* PCIe BAR management */
    void __iomem* bar[XGPU_BAR_NUM];        /* addresses for mapped BARs */
    int user_bar_idx;       /* BAR index of user logic */
    int config_bar_idx;     /* BAR index of config logic */
    int bypass_bar_idx;     /* BAR index of bypass logic */
    int regions_in_use;     /* flag if dev was in use during probe() */
    int got_regions;        /* flag if probe() obtained the regions */

    int user_max;

	/* Interrupt management */
	int irq_count;		/* interrupt counter */
	int irq_line;		/* flag if irq allocated successfully */
    int msi_enabled;	/* flag if msi was enabled for the device */
    int msix_enabled;	/* flag if msi-x was enabled for the device */
	struct msix_entry *msix_entries;
	int num_msix_entries;
	char **msix_names;
	struct xgpu_user_irq user_irq[16];	/* user IRQ management */
	unsigned int mask_irq_user;
};
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
