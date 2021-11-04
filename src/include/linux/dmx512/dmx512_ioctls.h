/*
 * Copyright (C) Michael Stickel <michael@cubic.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#ifndef DEFINED_DMX512_IOCTLS
#define DEFINED_DMX512_IOCTLS

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif

#define DMX4LINUX2_VERSION_MAJOR  3
#define DMX4LINUX2_VERSION_MINOR  0
#define DMX4LINUX2_VERSION_PATCH  0
#define DMX4LINUX2_VERSION_STR    "3.0.0"

#define DMX4LINUX2_VERSION        ((DMX4LINUX2_VERSION_MAJOR<<16)|(DMX4LINUX2_VERSION_MINOR<<8)|DMX4LINUX2_VERSION_PATCH)

enum dmx4linux_tuple_id {
    DMX4LINUX2_ID_NONE,
    DMX4LINUX2_ID_MANUFACTURER,  /* The name of the manufacturer */
    DMX4LINUX2_ID_PRODUCT,       /* The name of the product */
    DMX4LINUX2_ID_SERIAL_NUMBER, /* The serial number of the card, if available. */
    DMX4LINUX2_ID_CUSTOMER_NAME, /* If the card can store a customer defined name */
    DMX4LINUX2_ID_PORT_LABEL,    /* if there is a label on the port, this is it. E.g "A","B","C",... */
    DMX4LINUX2_ID_MAX
};

#define DMX4LINUX2_MAX_NAME_LEN (256)

struct dmx4linux2_key_value_tuple {
    unsigned int key; /* DMX4LINUX2_ID_..., use DMX4LINUX2_ID_NONE for the card driver to fill in whatever it has.
                      * If you pre set them in a query to the card, the card driver will fill in the contents. */

    /* On query this means, that the tuple can be changed */
    unsigned int changable : 1;
    
    /* maximum physical available length of the value
     * This is usefull, for example if you have a display that shows that string
     * but the display has a limmited number of characters available.
     */
    unsigned int maxlength : 9;

    char         value[DMX4LINUX2_MAX_NAME_LEN];
};

struct dmx4linux2_card_info {
    /* this is the index of the card, use -1 for the first card. */
    int    card_index;

    /* number of ports available on this card */
    int    port_count;

    /* the number of tuples available in <tuples>.
     * Set to 0 to read back the current number of tuples supplied by the card. */
    int    tuple_count;
    struct dmx4linux2_key_value_tuple * tuples;
};

struct dmx4linux2_port_info {
    int   card_index;
    int   port_index;

    int    tuple_count; /* same as for card with respect to port */
    struct dmx4linux2_key_value_tuple * tuples;
};


#define DMX512_IOCTL_BASE 'D'


enum {
    /* DMX4Linux2 - Generic Info */
    DMX512_VERSION = 0,

    /* leave 3..9 unused, because of dmx4linux1 */

    /* Lookup, Enumeration */
    DMX512_QUERY_CARD_INFO = 10,
    DMX512_CHANGE_CARD_INFO,
    DMX512_QUERY_PORT_INFO,
    DMX512_CHANGE_PORT_INFO,

    /* Open File */
    DMX512_GET_PORT_FILTER = 20,
    DMX512_SET_PORT_FILTER,

    DMX512_SET_RX_MATCH_FILTER,
    DMX512_REM_RX_MATCH_FILTER,
    DMX512_GET_RX_MATCH_FILTER,

    /* Buffer Management */
    DMX512_ALLOCATE_DMX_BUFFERS = 30,
    DMX512_ENQUEUE_DMX_BUFFER,
    DMX512_DEQUEUE_DMX_BUFFER,
};


#define DMX512_IOCTL_VERSION            _IOR(DMX512_IOCTL_BASE,  DMX512_VERSION, unsigned long) /* 'D'[31:24] major[23:16] minor[15:8]  patch[7:0] */
#define DMX512_IOCTL_QUERY_CARD_INFO    _IOWR(DMX512_IOCTL_BASE, DMX512_QUERY_CARD_INFO,  struct dmx4linux2_card_info)
#define DMX512_IOCTL_CHANGE_CARD_INFO   _IOWR(DMX512_IOCTL_BASE, DMX512_CHANGE_CARD_INFO, struct dmx4linux2_card_info)
#define DMX512_IOCTL_QUERY_PORT_INFO    _IOWR(DMX512_IOCTL_BASE, DMX512_QUERY_PORT_INFO,  struct dmx4linux2_port_info)
#define DMX512_IOCTL_CHANGE_PORT_INFO   _IOWR(DMX512_IOCTL_BASE, DMX512_CHANGE_PORT_INFO, struct dmx4linux2_port_info)

#define DMX512_IOCTL_GET_PORT_FILTER   _IOR(DMX512_IOCTL_BASE, DMX512_GET_PORT_FILTER, unsigned long long)
#define DMX512_IOCTL_SET_PORT_FILTER   _IOW(DMX512_IOCTL_BASE, DMX512_SET_PORT_FILTER, unsigned long long)

#define DMX512_IOCTL_SET_RX_MATCH_FILTER   _IOW(DMX512_IOCTL_BASE, DMX512_SET_RX_MATCH_FILTER, struct dmx512_rxfilter_info)
#define DMX512_IOCTL_REM_RX_MATCH_FILTER   _IOW(DMX512_IOCTL_BASE, DMX512_REM_RX_MATCH_FILTER, struct dmx512_rxfilter_info)
#define DMX512_IOCTL_GET_RX_MATCH_FILTER   _IOW(DMX512_IOCTL_BASE, DMX512_GET_RX_MATCH_FILTER, struct dmx512_rxfilter_info)

/*
 * RDM replies are routed to the file handles the requests came from.
 */

/*
 * future dmabuf interface can be inspired by v4l2.
 * enqueue dmx-receive-buffer, dmx-transmit-buffer, rdm-request-reply,
 * incomming-rdm-request
 */

#endif // DEFINED_DMX512_IOCTLS
