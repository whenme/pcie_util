
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/pci.h>

#include "xgpu_mod.h"
#include "libxgpu.h"

static unsigned int interrupt_mode;
module_param(interrupt_mode, uint, 0644);
MODULE_PARM_DESC(interrupt_mode, "0-Auto , 1-MSI, 2-Legacy, 3-MSI-x");

/*
 * xgpu device management
 * maintains a list of the xgpu devices
 */
static LIST_HEAD(xdev_list);
static DEFINE_MUTEX(xdev_mutex);

#ifndef list_last_entry
#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)
#endif

static inline int xdev_list_add(struct xgpu_dev *xdev)
{
	mutex_lock(&xdev_mutex);
	if (list_empty(&xdev_list)) {
		xdev->idx = 0;
	} else {
		struct xgpu_dev *last;
		last = list_last_entry(&xdev_list, struct xgpu_dev, list_head);
		xdev->idx = last->idx + 1;
	}
	list_add_tail(&xdev->list_head, &xdev_list);
	mutex_unlock(&xdev_mutex);

	pr_info("%s: dev %s, xdev 0x%p, xgpu idx %d.\n", __func__,
		 dev_name(&xdev->pdev->dev), xdev, xdev->idx);
	return 0;
}

static inline void xdev_list_remove(struct xgpu_dev *xdev)
{
	mutex_lock(&xdev_mutex);

    if (!list_empty(&xdev->list_head)) {
	    list_del(&xdev->list_head);
    }
	mutex_unlock(&xdev_mutex);
}

struct xgpu_dev *xdev_find_by_pdev(struct pci_dev *pdev)
{
	struct xgpu_dev *xdev, *tmp;

	mutex_lock(&xdev_mutex);
	list_for_each_entry_safe(xdev, tmp, &xdev_list, list_head) {
		if (xdev->pdev == pdev) {
			mutex_unlock(&xdev_mutex);
			return xdev;
		}
	}
	mutex_unlock(&xdev_mutex);
	return NULL;
}

static inline int debug_check_dev_hndl(const char *fname, struct pci_dev *pdev,
                      void *hndl)
{
    if (!pdev)
        return -EINVAL;

    struct xgpu_dev *xdev = xdev_find_by_pdev(pdev);
    if (!xdev) {
        pr_info("%s pdev 0x%p, hndl 0x%p, NO match found!\n", fname, pdev, hndl);
        return -EINVAL;
    }
    if (xdev != hndl) {
        pr_err("%s pdev 0x%p, hndl 0x%p != 0x%p!\n", fname, pdev, hndl, xdev);
        return -EINVAL;
    }

    return 0;
}

#define write_register(v, mem, off) iowrite32(v, mem)

inline u32 read_register(void *iomem)
{
    return ioread32(iomem);
}

static inline u32 build_u32(u32 hi, u32 lo)
{
    return ((hi & 0xFFFFUL) << 16) | (lo & 0xFFFFUL);
}

static inline u64 build_u64(u64 hi, u64 lo)
{
    return ((hi & 0xFFFFFFFULL) << 32) | (lo & 0xFFFFFFFFULL);
}

static struct xgpu_dev *alloc_dev_instance(struct pci_dev *pdev)
{
    if (!pdev) {
        pr_err("Invalid pdev\n");
        return NULL;
    }

    /* allocate zeroed device book keeping structure */
    struct xgpu_dev *xdev = kzalloc(sizeof(struct xgpu_dev), GFP_KERNEL);
    if (!xdev) {
        pr_info("OOM, xgpu_dev.\n");
        return NULL;
    }
    spin_lock_init(&xdev->lock);

    xdev->config_bar_idx = -1;
    xdev->user_bar_idx = -1;
    xdev->bypass_bar_idx = -1;

    /* create a driver to device reference */
    xdev->pdev = pdev;
    pr_info("%s: xdev = 0x%p\n", __func__, xdev);

    return xdev;
}

/*
 * Unmap the BAR regions that had been mapped earlier using map_bars()
 */
static void unmap_bars(struct xgpu_dev *xdev, struct pci_dev *dev)
{
    for (int i = 0; i < XGPU_BAR_NUM; i++) {
        /* is this BAR mapped? */
        if (xdev->bar[i]) {
            /* unmap BAR */
            pci_iounmap(dev, xdev->bar[i]);
            /* mark as unmapped */
            xdev->bar[i] = NULL;
        }
    }
}

static int map_single_bar(struct xgpu_dev *xdev, struct pci_dev *dev, int idx)
{
	resource_size_t bar_start = pci_resource_start(dev, idx);
	resource_size_t bar_len = pci_resource_len(dev, idx);
	resource_size_t map_len = bar_len;

	xdev->bar[idx] = NULL;

	/* do not map BARs with length 0. Note that start MAY be 0! */
	if (!bar_len) {
		pr_info("BAR #%d is not present - skipping\n", idx);
		return 0;
	}

	/* BAR size exceeds maximum desired mapping? */
	if (bar_len > INT_MAX) {
		pr_info("Limit BAR %d mapping from %llu to %d bytes\n", idx,
			(u64)bar_len, INT_MAX);
		map_len = (resource_size_t)INT_MAX;
	}
	/*
	 * map the full device memory or IO region into kernel virtual
	 * address space
	 */
	dbg_init("BAR%d: %llu bytes to be mapped.\n", idx, (u64)map_len);
	xdev->bar[idx] = pci_iomap(dev, idx, map_len);

	if (!xdev->bar[idx]) {
		pr_info("Could not map BAR %d.\n", idx);
		return -1;
	}

	pr_info("BAR%d at 0x%llx mapped at 0x%p, length=%llu(/%llu)\n", idx,
		(u64)bar_start, xdev->bar[idx], (u64)map_len, (u64)bar_len);

	return (int)map_len;
}

static int identify_bars(struct xgpu_dev *xdev, int *bar_id_list, int num_bars,
                         int config_bar_pos)
{
	/*
	 * The following logic identifies which BARs contain what functionality
	 * based on the position of the XDMA config BAR and the number of BARs
	 * detected. The rules are that the user logic and bypass logic BARs
	 * are optional.  When both are present, the XDMA config BAR will be the
	 * 2nd BAR detected (config_bar_pos = 1), with the user logic being
	 * detected first and the bypass being detected last. When one is
	 * omitted, the type of BAR present can be identified by whether the
	 * XDMA config BAR is detected first or last.  When both are omitted,
	 * only the XDMA config BAR is present.  This somewhat convoluted
	 * approach is used instead of relying on BAR numbers in order to work
	 * correctly with both 32-bit and 64-bit BARs.
	 */

	if (!xdev) {
		pr_err("%s: Invalid xdev\n", __func__);
		return -EINVAL;
	}

	if (!bar_id_list) {
		pr_err("%s: Invalid bar id list.\n", __func__);
		return -EINVAL;
	}

	dbg_init("xdev 0x%p, bars %d, config at %d.\n", xdev, num_bars,
		 config_bar_pos);

	switch (num_bars) {
	case 1:
		/* Only one BAR present - no extra work necessary */
		break;

	case 2:
		if (config_bar_pos == 0) {
			xdev->bypass_bar_idx = bar_id_list[1];
		} else if (config_bar_pos == 1) {
			xdev->user_bar_idx = bar_id_list[0];
		} else {
			pr_info("%s: 2, XDMA config BAR unexpected %d.\n",
				__func__, config_bar_pos);
		}
		break;

	case 3:
	case 4:
		if ((config_bar_pos == 1) || (config_bar_pos == 2)) {
			/* user bar at bar #0 */
			xdev->user_bar_idx = bar_id_list[0];
			/* bypass bar at the last bar */
			xdev->bypass_bar_idx = bar_id_list[num_bars - 1];
		} else {
			pr_info("%s: 3/4, XDMA config BAR unexpected %d.\n",
				__func__, config_bar_pos);
		}
		break;

	default:
		/* Should not occur - warn user but safe to continue */
		pr_info("%s: Unexpected # BARs (%d), XDMA config BAR only.\n",
            __func__, num_bars);
		break;
	}
	pr_info("%d BARs: config %d, user %d, bypass %d.\n", num_bars,
		config_bar_pos, xdev->user_bar_idx, xdev->bypass_bar_idx);
	return 0;
}

/* map_bars() -- map device regions into kernel virtual address space
 *
 * Map the device memory regions into kernel virtual address space after
 * verifying their sizes respect the minimum sizes needed
 */
static int map_bars(struct xgpu_dev *xdev, struct pci_dev *dev)
{
    int rv;
    int bar_id_list[XGPU_BAR_NUM];
    int bar_id_idx = 0;
    int config_bar_pos = 0;

	/* iterate through all the BARs */
	for (int i = 0; i < XGPU_BAR_NUM; i++) {
		int bar_len = map_single_bar(xdev, dev, i);
		if (bar_len == 0) {
			continue;
		} else if (bar_len < 0) {
			rv = -EINVAL;
			goto fail;
		}

		// Try to identify BAR as XDMA control BAR
		/*if ((bar_len >= XGPU_BAR_SIZE) && (xdev->config_bar_idx < 0)) {
			if (is_config_bar(xdev, i)) {
				xdev->config_bar_idx = i;
				config_bar_pos = bar_id_idx;
				pr_info("config bar %d, pos %d.\n",
					xdev->config_bar_idx, config_bar_pos);
			}
		}*/

		bar_id_list[bar_id_idx] = i;
		bar_id_idx++;
	}

    pr_warn("manually set config bar to 1...\n"); 
    xdev->config_bar_idx = 1;
	/* The XDMA config BAR must always be present */
	if (xdev->config_bar_idx < 0) {
		pr_info("Failed to detect XDMA config BAR\n");
		rv = -EINVAL;
		goto fail;
	}

	rv = identify_bars(xdev, bar_id_list, bar_id_idx, config_bar_pos);
	if (rv < 0) {
		pr_err("%s: Failed to identify bars\n", __func__);
		return rv;
	}

	/* successfully mapped all required BAR regions */
	return 0;

fail:
	/* unwind; unmap any BARs that we did map */
	unmap_bars(xdev, dev);
	return rv;
}

/*
 * code to detect if MSI/MSI-X capability exists is derived
 * from linux/pci/msi.c - pci_msi_check_device
 */

#ifndef arch_msi_check_device
static int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif

/* type = PCI_CAP_ID_MSI or PCI_CAP_ID_MSIX */
static int msi_msix_capable(struct pci_dev *dev, int type)
{
	struct pci_bus *bus;
	int ret;

	if (!dev || dev->no_msi)
		return 0;

	for (bus = dev->bus; bus; bus = bus->parent)
		if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
			return 0;

	ret = arch_msi_check_device(dev, 1, type);
	if (ret)
		return 0;

	if (!pci_find_capability(dev, type))
		return 0;

	return 1;
}

static void disable_msi_msix(struct xgpu_dev *xdev, struct pci_dev *pdev)
{
	if (xdev->msix_enabled) {
		pci_disable_msix(pdev);
		xdev->msix_enabled = 0;
	} else if (xdev->msi_enabled) {
		pci_disable_msi(pdev);
		xdev->msi_enabled = 0;
	}
}

static int enable_msi_msix(struct xgpu_dev *xdev, struct pci_dev *pdev)
{
	int rv = 0;

	if (!xdev) {
		pr_err("Invalid xdev\n");
		return -EINVAL;
	}

	if (!pdev) {
		pr_err("Invalid pdev\n");
		return -EINVAL;
	}

	if ((interrupt_mode == 3 || !interrupt_mode) && msi_msix_capable(pdev, PCI_CAP_ID_MSIX)) {
		int req_nvec = xdev->user_max;

		dbg_init("Enabling MSI-X\n");
		rv = pci_alloc_irq_vectors(pdev, req_nvec, req_nvec, PCI_IRQ_MSIX);
		if (rv < 0)
			dbg_init("Couldn't enable MSI-X mode: %d\n", rv);

		xdev->msix_enabled = 1;
	} else if ((interrupt_mode == 1 || !interrupt_mode) &&
		   msi_msix_capable(pdev, PCI_CAP_ID_MSI)) {
		/* enable message signalled interrupts */
		dbg_init("pci_enable_msi()\n");
		rv = pci_enable_msi(pdev);
		if (rv < 0)
			dbg_init("Couldn't enable MSI mode: %d\n", rv);
		xdev->msi_enabled = 1;
	} else {
		dbg_init("MSI/MSI-X not detected - using legacy interrupts\n");
	}

	return rv;
}

static void pci_check_intr_pend(struct pci_dev *pdev)
{
	u16 v;

	pci_read_config_word(pdev, PCI_STATUS, &v);
	if (v & PCI_STATUS_INTERRUPT) {
		pr_info("%s PCI STATUS Interrupt pending 0x%x.\n",
			dev_name(&pdev->dev), v);
		pci_write_config_word(pdev, PCI_STATUS, PCI_STATUS_INTERRUPT);
	}
}

static void pci_keep_intx_enabled(struct pci_dev *pdev)
{
	/* workaround to a h/w bug:
	 * when msix/msi become unavaile, default to legacy.
	 * However the legacy enable was not checked.
	 * If the legacy was disabled, no ack then everything stuck
	 */
	u16 pcmd, pcmd_new;

	pci_read_config_word(pdev, PCI_COMMAND, &pcmd);
	pcmd_new = pcmd & ~PCI_COMMAND_INTX_DISABLE;
	if (pcmd_new != pcmd) {
		pr_info("%s: clear INTX_DISABLE, 0x%x -> 0x%x.\n",
			dev_name(&pdev->dev), pcmd, pcmd_new);
		pci_write_config_word(pdev, PCI_COMMAND, pcmd_new);
	}
}

static int request_regions(struct xgpu_dev *xdev, struct pci_dev *pdev)
{
    if (!xdev) {
        pr_err("%s: Invalid xdev\n", __func__);
        return -EINVAL;
    }

    if (!pdev) {
        pr_err("%s: Invalid pdev\n", __func__);
        return -EINVAL;
    }

    dbg_init("%s: mod_name %s\n", __func__, xdev->mod_name);
#if 0
    int rv = pci_request_regions(pdev, xdev->mod_name);
    /* could not request all regions? */
    if (rv) {
        dbg_init("pci_request_regions() = %d, device in use?\n", rv);
        /* assume device is in use so do not disable it later */
        xdev->regions_in_use = 1;
    } else {
        xdev->got_regions = 1;
    }
#endif

    // region 0 is in efifb. it cannot request_region.
    int rv = pci_request_region(pdev, 2, "region2 memory");
    if (rv) {
        pr_warn("fail to request region 2\n");
        return rv;
    }
    rv = pci_request_region(pdev, 5, "region5 memory");
    if (rv)
        pr_warn("fail to request region 5\n");

    return rv;
}

static int set_dma_mask(struct pci_dev *pdev)
{
	if (!pdev) {
		pr_err("Invalid pdev\n");
		return -EINVAL;
	}

	/* 64-bit addressing capability for XDMA? */
	if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64))) 
	{
		/* query for DMA transfer */
		/* @see Documentation/DMA-mapping.txt */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
#endif
		/* use 64-bit DMA */
		dbg_init("Using a 64-bit DMA mask.\n");
	}
    else if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) 
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
#endif
		/* use 32-bit DMA */
		dbg_init("Using a 32-bit DMA mask.\n");
	} else {
		dbg_init("No suitable DMA possible.\n");
		return -EINVAL;
	}

	return 0;
}

static void pci_enable_capability(struct pci_dev *pdev, int cap)
{
    u16 vcap;
    int pos = pci_pcie_cap(pdev);
    if (pos > 0) {
        pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &vcap);
        vcap |= cap;
        pci_write_config_word(pdev, pos + PCI_EXP_DEVCTL, vcap);
    }
}

void *xgpu_device_open(const char *mname, struct pci_dev *pdev, int *user_max)
{
    pr_info("%s: %s device %s, pdev 0x%p.\n", __func__, mname, dev_name(&pdev->dev), pdev);

    /* allocate zeroed device book keeping structure */
    struct xgpu_dev *xdev = alloc_dev_instance(pdev);
    if (!xdev)
        return NULL;

    xdev->mod_name = mname;
    xdev->user_max = *user_max;

    xgpu_device_flag_set(xdev, XDEV_FLAG_OFFLINE);

    int rv = xdev_list_add(xdev);
    if (rv < 0)
        goto free_xdev;

    rv = pci_enable_device(pdev);
    if (rv) {
        pr_warn("pci_enable_device() failed, %d.\n", rv);
        goto err_enable;
    }

    /* keep INTx enabled */
    pci_check_intr_pend(pdev);

    /* enable relaxed ordering */
    pci_enable_capability(pdev, PCI_EXP_DEVCTL_RELAX_EN);

    /* enable extended tag */
    pci_enable_capability(pdev, PCI_EXP_DEVCTL_EXT_TAG);

    /* force MRRS to be 512 */
    rv = pcie_set_readrq(pdev, 512);
    if (rv)
        pr_info("device %s, error set PCI_EXP_DEVCTL_READRQ: %d.\n",
            dev_name(&pdev->dev), rv);

    /* enable bus master capability */
    pci_set_master(pdev);

    rv = request_regions(xdev, pdev);
    if (rv) {
        pr_warn("%s: failed to request regions", __func__);
        goto err_regions;
    }

    rv = map_bars(xdev, pdev);
    if (rv) {
        pr_warn("%s: failed to map bars", __func__);
        goto err_map;
    }

    rv = set_dma_mask(pdev);
    if (rv) {
        pr_warn("%s: failed to set_dma_mask", __func__);
        goto err_mask;
    }
/*
    check_nonzero_interrupt_status(xdev);
    // explicitely zero all interrupt enable masks
    channel_interrupts_disable(xdev, ~0);
    user_interrupts_disable(xdev, ~0);
    read_interrupts(xdev);

    rv = probe_engines(xdev);
    if (rv)
        goto err_mask;
*/
    rv = enable_msi_msix(xdev, pdev);
    if (rv < 0) {
        pr_warn("%s: fail to enable msi/msix", __func__);
        //goto err_engines;
    }
/*
    rv = irq_setup(xdev, pdev);
    if (rv < 0)
        goto err_msix;

    // Flush writes 
    read_interrupts(xdev);*/

    *user_max = xdev->user_max;

    xgpu_device_flag_clear(xdev, XDEV_FLAG_OFFLINE);
    return (void *)xdev;

//err_msix:
//    disable_msi_msix(xdev, pdev);
//err_engines:
	//remove_engines(xdev);
err_mask:
    unmap_bars(xdev, pdev);
err_map:
    if (xdev->got_regions)
        pci_release_regions(pdev);
err_regions:
    if (!xdev->regions_in_use)
        pci_disable_device(pdev);
err_enable:
    xdev_list_remove(xdev);
free_xdev:
    kfree(xdev);
    return NULL;
}

void xgpu_device_close(struct pci_dev *pdev, void *dev_hndl)
{
	if (!dev_hndl)
		return;

	struct xgpu_dev *xdev = (struct xgpu_dev *)dev_hndl;
	pr_info("%s: pdev 0x%p, xdev 0x%p.\n", __func__, pdev, dev_hndl);

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	pr_info("remove(dev = 0x%p) where pdev->dev.driver_data = 0x%p\n", pdev, xdev);
	if (xdev->pdev != pdev) {
		pr_warn("pci_dev(0x%lx) != pdev(0x%lx)\n",
		       (unsigned long)xdev->pdev, (unsigned long)pdev);
	}

	//channel_interrupts_disable(xdev, ~0);
	//user_interrupts_disable(xdev, ~0);
	//read_interrupts(xdev);

	//irq_teardown(xdev);
	//disable_msi_msix(xdev, pdev);

	//remove_engines(xdev);
	unmap_bars(xdev, pdev);

	if (xdev->got_regions) {
		pr_info("pci_release_regions 0x%p.\n", pdev);
		pci_release_regions(pdev);
	}

	if (!xdev->regions_in_use) {
		pr_info("pci_disable_device 0x%p.\n", pdev);
		pci_disable_device(pdev);
	}

	xdev_list_remove(xdev);
	kfree(xdev);
}

void xgpu_device_offline(struct pci_dev *pdev, void *dev_hndl)
{
	if (!dev_hndl)
		return;

	struct xgpu_dev *xdev = (struct xgpu_dev *)dev_hndl;
	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	pr_info("pdev 0x%p, xdev 0x%p.\n", pdev, xdev);
	xgpu_device_flag_set(xdev, XDEV_FLAG_OFFLINE);

	pr_info("%s: xdev 0x%p, done.\n", __func__, xdev);
}

void xgpu_device_online(struct pci_dev *pdev, void *dev_hndl)
{
	if (!dev_hndl)
		return;

	struct xgpu_dev *xdev = (struct xgpu_dev *)dev_hndl;
	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	pr_info("pdev 0x%p, xdev 0x%p.\n", pdev, xdev);

	xgpu_device_flag_clear(xdev, XDEV_FLAG_OFFLINE);
	pr_info("%s: xdev 0x%p, done.\n", __func__, xdev);
}
