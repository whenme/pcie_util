
# Full KVERSION used to find system build sources
KVERSION ?= $(shell uname -r)

SOURCES := xgpu_mod.c xgpu_cdev.c libxgpu.c cdev_ctrl.c
OBJS    := $(SOURCES:.c=.o)

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

TARGET_MODULE := xgpu

EXTRA_CFLAGS := -I. -D__LIBXDMA_DEBUG__

$(TARGET_MODULE)-objs := $(OBJS)
obj-m := $(TARGET_MODULE).o

default: banner all

banner:
	@echo "Building $(TARGET_MODULE) Driver"
	@echo "Target kernel version: $(KVERSION)"
	@echo

all:
	make -C /lib/modules/$(KVERSION)/build M=$(shell pwd) modules

install: all
	make -C /lib/modules/$(KVERSION)/build M=$(shell pwd) modules_install

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(shell pwd) clean
