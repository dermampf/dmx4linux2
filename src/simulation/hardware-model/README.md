This is the simulation of a 16550 uart using verilator.
All the verilator and WishboneBus magic is encapsulated and you
have the C functions wbStartup, wbCleanup, wbWriteMemory, wbReadMemory
and wbBindInterrupt available. They are described in hardware_model.h.

The idea is to have a virtual uart that can be used to develop and
improve the dmxuart driver without the need for real hardware.
