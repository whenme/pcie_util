
/**
 * @file amd_tng_dev/pcie_dev.c
 * @brief amd pcie device driver
 * @copyright Copyright © 2024 Advanced Micro Devices, Inc. All rights reserved.
 */
 
#include <linux/ioctl.h>
#include <linux/errno.h>

#include "pcie_param.h"

static const struct pci_device_id amd_pci_ids[] = {
    { PCI_DEVICE(0x10ee, 0x9048), },

    {0,}
};
MODULE_DEVICE_TABLE(pci, amd_pci_ids);

static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
    return 0;
}

static void remove_one(struct pci_dev *pdev)
{
}

static pci_ers_result_t amd_pci_error_detected(struct pci_dev *pdev,
					pci_channel_state_t state)
{
    switch (state) {
    case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
    case pci_channel_io_frozen:
        pr_warn("dev 0x%p, frozen state error, reset controller\n", pdev);
        pci_disable_device(pdev);
        return PCI_ERS_RESULT_NEED_RESET;
    case pci_channel_io_perm_failure:
        pr_warn("dev 0x%p, failure state error, req. disconnect\n", pdev);
        return PCI_ERS_RESULT_DISCONNECT;
    }

    return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t amd_pci_slot_reset(struct pci_dev *pdev)
{
	pr_info("0x%p restart after slot reset\n", pdev);
	if (pci_enable_device_mem(pdev)) {
		pr_info("0x%p failed to renable after slot reset\n", pdev);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void amd_pci_error_resume(struct pci_dev *pdev)
{
    pr_info("dev 0x%p error_resume\n", pdev);
}

static void amd_pci_reset_prepare(struct pci_dev *pdev)
{
	pr_info("dev 0x%p reset_prepare\n", pdev);
}

static void amd_pci_reset_done(struct pci_dev *pdev)
{
	pr_info("dev 0x%p reset_done\n", pdev);
}

static const struct pci_error_handlers amd_pci_err_handler = {
	.error_detected	= amd_pci_error_detected,
	.slot_reset     = amd_pci_slot_reset,
	.resume         = amd_pci_error_resume,
	.reset_prepare  = amd_pci_reset_prepare,
	.reset_done	    = amd_pci_reset_done,
};

static struct pci_driver amd_pci_driver = {
	.name        = AMD_PCIE_DEV_NAME,
	.id_table    = amd_pci_ids,
	.probe       = probe_one,
	.remove      = remove_one,
	.err_handler = &amd_pci_err_handler,
};

static int pcie_dev_init(void)
{
    return pci_register_driver(&amd_pci_driver);
}

static void pcie_dev_exit(void)
{
    pci_unregister_driver(&amd_pci_driver);
}

module_init(pcie_dev_init);
module_exit(pcie_dev_exit);
