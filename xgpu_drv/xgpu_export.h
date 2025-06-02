
#ifndef __XGPU_EXPORT_H__
#define __XGPU_EXPORT_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm-generic/ioctl.h>

typedef struct __register_read_write__
{
    uint32_t address;
    uint32_t value;
} register_read_write;

#define XGPU_IOCTL_REGISTER_READ   _IOWR('M', 1, register_read_write)
#define XGPU_IOCTL_REGISTER_WRITE  _IOWR('M', 2, register_read_write)

#endif

