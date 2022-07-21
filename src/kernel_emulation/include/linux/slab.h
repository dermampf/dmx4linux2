#ifndef DEFINED_SLAB
#define DEFINED_SLAB

#include <stdlib.h>

#define GFP_ATOMIC (1)
#define GFP_KERNEL (2)
#define GFP_KERNEL_ACCOUNT (3)
#define GFP_NOWAIT (4)
#define GFP_NOIO (5)
#define GFP_NOFS (6)
#define GFP_USER (7)
#define GFP_DMA (8)
#define GFP_DMA32 (9)
#define GFP_HIGHUSER (10)
#define GFP_HIGHUSER_MOVABLE (11)
#define GFP_TRANSHUGE_LIGHT (12)
#define GFP_TRANSHUGE (13)

void * kzalloc(const size_t size, const unsigned long flags)
{
  (void)flags;
  return malloc(size);
}

void kfree(void * p)
{
  free(p);
}
  
#endif
