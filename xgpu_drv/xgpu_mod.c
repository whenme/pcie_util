/**
 * @file   gpu_drv.c
 * @brief  amd pcie device driver
 * @copyright Copyright © 2024 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <linux/ioctl.h>
#include <linux/errno.h>

#include "xgpu_cdev.h"
#include "libxgpu.h"

static const struct pci_device_id amd_pci_ids[] = {
    { PCI_DEVICE(0x1002, 0x748f), }, //nv44
    { PCI_DEVICE(0x1002, 0x7440), }, //nv31
    { PCI_DEVICE(0x1002, 0x743f), }, //nv32
    { PCI_DEVICE(0x1002, 0x746f), }, //nv48
    { PCI_DEVICE(0x1002, 0x15bf), }, //phx
    { PCI_DEVICE(0x1002, 0x164e), }, //rpl
    {0,}
};
MODULE_DEVICE_TABLE(pci, amd_pci_ids);

/* SECTION: Module global variables */
static int xpdev_cnt = 0;

static void xpdev_free(struct xgpu_pci_dev *xpdev)
{
	struct xgpu_dev *xdev = xpdev->xdev;

	pr_info("xpdev 0x%p, destroy_interfaces, xdev 0x%p.\n", xpdev, xdev);
	xpdev_destroy_interfaces(xpdev);
	xpdev->xdev = NULL;
	pr_info("xpdev 0x%p, xdev 0x%p xgpu_device_close.\n", xpdev, xdev);

	xgpu_device_close(xpdev->pdev, xdev);
	xpdev_cnt--;

	kfree(xpdev);
}

static struct xgpu_pci_dev *xpdev_alloc(struct pci_dev *pdev)
{
	struct xgpu_pci_dev *xpdev = kmalloc(sizeof(*xpdev), GFP_KERNEL);
	if (!xpdev)
		return NULL;

	memset(xpdev, 0, sizeof(*xpdev));
	xpdev->pdev = pdev;

	xpdev_cnt++;
	return xpdev;
}

static irqreturn_t xgpu_msi_interrupt(int irq, void *dev_id)
{
    struct xgpu_dev *xdev = (struct xgpu_dev *)dev_id;
    dev_info(&xdev->pdev->dev, "MSI interrupt received!\n");
    return IRQ_HANDLED;
}

static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int rv = 0;

    struct xgpu_pci_dev *xpdev = xpdev_alloc(pdev);
    if (!xpdev)
        return -ENOMEM;

    for (int ii = 0; ii < LIST_HEAD_NUM; ii++) {
        INIT_LIST_HEAD(&xpdev->listHead[ii]);
    }

    void *hndl = xgpu_device_open(AMD_GPU_DEV_NAME, pdev, &xpdev->user_max);
    if (!hndl) {
        rv = -EINVAL;
        goto err_out;
    }

    /* make sure no duplicate */
    struct xgpu_dev* xdev = xdev_find_by_pdev(pdev);
    if (!xdev) {
        pr_warn("%s: NO xdev found!\n", __func__);
        rv = -EINVAL;
        goto err_out;
	}

    if (hndl != xdev) {
        pr_err("%s: xdev handle mismatch\n", __func__);
        rv = -EINVAL;
        goto err_out;
    }

    pr_info("%s: %s xgpu%d, pdev 0x%p, xdev 0x%p, 0x%p.\n", __func__,
        dev_name(&pdev->dev), xdev->idx, pdev, xpdev, xdev);

    xpdev->xdev = hndl;

    rv = pci_enable_msi(pdev);
    if (rv) {
        dev_err(&pdev->dev, "pci_enable_msi failed\n");
        goto err_out;
    }

    rv = request_irq(pdev->irq, xgpu_msi_interrupt, 0, "xgpu_msi", xpdev->xdev);
    if (rv) {
        dev_err(&pdev->dev, "request_irq failed\n");
        pci_disable_msi(pdev);
        goto err_out;
    }

    rv = xpdev_create_interfaces(xpdev);
    if (rv)
        goto err_irq;

    dev_set_drvdata(&pdev->dev, xpdev);

    return 0;

err_irq:
    free_irq(pdev->irq, xpdev->xdev);
    pci_disable_msi(pdev);
err_out:
    pr_err("%s: pdev 0x%p, err %d.\n", __func__, pdev, rv);
    xpdev_free(xpdev);
    return rv;
}

static void remove_one(struct pci_dev *pdev)
{
    struct xgpu_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);
    if (xpdev) {
        pr_info("%s: pdev 0x%p, xpdev 0x%p.\n", __func__, pdev, xpdev);
        free_irq(pdev->irq, xpdev->xdev);
        pci_disable_msi(pdev);
        xpdev_free(xpdev);
    }
}

static pci_ers_result_t xgpu_error_detected(struct pci_dev *pdev,
           pci_channel_state_t state)
{
    struct xgpu_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

    switch (state) {
    case pci_channel_io_normal:
        return PCI_ERS_RESULT_CAN_RECOVER;
    case pci_channel_io_frozen:
        pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
            pdev, xpdev);
        pci_disable_device(pdev);
        return PCI_ERS_RESULT_NEED_RESET;
    case pci_channel_io_perm_failure:
        pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
            pdev, xpdev);
        return PCI_ERS_RESULT_DISCONNECT;
    }

    return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t xgpu_slot_reset(struct pci_dev *pdev)
{
    struct xgpu_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

    pr_info("0x%p restart after slot reset\n", xpdev);
    if (pci_enable_device_mem(pdev)) {
        pr_info("0x%p failed to renable after slot reset\n", xpdev);
        return PCI_ERS_RESULT_DISCONNECT;
    }

    pci_set_master(pdev);
    pci_restore_state(pdev);
    pci_save_state(pdev);

    return PCI_ERS_RESULT_RECOVERED;
}

static void xgpu_error_resume(struct pci_dev *pdev)
{
    struct xgpu_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);
    pr_info("dev 0x%p,0x%p error_resume\n", pdev, xpdev);
}

static void xgpu_reset_prepare(struct pci_dev *pdev)
{
    struct xgpu_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

    pr_info("dev 0x%p,0x%p reset_prepare\n", pdev, xpdev);
}

static void xgpu_reset_done(struct pci_dev *pdev)
{
    struct xgpu_pci_dev* xpdev = dev_get_drvdata(&pdev->dev);

    pr_info("dev 0x%p,0x%p reset_done\n", pdev, xpdev);
}

static const struct pci_error_handlers xgpu_err_handler = {
	.error_detected	= xgpu_error_detected,
	.slot_reset     = xgpu_slot_reset,
	.resume         = xgpu_error_resume,
	.reset_prepare  = xgpu_reset_prepare,
	.reset_done     = xgpu_reset_done,
};

static struct pci_driver xgpu_pci_driver = {
	.name        = AMD_GPU_DEV_NAME,
	.id_table    = amd_pci_ids,
	.probe       = probe_one,
	.remove      = remove_one,
	.err_handler = &xgpu_err_handler,
};

static int __init xgpu_mod_init(void)
{
    char version[] = DRV_MODULE_DESC ": " AMD_GPU_DEV_NAME " v" DRV_MODULE_VERSION;
    pr_info("%s\n", version);

    int rv = xgpu_cdev_init();
    if (rv < 0) {
        pr_err("failed in xgpu_cdev_init");
        return rv;
    }

    return pci_register_driver(&xgpu_pci_driver);
}

static void __exit xgpu_mod_exit(void)
{
    /* unregister this driver from the PCI bus driver */
    dbg_init("pci_unregister_driver %s\n", AMD_GPU_DEV_NAME);
    pci_unregister_driver(&xgpu_pci_driver);
    xgpu_cdev_cleanup();
}

module_init(xgpu_mod_init);
module_exit(xgpu_mod_exit);
