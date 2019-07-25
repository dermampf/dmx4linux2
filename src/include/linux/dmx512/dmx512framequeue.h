#ifndef DEFINED_DMX512_FRAMEQUEUE
#define DEFINED_DMX512_FRAMEQUEUE

#include <linux/list.h>
#include <linux/spinlock.h>

#include <linux/dmx512/dmx512frame.h>

/*
 * Local types
 */
typedef struct dmx512_framequeue_entry
{
    struct list_head    head;
    struct dmx512frame  frame;
} dmx512_framequeue_entry_t;

typedef struct dmx512_framequeue
{
    spinlock_t       queue_lock;
    struct list_head head;
} dmx512_framequeue_t;

void dmx512_framequeue_init(struct dmx512_framequeue *);
void dmx512_framequeue_cleanup(struct dmx512_framequeue *);
int  dmx512_framequeue_isempty(struct dmx512_framequeue *);
struct dmx512_framequeue_entry * dmx512_framequeue_get (struct dmx512_framequeue *);
struct dmx512_framequeue_entry * dmx512_framequeue_front (struct dmx512_framequeue *);
int  dmx512_framequeue_put(struct dmx512_framequeue *, struct dmx512_framequeue_entry *);

struct dmx512_framequeue_entry * dmx512_framequeue_entry_alloc(void);
void dmx512_framequeue_entry_free(struct dmx512_framequeue_entry *);

#endif
