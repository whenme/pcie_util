#ifndef __XGPU_MODULE_H__
#define __XGPU_MODULE_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include <linux/version.h>

#define DRV_MOD_MAJOR		2024
#define DRV_MOD_MINOR		11
#define DRV_MOD_PATCHLEVEL	10

#define DRV_MODULE_VERSION      \
	__stringify(DRV_MOD_MAJOR) "." \
	__stringify(DRV_MOD_MINOR) "." \
	__stringify(DRV_MOD_PATCHLEVEL)

#define DRV_MOD_VERSION_NUMBER  \
	((DRV_MOD_MAJOR)*1000 + (DRV_MOD_MINOR)*100 + DRV_MOD_PATCHLEVEL)

#define AMD_GPU_DEV_NAME     "xgpu"
#define DRV_MODULE_DESC      "Gpu Reference Driver"

MODULE_AUTHOR("whenme");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

/* SECTION: Preprocessor macros/constants */
#define XGPU_BAR_NUM   (6)
#define LIST_HEAD_NUM  (3)

typedef struct _xgpu_cdev_node_ {
    uint32_t  devId;               //16bit(base) + 16bit(id)
    uint32_t  offset, length;
    char      devnode_name[32];
}xgpu_cdev_node;

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

    int msi_enabled;	/* flag if msi was enabled for the device */
    int msix_enabled;	/* flag if msi-x was enabled for the device */
	struct msix_entry entry[32];	/* msi-x vector/entry table */
};

struct xgpu_cdev {
	struct xgpu_pci_dev* xpdev;
	struct xgpu_dev*     xdev;
	dev_t                cdevno;	/* character device major:minor */
	struct cdev          cdev;		/* character device embedded struct */
	int                  bar;		/* PCIe BAR for HW access, if needed */
	unsigned long        base;		/* bar access offset */
	struct device*       sys_device;/* sysfs device */
	spinlock_t lock;
};

struct xgpu_pci_dev {
	struct pci_dev*  pdev;		/* pci device struct from probe() */
	struct xgpu_dev* xdev;
	int              major;		/* major number */
	int              instance;	/* instance number */
    int              user_max;

    struct list_head listHead[LIST_HEAD_NUM];
    void *data;
};

struct xcdev_member {
    struct xgpu_cdev xcdev;
    struct list_head entry;
};

#endif /* ifndef __XGPU_MODULE_H__ */
