/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DMX512_FRAME_H
#define DMX512_FRAME_H

/*
 * Used from kernel and userspace.
 */
#ifdef __KERNEL__
#include <linux/types.h>
#include <uapi/linux/time.h>
#else
#include <stdint.h>
#include <time.h>
#endif

enum
  {
    DMX512_FLAG_NOBREAK = (1<<0), /* omit break on tx, no break on rx, eg rdm. */

    DMX512_FLAG_RDMCRC_UNKNOWN = (1<<1), /* */
    DMX512_FLAG_RDMCRC_VALID   = (2<<1), /* */
    DMX512_FLAG_RDMCRC_INVALID = (3<<1), /* */

    DMX512_FLAG_TIMESTAMP      = (1<<3), /* have front timestamp */
    DMX512_FLAG_BACK_TIMESTAMP = (1<<4), /* have back timestamp - on rx only. */

    DMX512_FLAG_IS_RDM      = (1<<6),/* RDM frame. This is also set if DMX512_FLAG_IS_RDM_DISC is set */
    DMX512_FLAG_IS_RDM_DISC = (1<<7) /* RDM Discover request or reply */
  };

struct dmx512frame
{
    /*
     * The port number this frame was received from or should be send out to.
     */
    uint16_t port;
    uint16_t flags; // up to 16 flags used for whatever, defaults to 0.

    uint8_t  breaksize;    /* size of break in 4us units, can be up to 4*254us */
    uint8_t  unused[3];

    uint16_t payload_size; /* number of octets in the payload. */

    /*-- this is the 64 bit boundary --*/
	union {
		struct {
			uint8_t  startcode;
			uint8_t  payload[512];
		};
		uint8_t  data[513];
	};

    /* @timestamp:
       for incoming frames this is closest to the beginning of the frame possible.
       If this is {0,0} it means the hardware is not able to create a timestamp and the option for software timestamping must be enabled.

       for outgoing frames this is the place in time it shall be send out.
       If this is {0,0} it means as soon as possible.
    */
    struct timespec timestamp; // timestamp near the start of the break.
    struct timespec back_timestamp; // timestamp near the end of the frame (after last octet).
} __attribute__((packed));

#endif // DMX512_FRAME_H
