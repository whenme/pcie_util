#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>

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
    return 0;
}
