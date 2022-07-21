#ifndef DEFINED_SPINLOCK
#define DEFINED_SPINLOCK

typedef unsigned long spinlock_t;

static void spin_lock_init(spinlock_t * sl) { (void)sl;  }
static void spin_lock(spinlock_t * sl) { (void)sl;  }
static void spin_unlock(spinlock_t * sl) { (void)sl; }

#endif
