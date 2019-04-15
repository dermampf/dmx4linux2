ifneq ($(KERNELRELEASE),)
include Kbuild
else
KDIR ?= /lib/modules/`uname -r`/build
modules clean:
	$(MAKE) -C $(KDIR) M=$$PWD $@
endif
