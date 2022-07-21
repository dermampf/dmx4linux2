# dmx4linux2 chardev examples

Here you find a set of small examples that show how to use aspects of the dmx4linux2 driver framework. They can be used with the kernel drivers as well as with the cuse-drivers.


dmxconsole/
  A small dmx512 console with simple crossfade support for playing with a device. This is taken from dmx4linux1 and adopted to dmx4linux2.


## t_dmx512-chardev-copy.c
  Copies frames from one devices port to another devices port.

## t_dmx512-chardev-info.c
  Displays info on a dmx-card.

## t_dmx512-chardev-read-one.c
  Read exactly one dmx frame.

## t_dmx512-chardev-read-poll.c
  Read dmx frames with the use of the poll call.

## t_dmx512-chardev-read-with-filter.c
  Reads dmx frames with a port filter set.

## t_dmx512-chardev-read.c
  Reads frames and dump them to stdout.

## t_dmx512-chardev-select.c
  Reads frames with the use of the select call.

## t_dmx512-chardev-set-filter-and-loop-get-filter.c
  Sets and gets port filters.

## t_dmx512-chardev-toomanyopen.c
  Opens a dmx card many times.

## t_dmx512-chardev-write-one.c
  Write one dmx frame.

## t_dmx512-chardev-write.c
 Write many dmx frames in a loop.

## t_dmx512-send-test-pattern.c
  Used to send a test pattern to a device. Often used during driver development for a new device.

## t_dmx512_recv_test_pattern.c
  Receive test pattern and output results.

## t_sinus.c
  Outputs a sinus on all dmx slots of a port on a dmx device.
