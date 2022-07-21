# cuse based dmx drivers

Cuse is a linux driver and userspace library to create drivers as applications in userspace that have special files visible in /dev/ thru which these applications can be accessed. You practically write a device driver in userspace.

In dmx4linux2 this is used to try new thing out, like new ioctl's or drivers for devices that can then be ported to kernel space.

## dmx512_cuse_dev.c
  The core that abstacs away most of the cuse stuff so that the drivers can concentrate on dmx.

## dmx512_cuse_loop_device.c
  A simple loopback driver, that opens two cards and loops the frame from one card to the other and vice versa.

## dmx512_cuse_llgip.c
  Implements a dmx device with the use of a dmx ip-core that implements a number of dmx-serialisers/-deserializers written in verilog.

## dmx512_cuse_pwm.c
  Write the slots to memory mapped registers that then are output somehow, for example via PWM ip cores.

## dmx512_cuse_uart.c
  An experiment to use the linux serial tty driver to output dmx.
