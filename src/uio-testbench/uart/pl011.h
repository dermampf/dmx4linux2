#ifndef DEFINED_PL011
#define DEFINED_PL011

struct rtuart;
struct rtuart * pl011_create(const unsigned long base_clock);

#endif
