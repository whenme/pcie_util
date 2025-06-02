#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include <sys/ioctl.h>

#include "../xgpu_export.h"

int main()
{
    int fd = open("/dev/xgpu0/reg/CfgMemSize", O_RDONLY);
    if (fd < 0) {
        perror("failed to open file");
        return -1;
    }

    uint32_t value = 0;
    ssize_t byteRead = read(fd, &value, sizeof(uint32_t));
    if (byteRead < 0) {
        perror("fail to read file");
        return -1;
    }

    printf("register read value: 0x%x size: %ld\n", value, byteRead);
    value += 1;
    write(fd, &value, sizeof(value));
    read(fd, &value, sizeof(uint32_t));
    printf("register write and read value: 0x%x\n", value);
    close(fd);

    fd = open("/dev/xgpu0/register", O_RDONLY);
    if (fd < 0) {
        perror("failed to open file");
        return -1;
    }
    register_read_write reg;
    reg.address = 0x378c;
    int ret = ioctl(fd, XGPU_IOCTL_REGISTER_READ, &reg);
    if (ret) {
        perror("fail in XGPU_IOCTL_REGISTER_READ");
    } else {
        printf("ioctl read register value: 0x%x\n", reg.value);
        reg.value++;
        ret = ioctl(fd, XGPU_IOCTL_REGISTER_WRITE, &reg);
        if (ret) {
            perror("fail in XGPU_IOCTL_REGISTER_READ");
        } else {
            ret = ioctl(fd, XGPU_IOCTL_REGISTER_READ, &reg);
            printf("ioctl after write, register value: 0x%x\n", reg.value);
        }
    }

    close(fd);
    return 0;
}
