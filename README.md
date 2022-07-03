# dmx4linux2

dmx4linux2 is a dmx512/rdm device driver framework for linux.


## build the kernel drivers

If you build for your current platform, you can do so by typing "make" in the src folder. If you plan to build for another architecture, take a look into the file src/README.target.

The modules compiled with simply typing "make" can be found here:
drivers/dmx512/core/dmx512-core.ko
drivers/dmx512/boards/dmx512_dummy.ko

## loading the kernel drivers

There are two scripts in the "scripts" folder called load.sh and unload.sh to quickly load and unload the modules. Execute them from the src/ folder.

## installing the modules

To install them into your kernel module directory type "make install" in the src/ folder.

## purpose of the modules
dmx512-core.ko is the dmx512 core that creates the char devices /dev/dmx-card* once a the card driver registers itself with the dmx512-core. If is also responsible for handling the file operations made on /dev/dmx-card* and handling multiple clients and managing dmx-port filters on the devices.

dmx512_dummy.ko is a dummy device that creates to dmx-cards. The core then creates char devices for it, e.g. /dev/dmx-card0 and /dev/dmx-card1. the ports from the first and second card are interconnected. Frames send via port0 on one are received on port0 of the other card and the other way round. It is some kind of loopback dmx device.

## userspace examples

The folder examples/chardev contains some small userspace example to explain the application binding to the kernel drivers.

the examples are held as small as possible and are hopefully self explaining.

## cuse userspace dmx driver framework

The folder cuse-drivers contains a cuse based userspace driver framework and is meant as a playground to quickly try out new ABI features. dmx512_cuse_loop_device is the same as the kernel dummy driver but implemented in userspace. All toos from the examples/chardev folder can be used with the userspace drivers.

The cuse-drivers can be build by typing make in the cuse-drivers folder. The binaries are to be found in the cuse-drivers/obj folder. You need to call them via sudo because the need root rights to create the dmx char devices /dev/dmx-card* via cuse.

## uart based protoype driver in userspace

The folder uio-testbench contains a prototype driver for pc16c450/550 as well as an ARM pl11 uart. The pl11 uart can be found on the ARM based RaspberryPi. The PC16Cx50 is the uart usually used in PCs.

## RDM library and tools

Librdm contains a simple rdm library used to test the rdm capabilities of the dmx drivers.

