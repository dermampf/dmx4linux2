#ifndef WBUART_16C50_H
#define WBUART_16C50_H

#include "pc16cx50.h"

struct pc16x50 * wbuart_pc16x50_create();
struct pc16x50 * wbuart_pc16x50_create_by_index(int index);
void wbuart_delete(struct pc16x50 * u);
void wbuart_set_irqhandler(struct pc16x50 * u,
                           void (*irqfunc)(int, void *),
                           void * irqarg);

#endif
