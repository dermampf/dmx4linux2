#include <linux/dmx512/dmx512framequeue.h>

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

static struct dmx512_framequeue_entry * _dmx512_framequeue_get (struct dmx512_framequeue * q);

void dmx512_framequeue_init(struct dmx512_framequeue *q)
{
    if (!q)
	return;
    INIT_LIST_HEAD(&q->head);
    spin_lock_init(&q->queue_lock);
}

void dmx512_framequeue_cleanup(struct dmx512_framequeue * q)
{
    struct dmx512_framequeue_entry *e;
    if (!q)
	return;
    spin_lock (&q->queue_lock);
    while ((e=_dmx512_framequeue_get (q)) != 0)
	dmx512_framequeue_entry_free(e);
    spin_unlock (&q->queue_lock);
}

int  dmx512_framequeue_isempty(struct dmx512_framequeue * q)
{
    int ret = 1;
    spin_lock (&q->queue_lock);
    ret = list_empty(&q->head);
    spin_unlock (&q->queue_lock);
    return ret;
}

static struct dmx512_framequeue_entry * _dmx512_framequeue_get (struct dmx512_framequeue * q)
{
    struct dmx512_framequeue_entry * e = NULL;
    if (!q)
	return 0;
    if (!list_empty(&q->head))
    {
        e = (struct dmx512_framequeue_entry *)(q->head.next);
        if (e)
            list_del(&e->head);
    }
    return e;
}

struct dmx512_framequeue_entry * dmx512_framequeue_get (struct dmx512_framequeue * q)
{
    struct dmx512_framequeue_entry * e = NULL;
    if (!q)
	return 0;
    spin_lock (&q->queue_lock);
    e = _dmx512_framequeue_get (q);
    spin_unlock (&q->queue_lock);
    return e;
}

static struct dmx512_framequeue_entry * _dmx512_framequeue_front (struct dmx512_framequeue * q)
{
    struct dmx512_framequeue_entry * e = NULL;
    if (!q)
	return 0;
    if (!list_empty(&q->head))
        e = (struct dmx512_framequeue_entry *)(q->head.next);
    return e;
}

/*
 * Do not get(remove) the entry, just return a reference to the front entry.
 */
struct dmx512_framequeue_entry * dmx512_framequeue_front (struct dmx512_framequeue * q)
{
    struct dmx512_framequeue_entry * e = NULL;
    if (!q)
	return 0;
    spin_lock (&q->queue_lock);
    e = _dmx512_framequeue_front (q);
    spin_unlock (&q->queue_lock);
    return e;
}

int  dmx512_framequeue_put(struct dmx512_framequeue * q, struct dmx512_framequeue_entry * e)
{
    if (!q || !e)
	return -1;
    spin_lock (&q->queue_lock);
    list_add_tail(&e->head, &q->head);
    spin_unlock (&q->queue_lock);
    return 0;
}

struct dmx512_framequeue_entry * dmx512_framequeue_entry_alloc()
{
    struct dmx512_framequeue_entry * e = kzalloc(sizeof(*e), GFP_KERNEL);
    if (e) {
	INIT_LIST_HEAD(&e->head);
    }
    return e;
}

void dmx512_framequeue_entry_free(struct dmx512_framequeue_entry * e)
{
    if (!e)
	return;
    // list_del(&e->head); /* remove from list, if not allready done. */
    kfree(e);
}
