
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

static void check_nonzero_interrupt_status(struct xgpu_dev *xdev)
{
//fix it...
#if 0
	struct interrupt_regs *reg =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);
	u32 w;

	w = read_register(&reg->user_int_enable);
	if (w)
		pr_info("%s xdma%d user_int_enable = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);

	w = read_register(&reg->channel_int_enable);
	if (w)
		pr_info("%s xdma%d channel_int_enable = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);

	w = read_register(&reg->user_int_request);
	if (w)
		pr_info("%s xdma%d user_int_request = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);
	w = read_register(&reg->channel_int_request);
	if (w)
		pr_info("%s xdma%d channel_int_request = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);

	w = read_register(&reg->user_int_pending);
	if (w)
		pr_info("%s xdma%d user_int_pending = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);
	w = read_register(&reg->channel_int_pending);
	if (w)
		pr_info("%s xdma%d channel_int_pending = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);
#endif
}

/* channel_interrupts_enable -- Enable interrupts we are interested in */
static void channel_interrupts_enable(struct xgpu_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->channel_int_enable_w1s, XDMA_OFS_INT_CTRL);
}

/* channel_interrupts_disable -- Disable interrupts we not interested in */
static void channel_interrupts_disable(struct xgpu_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->channel_int_enable_w1c, XDMA_OFS_INT_CTRL);
}

/* user_interrupts_disable -- Disable interrupts we not interested in */
static void user_interrupts_disable(struct xgpu_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->user_int_enable_w1c, XDMA_OFS_INT_CTRL);
}

/* read_interrupts -- Print the interrupt controller status */
static u32 read_interrupts(struct xgpu_dev *xdev)
{
	struct interrupt_regs *reg =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);
	u32 lo;
	u32 hi;

	/* extra debugging; inspect complete engine set of registers */
	hi = read_register(&reg->user_int_request);
	dbg_io("ioread32(0x%p) returned 0x%08x (user_int_request).\n",
	       &reg->user_int_request, hi);
	lo = read_register(&reg->channel_int_request);
	dbg_io("ioread32(0x%p) returned 0x%08x (channel_int_request)\n",
	       &reg->channel_int_request, lo);

	/* return interrupts: user in upper 16-bits, channel in lower 16-bits */
	return build_u32(hi, lo);
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

static irqreturn_t user_irq_service(int irq, struct xgpu_user_irq *user_irq)
{
	unsigned long flags;

	if (!user_irq) {
		pr_err("Invalid user_irq\n");
		return IRQ_NONE;
	}

	if (user_irq->handler)
		return user_irq->handler(user_irq->user_idx, user_irq->dev);

	spin_lock_irqsave(&(user_irq->events_lock), flags);
	if (!user_irq->events_irq) {
		user_irq->events_irq = 1;
		wake_up_interruptible(&(user_irq->events_wq));
	}
	spin_unlock_irqrestore(&(user_irq->events_lock), flags);

	return IRQ_HANDLED;
}

/*
 * xgpu_isr() - Interrupt handler
 *
 * @dev_id pointer to xdma_dev
 */
static irqreturn_t xgpu_isr(int irq, void *dev_id)
{
	u32 ch_irq, user_irq;
	struct xgpu_dev *xdev;
	struct interrupt_regs *irq_regs;

	dbg_irq("(irq=%d, dev 0x%p) <<<< ISR.\n", irq, dev_id);
	if (!dev_id) {
		pr_err("Invalid dev_id on irq line %d\n", irq);
		return -IRQ_NONE;
	}
	xdev = (struct xgpu_dev *)dev_id;

	if (!xdev) {
		WARN_ON(!xdev);
		dbg_irq("%s(irq=%d) xdev=%p ??\n", __func__, irq, xdev);
		return IRQ_NONE;
	}

	irq_regs = (struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					     XDMA_OFS_INT_CTRL);

	/* read channel interrupt requests */
	ch_irq = read_register(&irq_regs->channel_int_request);
	dbg_irq("ch_irq = 0x%08x\n", ch_irq);

	/*
	 * disable all interrupts that fired; these are re-enabled individually
	 * after the causing module has been fully serviced.
	 */
	if (ch_irq)
		channel_interrupts_disable(xdev, ch_irq);

	/* read user interrupts - this read also flushes the above write */
	user_irq = read_register(&irq_regs->user_int_request);
	dbg_irq("user_irq = 0x%08x\n", user_irq);

	if (user_irq) {
		int user = 0;
		u32 mask = 1;
		int max = xdev->user_max;

		for (; user < max && user_irq; user++, mask <<= 1) {
			if (user_irq & mask) {
				user_irq &= ~mask;
				user_irq_service(irq, &xdev->user_irq[user]);
			}
		}
	}
#if 0
	u32 mask = ch_irq & xdev->mask_irq_h2c;
	if (mask) {
		int channel = 0;
		int max = xdev->h2c_channel_max;

		/* iterate over H2C (PCIe read) */
		for (channel = 0; channel < max && mask; channel++) {
			struct xdma_engine *engine = &xdev->engine_h2c[channel];

			/* engine present and its interrupt fired? */
			if ((engine->irq_bitmask & mask) &&
			    (engine->magic == MAGIC_ENGINE)) {
				mask &= ~engine->irq_bitmask;
				dbg_tfr("schedule_work, %s.\n", engine->name);
				schedule_work(&engine->work);
			}
		}
	}

	mask = ch_irq & xdev->mask_irq_c2h;
	if (mask) {
		int channel = 0;
		int max = xdev->c2h_channel_max;

		/* iterate over C2H (PCIe write) */
		for (channel = 0; channel < max && mask; channel++) {
			struct xdma_engine *engine = &xdev->engine_c2h[channel];

			/* engine present and its interrupt fired? */
			if ((engine->irq_bitmask & mask) &&
			    (engine->magic == MAGIC_ENGINE)) {
				mask &= ~engine->irq_bitmask;
				dbg_tfr("schedule_work, %s.\n", engine->name);
				schedule_work(&engine->work);
			}
		}
	}
#endif
	xdev->irq_count++;
	return IRQ_HANDLED;
}

/*
 * xdma_user_irq() - Interrupt handler for user interrupts in MSI-X mode
 *
 * @dev_id pointer to xdma_dev
 */
static irqreturn_t xgpu_user_irq(int irq, void *dev_id)
{
	struct xgpu_user_irq *user_irq;

	dbg_irq("(irq=%d) <<<< INTERRUPT SERVICE ROUTINE\n", irq);

	if (!dev_id) {
		pr_err("Invalid dev_id on irq line %d\n", irq);
		return IRQ_NONE;
	}
	user_irq = (struct xgpu_user_irq *)dev_id;

	return user_irq_service(irq, user_irq);
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

static void prog_irq_msix_user(struct xgpu_dev *xdev, bool clear)
{
	/* user */
	struct interrupt_regs *int_regs =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);
    //fix it...
	u32 i = 0;//= xdev->c2h_channel_max + xdev->h2c_channel_max;
	u32 max = i + xdev->user_max;
	int j;

	for (j = 0; i < max; j++) {
		u32 val = 0;
		int k, shift = 0;

		if (clear)
			i += 4;
		else
			for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
				val |= (i & 0x1f) << shift;

		write_register(val, &int_regs->user_msi_vector[j],
			XDMA_OFS_INT_CTRL +
				((unsigned long)&int_regs->user_msi_vector[j] -
				 (unsigned long)int_regs));

		dbg_init("vector %d, 0x%x.\n", j, val);
	}
}

static void prog_irq_msix_channel(struct xgpu_dev *xdev, bool clear)
{
	struct interrupt_regs *int_regs =
		(struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					  XDMA_OFS_INT_CTRL);
    //fix it...
	u32 max = 0;// = xdev->c2h_channel_max + xdev->h2c_channel_max;
	u32 i, j;

	/* engine */
	for (i = 0, j = 0; i < max; j++) {
		u32 val = 0;
		int k, shift = 0;

		if (clear)
			i += 4;
		else
			for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
				val |= (i & 0x1f) << shift;

		write_register(val, &int_regs->channel_msi_vector[j],
			       XDMA_OFS_INT_CTRL +
				       ((unsigned long)&int_regs
						->channel_msi_vector[j] -
					(unsigned long)int_regs));
		dbg_init("vector %d, 0x%x.\n", j, val);
	}
}

static void irq_msix_channel_teardown(struct xgpu_dev *xdev)
{
//fix it...
#if 0
	struct xdma_engine *engine;
	int j = 0, i = 0;

	if (!xdev->msix_enabled)
		return;

	prog_irq_msix_channel(xdev, 1);

	engine = xdev->engine_h2c;
	for (i = 0; i < xdev->h2c_channel_max; i++, j++, engine++) {
		if (!engine->msix_irq_line)
			break;
		dbg_sg("Release IRQ#%d for engine %p\n", engine->msix_irq_line,
		       engine);
		free_irq(engine->msix_irq_line, engine);
	}

	engine = xdev->engine_c2h;
	for (i = 0; i < xdev->c2h_channel_max; i++, j++, engine++) {
		if (!engine->msix_irq_line)
			break;
		dbg_sg("Release IRQ#%d for engine %p\n", engine->msix_irq_line,
		       engine);
		free_irq(engine->msix_irq_line, engine);
	}
#endif
}

static int irq_msix_channel_setup(struct xgpu_dev *xdev)
{
//fix it...
#if 0
	int i, j, rv = 0;
	u32 vector;
	struct xdma_engine *engine;

	if (!xdev) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	if (!xdev->msix_enabled)
		return 0;

	j = xdev->h2c_channel_max;
	engine = xdev->engine_h2c;
	for (i = 0; i < xdev->h2c_channel_max; i++, engine++) {
#if KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE
		vector = pci_irq_vector(xdev->pdev, i);
#else
		vector = xdev->entry[i].vector;
#endif
		rv = request_irq(vector, xdma_channel_irq, 0, xdev->mod_name,
				 engine);
		if (rv) {
			pr_info("requesti irq#%d failed %d, engine %s.\n",
				vector, rv, engine->name);
			return rv;
		}
		pr_info("engine %s, irq#%d.\n", engine->name, vector);
		engine->msix_irq_line = vector;
	}

	engine = xdev->engine_c2h;
	for (i = 0; i < xdev->c2h_channel_max; i++, j++, engine++) {
#if KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE
		vector = pci_irq_vector(xdev->pdev, j);
#else
		vector = xdev->entry[j].vector;
#endif
		rv = request_irq(vector, xdma_channel_irq, 0, xdev->mod_name,
				 engine);
		if (rv) {
			pr_info("requesti irq#%d failed %d, engine %s.\n",
				vector, rv, engine->name);
			return rv;
		}
		pr_info("engine %s, irq#%d.\n", engine->name, vector);
		engine->msix_irq_line = vector;
	}
#endif
	return 0;
}

static void irq_msix_user_teardown(struct xgpu_dev *xdev)
{
	int i, j = 0;

	if (!xdev) {
		pr_err("Invalid xdev\n");
		return;
	}

	if (!xdev->msix_enabled)
		return;

    // fix it...
	//j = xdev->h2c_channel_max + xdev->c2h_channel_max;

	prog_irq_msix_user(xdev, 1);

	for (i = 0; i < xdev->user_max; i++, j++) {
#if KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE
		u32 vector = pci_irq_vector(xdev->pdev, j);
#else
		u32 vector = xdev->entry[j].vector;
#endif
		dbg_init("user %d, releasing IRQ#%d\n", i, vector);
		free_irq(vector, &xdev->user_irq[i]);
	}
}

static int irq_msix_user_setup(struct xgpu_dev *xdev)
{
	int i, rv = 0;
    //fix it...
	int j = 0;// = xdev->h2c_channel_max + xdev->c2h_channel_max;

	/* vectors set in probe_scan_for_msi() */
	for (i = 0; i < xdev->user_max; i++, j++) {
#if KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE
		u32 vector = pci_irq_vector(xdev->pdev, j);
#else
		u32 vector = xdev->entry[j].vector;
#endif
		rv = request_irq(vector, xgpu_user_irq, 0, xdev->mod_name,
				 &xdev->user_irq[i]);
		if (rv) {
			pr_info("user %d couldn't use IRQ#%d, %d\n", i, vector,
				rv);
			break;
		}
		pr_info("%d-USR-%d, IRQ#%d with 0x%p\n", xdev->idx, i, vector,
			&xdev->user_irq[i]);
	}

	/* If any errors occur, free IRQs that were successfully requested */
	if (rv) {
		for (i--, j--; i >= 0; i--, j--) {
#if KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE
			u32 vector = pci_irq_vector(xdev->pdev, j);
#else
			u32 vector = xdev->entry[j].vector;
#endif
			free_irq(vector, &xdev->user_irq[i]);
		}
	}

	return rv;
}

static int irq_msi_setup(struct xgpu_dev *xdev, struct pci_dev *pdev)
{
	int rv;

	xdev->irq_line = (int)pdev->irq;
	rv = request_irq(pdev->irq, xgpu_isr, 0, xdev->mod_name, xdev);
	if (rv)
		dbg_init("Couldn't use IRQ#%d, %d\n", pdev->irq, rv);
	else
		dbg_init("Using IRQ#%d with 0x%p\n", pdev->irq, xdev);

	return rv;
}

static int irq_legacy_setup(struct xgpu_dev *xdev, struct pci_dev *pdev)
{
	u32 w;
	u8 val;
	void *reg;
	int rv;

	pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &val);
	if (val == 0) {
		dbg_init("Legacy interrupt not supported\n");
		return -EINVAL;
	}

	dbg_init("Legacy Interrupt register value = %d\n", val);
	if (val > 1) {
		val--;
		w = (val << 24) | (val << 16) | (val << 8) | val;
		/* Program IRQ Block Channel vector and IRQ Block User vector
		 * with Legacy interrupt value
		 */
		reg = xdev->bar[xdev->config_bar_idx] + 0x2080; // IRQ user
		write_register(w, reg, 0x2080);
		write_register(w, reg + 0x4, 0x2084);
		write_register(w, reg + 0x8, 0x2088);
		write_register(w, reg + 0xC, 0x208C);
		reg = xdev->bar[xdev->config_bar_idx] + 0x20A0; // IRQ Block
		write_register(w, reg, 0x20A0);
		write_register(w, reg + 0x4, 0x20A4);
	}

	xdev->irq_line = (int)pdev->irq;
	rv = request_irq(pdev->irq, xgpu_isr, IRQF_SHARED, xdev->mod_name,
			 xdev);
	if (rv)
		dbg_init("Couldn't use IRQ#%d, %d\n", pdev->irq, rv);
	else
		dbg_init("Using IRQ#%d with 0x%p\n", pdev->irq, xdev);

	return rv;
}

static void irq_teardown(struct xgpu_dev *xdev)
{
    if (xdev->msix_enabled) {
        irq_msix_channel_teardown(xdev);
        irq_msix_user_teardown(xdev);
    } else if (xdev->irq_line != -1) {
        dbg_init("Releasing IRQ#%d\n", xdev->irq_line);
        free_irq(xdev->irq_line, xdev);
    }
}

static int irq_setup(struct xgpu_dev *xdev, struct pci_dev *pdev)
{
    pci_keep_intx_enabled(pdev);

    if (xdev->msix_enabled) {
        int rv = irq_msix_channel_setup(xdev);
        if (rv)
            return rv;
        rv = irq_msix_user_setup(xdev);
        if (rv)
            return rv;
        prog_irq_msix_channel(xdev, 0);
        prog_irq_msix_user(xdev, 0);

        return 0;
    } else if (xdev->msi_enabled)
        return irq_msi_setup(xdev, pdev);

    return irq_legacy_setup(xdev, pdev);
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
#if 0   // commented for throw
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
//fit it...
    check_nonzero_interrupt_status(xdev);
    // explicitely zero all interrupt enable masks
    channel_interrupts_disable(xdev, ~0);
    user_interrupts_disable(xdev, ~0);
    read_interrupts(xdev);
/*
    rv = probe_engines(xdev);
    if (rv)
        goto err_mask;
*/
    rv = enable_msi_msix(xdev, pdev);
    if (rv < 0) {
        pr_warn("%s: fail to enable msi/msix", __func__);
        goto err_msix;
    }

    rv = irq_setup(xdev, pdev);
    if (rv < 0)
        goto err_msix;

    // Flush writes 
    read_interrupts(xdev);

    *user_max = xdev->user_max;

    xgpu_device_flag_clear(xdev, XDEV_FLAG_OFFLINE);
    return (void *)xdev;

err_msix:
    disable_msi_msix(xdev, pdev);
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

    irq_teardown(xdev);
    disable_msi_msix(xdev, pdev);

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
