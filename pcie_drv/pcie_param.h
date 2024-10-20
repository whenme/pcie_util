#ifndef __PCIE_PARAM_H__
#define __PCIE_PARAM_H__

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/workqueue.h>


#define DRV_MOD_MAJOR		2024
#define DRV_MOD_MINOR		1
#define DRV_MOD_PATCHLEVEL	0

#define DRV_MODULE_VERSION      \
	__stringify(DRV_MOD_MAJOR) "." \
	__stringify(DRV_MOD_MINOR) "." \
	__stringify(DRV_MOD_PATCHLEVEL)

#define DRV_MOD_VERSION_NUMBER  \
	((DRV_MOD_MAJOR)*1000 + (DRV_MOD_MINOR)*100 + DRV_MOD_PATCHLEVEL)

#define AMD_PCIE_DEV_NAME     "amd_pcie"
#define DRV_MODULE_DESC       "Amd pcie Reference Driver"

MODULE_AUTHOR("Amd, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("Dual BSD/GPL");


#endif
