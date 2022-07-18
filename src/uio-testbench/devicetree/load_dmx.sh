#!/bin/sh
rmmod uio_pdrv_genirq
modprobe uio_pdrv_genirq of_id=dmx512-uart16550
