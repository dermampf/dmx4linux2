// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
/*
 * This is a cuse client, that implements the dmx4linux2 uapi.
 * It is meant for testing new uapi features and for implementing
 * pure userspace drivers for which a kernel driver is not feasable.
 *
 * (C)2020 Michael Stickel
 */

#include <linux/dmx512/dmx512frame.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define FUSE_USE_VERSION 30
#define _FILE_OFFSET_BITS 64
#include <fuse/cuse_lowlevel.h>
#include <fuse/fuse_opt.h>
#include <fuse/fuse.h>

#include "dmx512_cuse_dev.h"


#define CONFIG_RXFRAMEQUEUE



enum { MAX_FD_WATCHERS = 256 };
static struct dmx512_cuse_fdwatcher g_fdwatchers[MAX_FD_WATCHERS];
static int g_num_fdwatchers = 0;


#ifdef CONFIG_RXFRAMEQUEUE
#include <linux/dmx512/dmx512framequeue.h>
#endif

struct dmx512_cuse_context
{
    /*
     * 1 if the context is in use
     * 0 if the context is unused.
     */
    int in_use : 1;
    int nonblocking : 1;

    /*
     * Mask of ports to monitor.
     * Bit0 = port1, Bit1=port2,...
     * Sums up to 64 ports. Should be enough for now.
     */
    unsigned long long port_mask;

    /*
     * same but for transmitted frames to loop back.
     */
    unsigned long long port_txmask;

    /*
     * Every context (open filehandle) should have not more than
     * one active read at a time, otherwise things can go wrong.
     */
    fuse_req_t         read_req;

    //-- !!!! we need a per context queue to store frames that come just like that.
    //-- We always write the new frame to the queue.
    //-- If we have an open read_req, we look at the start of the queue
    //-- and if it is not empty we complete the read_req with that frame.

    struct fuse_pollhandle *pollhandle;

#ifdef CONFIG_RXFRAMEQUEUE
    struct dmx512_framequeue  framequeue;
#else
    int pollnotify;
    struct dmx512frame lastframe; // rather create a queue that holds received frames. But it should be limmited and old frames shall be thrown away.
#endif
};

// number of parallel opens possible for one device.
#define DMX512_CUSE_CONTEXT_COUNT (32)

struct dmx512_cuse_card
{
    struct dmx512_cuse_card_config  config; // copy of the card parameter
    struct dmx512_cuse_context      contexts[DMX512_CUSE_CONTEXT_COUNT];
};


static int dmx512_cuse_context_index_valid(const int);

static struct dmx512_cuse_context * dmx512_cuse_context(struct dmx512_cuse_card *,
                                                        const int);

static struct dmx512_cuse_card * dmx512_cuse_req_card(fuse_req_t);



#ifdef CONFIG_RXFRAMEQUEUE
struct dmx512_framequeue free_framequeue;

// get a new frame from the pool of this card.
struct dmx512_framequeue_entry * dmx512_get_frame(struct dmx512_cuse_card * card)
{
    (void)card;
    /* we may later use the port to control the per port usage of frames
       or have some accounting. */
    return dmx512_framequeue_get (&free_framequeue);
}

// return a frame to the pool of this card.
void dmx512_put_frame(struct dmx512_cuse_card * card, struct dmx512_framequeue_entry * frame)
{
    (void)card;
    /* we may later use the port to control the per port usage of frames
       or have some accounting. */
    dmx512_framequeue_put(&free_framequeue, frame);
}
#endif


void dmx512_cuse_send_frame(struct dmx512_cuse_card *card,
                            struct dmx512frame *frame)
{
    if (card && card->config.ops && card->config.ops->sendFrame)
        card->config.ops->sendFrame(card, frame);
}

void dmx512_cuse_handle_received_frame(struct dmx512_cuse_card *card,
                                       struct dmx512frame *frame)
{
    int i;
    for (i = 0; i < DMX512_CUSE_CONTEXT_COUNT; ++i)
    {
        struct dmx512_cuse_context * ctx = dmx512_cuse_context(card, i);
        if (ctx->read_req && fuse_req_interrupted(ctx->read_req))
        {
            fuse_reply_err(ctx->read_req, EINTR);
            ctx->read_req = 0;
        }

	const unsigned long long port_mask =
	    (frame->flags & DMX512_FLAGS_IS_TRANSMIT_FRAME) ? ctx->port_txmask : ctx->port_mask;

        if (ctx->in_use)
	{
	    if ((frame->port < 64) && (port_mask & (1<<frame->port)))
	    {
		// TODO: can we have a rewad_req as well as a pollhandle?
		// if that is true, then we need to execute both paths.
		if (ctx->read_req)
		{
		    fuse_reply_buf(ctx->read_req, (void*)frame, sizeof(*frame));
		    ctx->read_req = 0;
		}
		else if (ctx->pollhandle)
		{
#ifdef CONFIG_RXFRAMEQUEUE
		    struct dmx512_framequeue_entry * e = dmx512_get_frame(card);
		    if (e)
		    {
			memcpy(&e->frame, frame, sizeof(*frame));
			dmx512_framequeue_put(&ctx->framequeue, e);
		    }
#else
		    //ctx->lastframe = *frame;
		    memcpy(&ctx->lastframe, frame, sizeof(*frame));
		    ctx->pollnotify++;
#endif
		    fuse_notify_poll(ctx->pollhandle);
		}
	    }
	}
    }
}



static int dmx512_cuse_free_context_index(struct dmx512_cuse_card * dmx512)
{
    int i;
    for (i = 0; i < DMX512_CUSE_CONTEXT_COUNT; ++i)
        if (dmx512->contexts[i].in_use == 0)
            return i;
    return -1;
}

static int dmx512_cuse_context_index_valid(const int index)
{
    return index >= 0 && index < DMX512_CUSE_CONTEXT_COUNT;
}

static struct dmx512_cuse_context * dmx512_cuse_context(struct dmx512_cuse_card * dmx512,
                                                        const int index)
{
    if (dmx512 && dmx512_cuse_context_index_valid(index))
        return &dmx512->contexts[index];
    return 0;
}

static struct dmx512_cuse_card * dmx512_cuse_req_card(fuse_req_t req)
{
    return (struct dmx512_cuse_card *)fuse_req_userdata(req);
}

static void dmx512_cuse_open(fuse_req_t req,
                             struct fuse_file_info *fi)
{
    struct dmx512_cuse_card * dmx512 = dmx512_cuse_req_card(req);
    const int index = dmx512 ? dmx512_cuse_free_context_index(dmx512) : -1;
    if (index == -1)
    {
        fprintf(stderr, "no free context\n");
        fuse_reply_err(req, ENOMEM);
        return;
    }
    struct dmx512_cuse_context * ctx = dmx512_cuse_context(dmx512, index);
    bzero(ctx, sizeof (struct dmx512_cuse_context));
    ctx->in_use = 1;
    ctx->port_mask = 0; // no port selected // 0xffffffffffffffff; // -1
    ctx->nonblocking = (fi->flags & O_NONBLOCK) ? 1 : 0;
#ifdef CONFIG_RXFRAMEQUEUE
    dmx512_framequeue_init(&ctx->framequeue);
#endif
    printf ("context%d activated %s\n",
	    index, (ctx->nonblocking ? "nonblocking" : "blocking"));
    fi->fh = index;
    fuse_reply_open(req, fi);
}

static void dmx512_cuse_release (fuse_req_t req,
                                 struct fuse_file_info *fi)
{
    struct dmx512_cuse_context * ctx = dmx512_cuse_context(dmx512_cuse_req_card(req), fi->fh);
    if (!ctx)
    {
        fuse_reply_err(req, EINVAL);
        return;
    }
    ctx->in_use = 0;
    ctx->port_mask = 0;
#ifdef CONFIG_RXFRAMEQUEUE
    dmx512_framequeue_cleanup(&ctx->framequeue);
#endif
    printf ("context deactivated\n");
    fuse_reply_err(req, 0);
}


static void dmx512_cuse_read(fuse_req_t req,
                             size_t size,
                             off_t  off,
                             struct fuse_file_info *fi)
{
    if (!fi)
    {
        printf("dmx512_cuse_read: fi=%p\n", fi);
        fuse_reply_err(req, EINVAL);
        return;
    }

    struct dmx512_cuse_context * ctx = dmx512_cuse_context(dmx512_cuse_req_card(req), fi->fh);
    if (!ctx)
    {
        fprintf(stderr, "dmx512_cuse_read fi is NULL: size:%lu off:%lu\n", size, off);
        fuse_reply_err(req, EINVAL);
        return;
    }


#ifdef CONFIG_RXFRAMEQUEUE
    if (!dmx512_framequeue_isempty(&ctx->framequeue))
    {
	struct dmx512_framequeue_entry * e = dmx512_framequeue_get (&ctx->framequeue);
	if (e)
	{
	    fuse_reply_buf(req, (void*)(&e->frame), sizeof(e->frame));
	    dmx512_put_frame(dmx512_cuse_req_card(req), e);
	}
    }
#else
    const int data_available = ctx->lastframe.payload_size > 0;
    if (data_available)
    {
        fuse_reply_buf(req, (void*)(&ctx->lastframe), sizeof(ctx->lastframe));
        bzero(&ctx->lastframe, sizeof(ctx->lastframe));
    }
#endif
    else // no data available
    {
        if (ctx->nonblocking)
             fuse_reply_buf(req, 0, 0);
        else
        {
            //-- the client context also holds the filters.
            //-- only if a buffer mathing the filter is available, it is forwarded.
            //queue_read_request(dmx512_client_context, req, size, off);
            if (ctx->read_req)
            {
                // should not have two concurrent reads.
                // ??? fuse_req_interrupted(ctx->read_req)
                fuse_reply_err(ctx->read_req, EINTR);
                ctx->read_req = 0; //TODO:this may be useless.
            }
            ctx->read_req = req;
        }
    }
}

static void dmx512_cuse_write(fuse_req_t req,
                              const char *buf,
                              size_t size,
                              off_t off,
                              struct fuse_file_info *fi)
{
    if (!fi)
    {
        fprintf(stderr, "dmx512_cuse_write fi is NULL: buf:%p size:%lu off:%lu\n", buf, size, off);
        fuse_reply_err(req, EINVAL);
        return;
    }

    struct dmx512_cuse_context * ctx = dmx512_cuse_context(dmx512_cuse_req_card(req), fi->fh);
    if (!ctx)
    {
        fprintf (stderr,"write: invalid context\n");
        fuse_reply_err(req, EINVAL);
        return;
    }

    struct dmx512frame * frame = (struct dmx512frame *)buf;
    if (size < sizeof(struct dmx512frame))
    {
        printf ("write: short dmx512 frame\n");
        fuse_reply_err(req, EINVAL);
        return;
    }

    frame->flags |= DMX512_FLAGS_IS_TRANSMIT_FRAME;

    dmx512_cuse_send_frame(dmx512_cuse_req_card(req), frame);
    dmx512_cuse_handle_received_frame(dmx512_cuse_req_card(req), frame);

    fuse_reply_write(req, size);
}


struct dmx512_cuse_card_config * dmx512_cuse_card_config(struct dmx512_cuse_card *card)
{
    return &card->config;
}

static int dmx512_card_query_info(struct dmx512_cuse_card * card,
                                  struct dmx512_cuse_context  * ctx,
                                  struct dmx4linux2_card_info * cardinfo)
{
    if (card && card->config.ops && card->config.ops->queryCardInfo)
        return card->config.ops->queryCardInfo(card, cardinfo);
    return EINVAL;
}

static int dmx512_card_change_info(struct dmx512_cuse_card * card,
                                   struct dmx512_cuse_context  * ctx,
                                   struct dmx4linux2_card_info * cardinfo)
{
    if (card && card->config.ops && card->config.ops->changeCardInfo)
        return card->config.ops->changeCardInfo(card, cardinfo);
    return EINVAL;
}

static int dmx512_port_query_info(struct dmx512_cuse_card * card,
                                   struct dmx512_cuse_context  * ctx,
                                   struct dmx4linux2_port_info *portinfo,
                                   int portIndex)
{
    if (card && card->config.ops && card->config.ops->queryPortInfo)
        return card->config.ops->queryPortInfo(card, portinfo, portIndex);
    return EINVAL;
}

static int dmx512_port_change_info(struct dmx512_cuse_card * card,
                                   struct dmx512_cuse_context  * ctx,
                                   struct dmx4linux2_port_info * portinfo,
                                   int portIndex)
{
    if (card && card->config.ops && card->config.ops->changePortInfo)
        return card->config.ops->changePortInfo(card, portinfo, portIndex);
    return EINVAL;
}


static void dmx512_cuse_query_card_info(struct dmx512_cuse_context * ctx,
                                        fuse_req_t req,
                                        void *addr,
                                        const void *in_buf,
                                        size_t in_bufsz,
                                        size_t out_bufsz)
{
    struct dmx512_cuse_card * dmx512 = dmx512_cuse_req_card(req);

    const struct dmx4linux2_card_info * ci;
    const struct dmx4linux2_key_value_tuple *tuples_in;
    struct iovec in_iov[2], out_iov[2], iov[2];

    in_iov[0].iov_base = addr;
    in_iov[0].iov_len = sizeof(*ci);
    if (!in_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, 1, NULL, 0);
        return;
    }
    ci = in_buf;
    in_buf += sizeof(*ci);
    in_bufsz -= sizeof(*ci);
    tuples_in = in_buf;
    in_iov[1].iov_base = ci->tuples;
    in_iov[1].iov_len = ci->tuple_count * sizeof(struct dmx4linux2_key_value_tuple);
    out_iov[0].iov_base = addr;
    out_iov[0].iov_len = sizeof(struct dmx4linux2_card_info) - sizeof(void*);
    out_iov[1].iov_base = ci->tuples;
    out_iov[1].iov_len = ci->tuple_count * sizeof(struct dmx4linux2_key_value_tuple);
    if (!out_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, ci->tuples ? 2 : 1, out_iov, ci->tuples ? 2 : 1);
        return;
    }

    struct dmx4linux2_key_value_tuple *tuples = ci->tuples ? malloc(out_iov[1].iov_len) : 0;
    if (tuples)
        memcpy (tuples, tuples_in, out_iov[1].iov_len);
    struct dmx4linux2_card_info card_out = *ci;
    card_out.tuples = tuples;

    if (dmx512_card_query_info(dmx512, ctx, &card_out) != 0)
        fuse_reply_err(req, EINVAL);
    else
    {
        iov[0].iov_base = &card_out;
        iov[0].iov_len = sizeof(*ci) - sizeof(void*);
        iov[1].iov_base = tuples;
        iov[1].iov_len = out_iov[1].iov_len;
        fuse_reply_ioctl_iov(req, 0, iov, ci->tuples ? 2 : 1);
    }
    if (tuples)
        free(tuples);
}


static void dmx512_cuse_change_card_info(struct dmx512_cuse_context * ctx,
                                         fuse_req_t req,
                                         void *addr,
                                         const void *in_buf,
                                         size_t in_bufsz,
                                         size_t out_bufsz)
{
    struct dmx512_cuse_card * card = dmx512_cuse_req_card(req);
    struct dmx4linux2_card_info ci;
    struct iovec in_iov[2];

    in_iov[0].iov_base = addr;
    in_iov[0].iov_len = sizeof(ci);
    if (!in_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, 1, NULL, 0);
        return;
    }
    ci = *((const struct dmx4linux2_card_info*)in_buf);
    in_buf += sizeof(ci);
    in_bufsz -= sizeof(ci);
    in_iov[1].iov_base = ci.tuples;
    in_iov[1].iov_len = ci.tuple_count * sizeof(struct dmx4linux2_key_value_tuple);
    if ((ci.tuple_count > 0) && !in_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, 2, NULL, 0);
        return;
    }
    ci.tuples = (struct dmx4linux2_key_value_tuple *)in_buf;

    if (dmx512_card_change_info(card,
                                ctx,
                                &ci))
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    fuse_reply_ioctl(req, 0, 0, 0);
}

static void dmx512_cuse_query_port_info(struct dmx512_cuse_context * ctx,
                                        fuse_req_t req,
                                        void *addr,
                                        const void *in_buf,
                                        size_t in_bufsz,
                                        size_t out_bufsz)
{
    struct dmx512_cuse_card * dmx512 = dmx512_cuse_req_card(req);

    const struct dmx4linux2_port_info * pi;
    const struct dmx4linux2_key_value_tuple *tuples_in;
    struct iovec in_iov[2], out_iov[2], iov[2];

    in_iov[0].iov_base = addr;
    in_iov[0].iov_len = sizeof(*pi);
    if (!in_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, 1, NULL, 0);
        return;
    }
    pi = in_buf;
    in_buf += sizeof(*pi);
    in_bufsz -= sizeof(*pi);
    tuples_in = in_buf;
    in_iov[1].iov_base = pi->tuples;
    in_iov[1].iov_len = pi->tuple_count * sizeof(struct dmx4linux2_key_value_tuple);
    out_iov[0].iov_base = addr;
    out_iov[0].iov_len = sizeof(struct dmx4linux2_card_info) - sizeof(void*);
    out_iov[1].iov_base = pi->tuples;
    out_iov[1].iov_len = pi->tuple_count * sizeof(struct dmx4linux2_key_value_tuple);
    if (!out_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, pi->tuples ? 2 : 1, out_iov, pi->tuples ? 2 : 1);
        return;
    }

    struct dmx4linux2_key_value_tuple *tuples = pi->tuples ? malloc(out_iov[1].iov_len) : 0;
    if (tuples)
        memcpy (tuples, tuples_in, out_iov[1].iov_len);
    struct dmx4linux2_port_info port_out = *pi;
    port_out.tuples = tuples;

    if (dmx512_port_query_info(dmx512, ctx, &port_out, pi->port_index) != 0)
        fuse_reply_err(req, EINVAL);
    else
    {
        iov[0].iov_base = &port_out;
        iov[0].iov_len = sizeof(*pi) - sizeof(void*);
        iov[1].iov_base = tuples;
        iov[1].iov_len = out_iov[1].iov_len;
        fuse_reply_ioctl_iov(req, 0, iov, pi->tuples ? 2 : 1);
    }
    if (tuples)
        free(tuples);
}

static void dmx512_cuse_change_port_info(struct dmx512_cuse_context * ctx,
                                         fuse_req_t req,
                                         void *addr,
                                         const void *in_buf,
                                         size_t in_bufsz,
                                         size_t out_bufsz)
{
    struct dmx512_cuse_card * card = dmx512_cuse_req_card(req);
    struct dmx4linux2_port_info pi;
    struct iovec in_iov[2];

    in_iov[0].iov_base = addr;
    in_iov[0].iov_len = sizeof(pi);
    if (!in_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, 1, NULL, 0);
        return;
    }
    pi = *((const struct dmx4linux2_port_info*)in_buf);
    in_buf += sizeof(pi);
    in_bufsz -= sizeof(pi);
    in_iov[1].iov_base = pi.tuples;
    in_iov[1].iov_len = pi.tuple_count * sizeof(struct dmx4linux2_key_value_tuple);
    if ((pi.tuple_count > 0) && !in_bufsz)
    {
        fuse_reply_ioctl_retry(req, in_iov, 2, NULL, 0);
        return;
    }
    pi.tuples = (struct dmx4linux2_key_value_tuple *)in_buf;

    if (dmx512_port_change_info(card,
                                ctx,
                                &pi,
                                pi.port_index))
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    fuse_reply_ioctl(req, 0, 0, 0);
}

static void dmx512_cuse_ioctl(fuse_req_t req,
                              int cmd,
                              void *arg,
                              struct fuse_file_info *fi,
                              unsigned flags,
                              const void *in_buf,
                              size_t in_bufsz,
                              size_t out_bufsz)
{
    struct dmx512_cuse_context * ctx = dmx512_cuse_context(dmx512_cuse_req_card(req), fi->fh);
    if (!ctx)
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    switch(cmd)
    {
    case DMX512_IOCTL_VERSION:
        if (out_bufsz == 0)
        {
            struct iovec iov = { arg, sizeof(unsigned long) };
            fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
        }
        else
        {
            unsigned long value = DMX4LINUX2_VERSION;
            fuse_reply_ioctl(req, 0, &value, sizeof(unsigned long));
        }
        break;

    case DMX512_IOCTL_SET_PORT_FILTER:
        if (in_bufsz == 0)
        {
            struct iovec iov = { arg, sizeof(unsigned long long) };
            fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
        }
        else
        {
            unsigned long long value = *((unsigned long long*)in_buf);
            printf("DMX512_IOCTL_SET_PORT_FILTER=%08llX\n", value);
            ctx->port_mask = value;
            fuse_reply_ioctl(req, 0, NULL, 0);
        }
        break;

    case DMX512_IOCTL_GET_PORT_FILTER:
        if (out_bufsz == 0)
        {
            struct iovec iov = { arg, sizeof(unsigned long long) };
            fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
        }
        else
        {
            unsigned long long value = 0;
            value = ctx->port_mask;
            fuse_reply_ioctl(req, 0, &value, sizeof(unsigned long long));
        }
        break;

    case DMX512_IOCTL_SET_PORT_TXFILTER:
        if (in_bufsz == 0)
        {
            struct iovec iov = { arg, sizeof(unsigned long long) };
            fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
        }
        else
        {
            unsigned long long value = *((unsigned long long*)in_buf);
            printf("DMX512_IOCTL_SET_PORT_TXFILTER=%08llX\n", value);
            ctx->port_txmask = value;
            fuse_reply_ioctl(req, 0, NULL, 0);
        }
        break;

    case DMX512_IOCTL_GET_PORT_TXFILTER:
        if (out_bufsz == 0)
        {
            struct iovec iov = { arg, sizeof(unsigned long long) };
            fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
        }
        else
        {
            unsigned long long value = 0;
            value = ctx->port_txmask;
            fuse_reply_ioctl(req, 0, &value, sizeof(unsigned long long));
        }
        break;

    case DMX512_IOCTL_QUERY_CARD_INFO:
        dmx512_cuse_query_card_info(ctx,
                                    req,
                                    arg,
                                    in_buf,
                                    in_bufsz,
                                    out_bufsz);
        break;

    case DMX512_IOCTL_CHANGE_CARD_INFO:
        dmx512_cuse_change_card_info(ctx,
                                     req,
                                     arg,
                                     in_buf,
                                     in_bufsz,
                                     out_bufsz);
      break;

    case DMX512_IOCTL_QUERY_PORT_INFO:
        dmx512_cuse_query_port_info(ctx,
                                    req,
                                    arg,
                                    in_buf,
                                    in_bufsz,
                                    out_bufsz);
        break;

    case DMX512_IOCTL_CHANGE_PORT_INFO:
        dmx512_cuse_change_port_info(ctx,
                                     req,
                                     arg,
                                     in_buf,
                                     in_bufsz,
                                     out_bufsz);
      break;

    default:
        fuse_reply_err(req, EINVAL);
        printf ("unhandled ioctl\n");
        break;
    }
}

void dmx512_cuse_poll (fuse_req_t req,
                       struct fuse_file_info *fi,
                       struct fuse_pollhandle *ph)
{
    struct dmx512_cuse_context * ctx = dmx512_cuse_context(dmx512_cuse_req_card(req), fi->fh);
    if (!ctx)
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    // based on: https://sourceforge.net/p/fuse/mailman/message/30451918/
    if (ph)
    {
	struct fuse_pollhandle *oldph = ctx->pollhandle;
	ctx->pollhandle = ph;
	if (oldph)
            fuse_pollhandle_destroy(oldph);
    }

    unsigned revents = 0;
#ifdef CONFIG_RXFRAMEQUEUE
    if (!dmx512_framequeue_isempty(&ctx->framequeue))
#else
    if (ctx->pollnotify > 0)
#endif
    {
	revents |= POLLIN;
#ifndef CONFIG_RXFRAMEQUEUE
	--ctx->pollnotify;
#endif
    }
    fuse_reply_poll(req, revents);
}

static void dmx512_cuse_init (void *userdata,
                       struct fuse_conn_info *conn)
{
    printf ("dmx512_cuse_init userdata=%p\n  conn=%p{.major=%d, .minor=%d, .capable=%d}\n",
            userdata,
            conn,
            conn->proto_major,
            conn->proto_minor,
            conn->capable
        );
}

static void dmx512_cuse_init_done (void *userdata)
{
    printf ("dmx512_cuse_init_done userdata=%p\n",
            userdata);
    struct dmx512_cuse_card *card = (struct dmx512_cuse_card *)userdata;
    if (card->config.ops && card->config.ops->init)
        card->config.ops->init (card);
}

static void dmx512_cuse_destroy (void *userdata)
{
    printf ("dmx512_cuse_destroy userdata=%p\n",
            userdata);
    struct dmx512_cuse_card *card = (struct dmx512_cuse_card *)userdata;

    int i;
    for (i = 0; i < DMX512_CUSE_CONTEXT_COUNT; ++i)
    {
        struct dmx512_cuse_context * ctx = dmx512_cuse_context(card, i);
        if (ctx && ctx->read_req)
        {
            fuse_reply_err(ctx->read_req, EINTR);
            ctx->read_req = 0;
        }
    }

    if (card->config.ops && card->config.ops->cleanup)
        card->config.ops->cleanup (card);

    free(userdata);
}


static const struct cuse_lowlevel_ops cusetest_clop =
{
    .init       = dmx512_cuse_init,
    .init_done  = dmx512_cuse_init_done,
    .destroy    = dmx512_cuse_destroy,
    .open       = dmx512_cuse_open,
    .release    = dmx512_cuse_release,
    .read       = dmx512_cuse_read,
    .write      = dmx512_cuse_write,
    .ioctl      = dmx512_cuse_ioctl,
    .poll       = dmx512_cuse_poll,
};


static int fuse_multisession_loop(struct fuse_session **sessions,
                                  const int num_sessions,
                                  struct dmx512_cuse_fdwatcher * fdwatchers,
                                  int num_fdwatchers)
{
    struct fuse_session_data
    {
        struct fuse_session *session;
        struct fuse_chan *ch;
        size_t bufsize;
        char *buf;
    };

    struct fuse_session_data sd[10];
    {
        int i;
        for (i = 0; i < num_sessions; ++i)
        {
            struct fuse_session *se = sessions[i];
            struct fuse_chan *ch = fuse_session_next_chan(se, NULL);
            size_t bufsize = fuse_chan_bufsize(ch);
            char *buf = (char *) malloc(bufsize);
            if (!buf)
            {
                fprintf(stderr, "fuse: failed to allocate read buffer\n");
                return -1;
            }
            sd[i].session = se;
            sd[i].ch = ch;
            sd[i].bufsize = bufsize;
            sd[i].buf = buf;
        }
    }

    {
	int i;
	for (i = 0; i < num_fdwatchers && g_num_fdwatchers < MAX_FD_WATCHERS; ++i)
	    g_fdwatchers[g_num_fdwatchers++] = fdwatchers[i];
	//for (i = 0; i < num_fdwatchers; ++i)
	//   dmx512_cuse_add_fdwatcher(fdwatchers[i]);
    }

    unsigned int timout_counter = 0;
    int res = 0;
    int run = 1;
    while (run)
    {
        int i;
        for (i = 0; i < num_sessions; ++i)
            if (fuse_session_exited(sd[i].session))
                run = 0;
        if (run == 0)
            break;

        int n = -1;
        fd_set readfds;
        FD_ZERO(&readfds);
        
        for (i = 0; i < num_sessions; ++i)
        {
            const int fd = fuse_chan_fd(sd[i].ch);
            FD_SET(fd, &readfds);
            if (fd+1 > n)
                n = fd+1;
        }

        for (i=0; i < g_num_fdwatchers; ++i)
	{
	    const int fd = g_fdwatchers[i].fd;
	    if (fd != -1)
	    {
		FD_SET(fd, &readfds);
		if (fd+1 > n)
		    n = fd+1;
	    }
	}

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        const int ret = select(n, &readfds, 0, 0, &timeout);
        if (ret == 0)
        {
	    printf ("timeout %d\r", timout_counter++);
	    fflush(stdout);
        }
        if (ret <= 0)
            continue;

        for (i = 0; i < num_sessions; ++i)
        {
            struct fuse_chan *ch = sd[i].ch;
            const int fd = fuse_chan_fd(ch);

            if (FD_ISSET(fd, &readfds))
            {
                struct fuse_chan *tmpch = ch;
                struct fuse_buf fbuf = {
                    .mem = sd[i].buf,
                    .size = sd[i].bufsize,
                };
                res = fuse_session_receive_buf(sd[i].session, &fbuf, &tmpch);
                if (res == -EINTR)
                    continue;
                if (res <= 0)
                {
                    run = 0;
                    break;
                }
                fuse_session_process_buf(sd[i].session, &fbuf, tmpch);
            }
        }

        for (i=0; i < g_num_fdwatchers; ++i)
	{
	    const int fd = g_fdwatchers[i].fd;
            if (FD_ISSET(fd, &readfds))
            {
		g_fdwatchers[i].callback(fd, g_fdwatchers[i].user);
            }
	}
    }

    {
        int i;
        for (i = 0; i < num_sessions; ++i)
        {
            free(sd[i].buf);
            fuse_session_reset(sd[i].session);
        }
    }
    return res < 0 ? -1 : 0;
}

struct fuse_session *dmx512_cuse_create_session(int argc, char *argv[],
                                                struct dmx512_cuse_card_config *card_config)
{
    char devarg0[1024];
    const char *devarg[1] = { devarg0 };
    snprintf(devarg0, sizeof(devarg0), "DEVNAME=dmx-card%d", card_config->cardno);

    struct cuse_info ci;
    memset(&ci, 0x00, sizeof(ci));
    ci.flags = CUSE_UNRESTRICTED_IOCTL;
    ci.dev_info_argc = 1;
    ci.dev_info_argv = devarg;

    struct dmx512_cuse_card * userdata = malloc(sizeof(struct dmx512_cuse_card));
    bzero(userdata, sizeof(*userdata));
    userdata->config = *card_config;

    int multithreaded;
    struct fuse_session *se;
    const struct cuse_lowlevel_ops *clop = &cusetest_clop;
    se = cuse_lowlevel_setup(argc, argv, &ci, clop, &multithreaded,
                             userdata);
    printf ("multithreaded=%d\n", multithreaded);
    return se;
}

int dmx512_cuse_lowlevel_main(int argc, char *argv[],
                              struct dmx512_cuse_card_config *cards,
                              const int num_cards,
                              struct dmx512_cuse_fdwatcher * fdwatchers,
                              int num_fd_watchers)
{
        int i;
        struct fuse_session *sessions[num_cards];
        int num_sessions = 0;

        for (i = 0; i < num_cards; ++i)
        {
            struct fuse_session *se;
            se = dmx512_cuse_create_session(argc, argv,
                                            &cards[i]);
            if (se == NULL)
                return 1;
            sessions[i] = se;
            num_sessions++;
        }

        int res = fuse_multisession_loop(sessions,
                                         num_sessions,
                                         fdwatchers,
                                         num_fd_watchers
                                         );
        for (i = 0; i < num_sessions; ++i)
        {
            cuse_lowlevel_teardown(sessions[i]);
        }

        return (res == -1) ? 1 : 0;
}

static int dmx512_cuse_fdwatcher_find_fd(const int fd)
{
    int i;
    for (i = 0; g_num_fdwatchers < MAX_FD_WATCHERS; ++i)
	if (g_fdwatchers[i].fd == fd)
	    return i;
    return -1;
}


int dmx512_cuse_fdwatcher_add(struct dmx512_cuse_fdwatcher * w)
{
    if (dmx512_cuse_fdwatcher_find_fd(w->fd)!=-1)
	return -EINVAL;

    const int first_free_entry = dmx512_cuse_fdwatcher_find_fd(-1);
    if (first_free_entry < 0)
	return -ENOMEM;

    g_fdwatchers[first_free_entry] = *w;
    if (first_free_entry+1 > g_num_fdwatchers)
	g_num_fdwatchers = first_free_entry+1;

    return 0;
}

int dmx512_cuse_fdwatcher_remove(struct dmx512_cuse_fdwatcher * w)
{
    const int i = dmx512_cuse_fdwatcher_find_fd(w->fd);
    if (i < 0)
	return -EINVAL;
    g_fdwatchers[i].fd = -1;
    g_fdwatchers[i].callback = 0;
    g_fdwatchers[i].user = 0;
    return 0;
}



int dmx512_core_init(void)
{
        printf("loading dmx512 core\n");
#ifdef CONFIG_RXFRAMEQUEUE
        dmx512_framequeue_init(&free_framequeue);
        /* put some frame in the freequeue: 32 ports with 32 contexts w directions with 4 frames each. */
        int i;
        for (i = 0; i < 32*32*2*4; ++i)
                dmx512_framequeue_put(&free_framequeue, dmx512_framequeue_entry_alloc());
#endif
        return 0;
}

void dmx512_core_exit(void)
{
    printf("unloading dmx512 core\n");
#ifdef CONFIG_RXFRAMEQUEUE
    dmx512_framequeue_cleanup(&free_framequeue);
#endif
}
