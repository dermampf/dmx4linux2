ifneq ($(KERNELRELEASE),)
include Kbuild
else

SRC := $(shell pwd)
KERNEL_SRC ?= /lib/modules/`uname -r`/build

modules clean:
	$(MAKE) -C $(KERNEL_SRC) M=$$PWD $@

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

endif
