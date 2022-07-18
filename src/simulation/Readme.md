# dmx uart simulation

## build requirements

You need verilator to build the simulated 16550 uart.

## build
First build the hardware-model and then the cuse based userspace driver.

git submodule init
git submodule update
( cd hardware-model && make )
make

