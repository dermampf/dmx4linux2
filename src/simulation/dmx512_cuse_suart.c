// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>
#include <linux/dmx512/dmx512_ioctls.h>


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "dmx512_cuse_dev.h"

#define LOG(...) do { fprintf(stderr, __VA_ARGS__); puts(""); } while (0)


#include "dmx512_suart.h"

struct uartdmx_card
{
    char  customer_name[64];
    char  port_label[64]; // no interface has more than 4 ports.
    struct dmx512suart_port * dmxport;
    struct dmx512_cuse_card * card;
};


static struct uartdmx_card uartdmx_cards[4]; // currently support only up to 4 cards

static const enum dmx4linux_tuple_id supported_card_tuples[] =
{
  DMX4LINUX2_ID_MANUFACTURER,
  DMX4LINUX2_ID_PRODUCT,
  DMX4LINUX2_ID_SERIAL_NUMBER,
  // DMX4LINUX2_ID_CUSTOMER_NAME,
};
static const int supported_card_tuples_count =
    sizeof(supported_card_tuples)/sizeof(*supported_card_tuples);

static int uartdmx_queryCardInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_card_info *cardinfo)
{
    LOG ("uartdmx_queryCardInfo\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    if (user == 0)
        return -1;

    cardinfo->card_index = cc->cardno;
    cardinfo->port_count = 1;
    if (cardinfo->tuples)
    {
        int i;
        for (i = 0; i < cardinfo->tuple_count; ++i)
        {
            if (cardinfo->tuples[i].key == DMX4LINUX2_ID_NONE)
                cardinfo->tuples[i].key = supported_card_tuples[i];
        }

        for (i = 0; (i < cardinfo->tuple_count) && (i < supported_card_tuples_count); ++i)
        {
            switch (cardinfo->tuples[i].key)
            {
            case DMX4LINUX2_ID_MANUFACTURER:
                strcpy(cardinfo->tuples[i].value, "LLG");
                break;
            case DMX4LINUX2_ID_PRODUCT:
                strcpy(cardinfo->tuples[i].value, "UART");
                break;
            case DMX4LINUX2_ID_SERIAL_NUMBER:
                strcpy(cardinfo->tuples[i].value, "????????");
                break;
            default:
                // Just make it empty, if it is not supported.
                strcpy(cardinfo->tuples[i].value, "");
                break;
            }
        }
    }
    else
    {
      cardinfo->tuple_count = sizeof(supported_card_tuples) / sizeof(*supported_card_tuples);
    }
    return 0;
}



static int uartdmx_changeCardInfo (struct dmx512_cuse_card *card,
                                   struct dmx4linux2_card_info *cardinfo)
{
    LOG ("uartdmx_changeCardInfo");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if (cardinfo->card_index != cc->cardno)
        return -1;

    if (cardinfo->tuples && cardinfo->tuple_count > 0)
    {
        int i;
        for (i = 0; i < cardinfo->tuple_count; ++i)
            switch (cardinfo->tuples[i].key)
            {
            case DMX4LINUX2_ID_CUSTOMER_NAME:
                strncpy(user->customer_name,
                        cardinfo->tuples[i].value,
                        sizeof(user->customer_name));
                break;

            default:
                break;
            }
    }
    return 0;
}

static const enum dmx4linux_tuple_id supported_port_tuples[] =
{
    DMX4LINUX2_ID_PORT_LABEL
};
static const int supported_port_tuples_count =
    sizeof(supported_port_tuples)/sizeof(*supported_port_tuples);

static int uartdmx_queryPortInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_port_info *portinfo,
                                  int portIndex)
{
    LOG ("uartdmx_queryPortInfo %d", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if (portIndex >= 1)
    {
        return -1;
    }

    portinfo->card_index = cc->cardno;
    portinfo->port_index = portIndex;

    if (portinfo->tuples && portinfo->tuple_count > 0)
    {
        int i;
        for (i = 0; (i < portinfo->tuple_count) && (i < supported_port_tuples_count); ++i)
        {
            if (portinfo->tuples[i].key == DMX4LINUX2_ID_NONE)
                portinfo->tuples[i].key = supported_port_tuples[i];
        }

        for (i = 0; i < portinfo->tuple_count; ++i)
        {
            switch (portinfo->tuples[i].key)
            {
            case DMX4LINUX2_ID_PORT_LABEL:
                strncpy(portinfo->tuples[i].value,
                        user->port_label,
                        sizeof(portinfo->tuples[i].value));
                portinfo->tuples[i].changable = 0;
                portinfo->tuples[i].maxlength = sizeof(user->port_label)-1;
                break;

            default:
                portinfo->tuples[i].key = DMX4LINUX2_ID_NONE;
                break;
            }
        }
    }
    else
        portinfo->tuple_count = sizeof(supported_port_tuples) / sizeof(*supported_port_tuples);
    return 0;
}


static int uartdmx_changePortInfo (struct dmx512_cuse_card * card,
                                   struct dmx4linux2_port_info *portinfo,
                                   int portIndex)
{
    LOG ("uartdmx_changePortInfo %d", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if (portIndex >= 1)
        return -1;

    if (portinfo->card_index != cc->cardno)
        return -1;
    
    if (portinfo->port_index != portIndex)
        return -1;

    if (portinfo->tuples && portinfo->tuple_count > 0)
    {
        int i;
        for (i = 0; i < portinfo->tuple_count; ++i)
            switch (portinfo->tuples[i].key)
            {
            case DMX4LINUX2_ID_PORT_LABEL:
                strncpy(user->port_label,
                        portinfo->tuples[i].value,
                        sizeof(user->port_label));
                break;

            default:
                break;
            }
    }
    return 0;
}



static void dmx512_receive_frame(void * handle,
                                 unsigned char * dmx_data,
                                 int dmxsize)
{
  printf ("dmx512_receive_frame\n");
    if (handle)
    {
        struct uartdmx_card * user = (struct uartdmx_card *)handle;
        if (user)
        {
            struct dmx512frame frame;
            memset(&frame, 0, sizeof(frame));
            frame.port  = 0;
            frame.flags = 0;
            frame.breaksize = 88/4; // As we can not tell how long the break was, lets assume 88us.
            frame.payload_size = (dmxsize < 513) ? dmxsize : 513;
            memcpy (frame.data, dmx_data, frame.payload_size);
            dmx512_cuse_handle_received_frame(user->card, &frame);
        }
    }
}


static int uartdmx_init          (struct dmx512_cuse_card * card)
{
    LOG ("uartdmx_init");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    // LOG ("open port %s for uart dmx", user->uartname);

    user->dmxport = dmx512suart_create_port (dmx512_receive_frame, user);
    user->card = card;
    if (!user->dmxport)
      return -1;
    return 0;
}

static int uartdmx_cleanup       (struct dmx512_cuse_card * card)
{
    LOG ("uartdmx_cleanup");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    dmx512suart_delete_port (user->dmxport);
}


static int uartdmx_sendFrame     (struct dmx512_cuse_card * card,
                                  struct dmx512frame *frame)
{
    LOG ("uartdmx_sendFrame");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    if (user->dmxport && (frame->port == 0) && (frame->payload_size <= 513))
    {
      /*dmx512suart_send_frame(user->dmxport,
                               frame->data,
                               frame->payload_size);
      */
        dmx512suart_set_dtr (user->dmxport, 1);
        usleep(50*000);
        dmx512suart_set_dtr (user->dmxport, 0);
        printf("OK\n");
    }
    else
      {
        if (!user->dmxport)
          printf ("dmxport is 0\n");
        if (frame->port)
          printf ("dmxport is not 0\n");
        if (frame->payload_size <= 513)
          printf ("frame size > 513\n");
      }
}

static struct dmx512_cuse_card_ops uartdmx_ops =
{
    .queryCardInfo  =     uartdmx_queryCardInfo,
    .changeCardInfo =     uartdmx_changeCardInfo,
    .queryPortInfo  =     uartdmx_queryPortInfo,
    .changePortInfo =     uartdmx_changePortInfo,
    .init          =      uartdmx_init,
    .cleanup       =      uartdmx_cleanup,
    .sendFrame     =      uartdmx_sendFrame
};


static void dmx512_suart_received_frame(struct dmx512suart_port *dmxport,
                                        unsigned char * dmx_data,
                                        int dmxsize)
{
}
  

int main(int argc, char** argv)
{
    // -f: run in foreground, -d: debug ouput
    // Compile official example and use -h
    const char* cusearg[] = { "test", "-f" /*, "-d"*/ };

    if (argc == 2 && strcmp(argv[1], "--help")==0)
    {
        LOG ("usage: %s dmx512_cuse_dev <cardno>", argv[0]);
        return 1;
    }

    bzero(&uartdmx_cards, sizeof(uartdmx_cards));

    struct dmx512_cuse_card_config card;

    card.cardno = (argc > 1) ? strtoul(argv[1], 0, 0) : 0;
    card.userpointer = &uartdmx_cards[0];
    card.ops = &uartdmx_ops;

    strcpy(uartdmx_cards[0].customer_name, "customer label");
    snprintf(uartdmx_cards[0].port_label, 63, "UART%d", 0);

    const int ret = dmx512_cuse_lowlevel_main(
        sizeof(cusearg)/sizeof(cusearg[0]),
        (char**) &cusearg,
        &card, 1,
        NULL, 0);

    return ret;
}
