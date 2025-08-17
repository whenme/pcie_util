
#ifndef __XGPU_EXPORT_H__
#define __XGPU_EXPORT_H__

#include <linux/ioctl.h>
#include <linux/types.h>

typedef struct __register_read_write__
{
    uint32_t address;
    uint32_t value;
} register_read_write;

#define XGPU_IOCTL_MAGIC 'X'

#define XGPU_IOCTL_REGISTER_READ   _IOWR(XGPU_IOCTL_MAGIC, 1, register_read_write)
#define XGPU_IOCTL_REGISTER_WRITE  _IOWR(XGPU_IOCTL_MAGIC, 2, register_read_write)

#endif
