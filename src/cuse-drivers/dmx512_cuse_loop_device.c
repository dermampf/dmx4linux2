// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "dmx512_cuse_dev.h"


struct testdmx_card
{
    struct dmx512_cuse_card *other_card;
    int   num_ports;
    char  customer_name[64];
    char  port_labels[16][64];
};


static struct testdmx_card testdmx_cards[2];

static const enum dmx4linux_tuple_id supported_card_tuples[] =
{
  DMX4LINUX2_ID_MANUFACTURER,
  DMX4LINUX2_ID_PRODUCT,
  DMX4LINUX2_ID_SERIAL_NUMBER,
  DMX4LINUX2_ID_CUSTOMER_NAME,
};
static const int supported_card_tuples_count =
    sizeof(supported_card_tuples)/sizeof(*supported_card_tuples);

static int testdmx_queryCardInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_card_info *cardinfo)
{
    printf ("testdmx_queryCardInfo\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct testdmx_card * user = (struct testdmx_card *)(cc ? cc->userpointer : 0);
    if (user == 0)
        return -1;

    cardinfo->card_index = cc->cardno;
    cardinfo->port_count = user->num_ports;
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
                strcpy(cardinfo->tuples[i].value, "llg-Dmx4linux2");
                break;
            case DMX4LINUX2_ID_PRODUCT:
                strcpy(cardinfo->tuples[i].value, "dummydmx");
                break;
            case DMX4LINUX2_ID_SERIAL_NUMBER:
                strcpy(cardinfo->tuples[i].value, "0123456789");
                break;
            case DMX4LINUX2_ID_CUSTOMER_NAME:
                strncpy(cardinfo->tuples[i].value,
                        user->customer_name,
                        sizeof(cardinfo->tuples[i].value));
                cardinfo->tuples[i].changable = 1;
                cardinfo->tuples[i].maxlength = sizeof(user->customer_name)-1;
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



static int testdmx_changeCardInfo (struct dmx512_cuse_card *card,
                                   struct dmx4linux2_card_info *cardinfo)
{
    printf ("testdmx_changeCardInfo\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct testdmx_card * user = (struct testdmx_card *)(cc ? cc->userpointer : 0);

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

static int testdmx_queryPortInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_port_info *portinfo,
                                  int portIndex)
{
    printf ("testdmx_queryPortInfo %d\n", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct testdmx_card * user = (struct testdmx_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if ((portIndex >= user->num_ports) || (portIndex >= 16))
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
                        user->port_labels[portIndex],
                        sizeof(portinfo->tuples[i].value));
                portinfo->tuples[i].changable = 1;
                portinfo->tuples[i].maxlength = sizeof(user->port_labels[0])-1;
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


static int testdmx_changePortInfo (struct dmx512_cuse_card * card,
                                   struct dmx4linux2_port_info *portinfo,
                                   int portIndex)
{
    printf ("testdmx_changePortInfo %d\n", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct testdmx_card * user = (struct testdmx_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if ((portIndex >= user->num_ports) || (portIndex >= 16))
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
                strncpy(user->port_labels[portIndex],
                        portinfo->tuples[i].value,
                        sizeof(user->port_labels[0]));
                break;

            default:
                break;
            }
    }
    return 0;
}


static int testdmx_init          (struct dmx512_cuse_card * card)
{
    printf ("testdmx_init\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    testdmx_cards[cc->cardno ? 0 : 1].other_card = card;
}

static int testdmx_cleanup       (struct dmx512_cuse_card * card)
{
    printf ("testdmx_cleanup\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    testdmx_cards[cc->cardno ? 0 : 1].other_card = 0;
}

static int testdmx_sendFrame     (struct dmx512_cuse_card * card,
                                  struct dmx512frame *frame)
{
    printf ("testdmx_sendFrame\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct testdmx_card * user = (struct testdmx_card *)(cc ? cc->userpointer : 0);
    dmx512_cuse_handle_received_frame(user->other_card,
                                      frame);
}

static struct dmx512_cuse_card_ops testdmx_ops =
{
    .queryCardInfo  =     testdmx_queryCardInfo,
    .changeCardInfo =     testdmx_changeCardInfo,
    .queryPortInfo  =     testdmx_queryPortInfo,
    .changePortInfo =     testdmx_changePortInfo,
    .init          =      testdmx_init,
    .cleanup       =      testdmx_cleanup,
    .sendFrame     =      testdmx_sendFrame
};

int main(int argc, char** argv)
{
    // -f: run in foreground, -d: debug ouput
    // Compile official example and use -h
    const char* cusearg[] = { "test", "-f" /*, "-d"*/ };

    if (argc == 2 && strcmp(argv[1], "--help")==0)
    {
        printf ("usage: %s dmx512_cuse_dev <cardno> <cardno>\n", argv[0]);
        return 1;
    }
    dmx512_core_init();

    bzero(&testdmx_cards, sizeof(testdmx_cards));

    struct dmx512_cuse_card_config cards[2] =
        {
            {
                .cardno = 0,
                .userpointer = &testdmx_cards[0],
                .ops = &testdmx_ops
            },
            {
                .cardno = 1,
                .userpointer = &testdmx_cards[1],
        .ops = &testdmx_ops
            }
        };

    const int num_cards =
        sizeof(testdmx_cards) / sizeof(*testdmx_cards);
    const int num_ports =
        sizeof(testdmx_cards[0].port_labels) / sizeof(*testdmx_cards[0].port_labels);

    int card;
    for (card = 0; card < num_cards; ++card)
    {
        testdmx_cards[card].num_ports = num_ports;
        strcpy(testdmx_cards[card].customer_name, "customer label");
        int i;
        for (i = 0; i < num_ports; ++i)
            snprintf(testdmx_cards[card].port_labels[i], 63, "Port-%c", 'A' + i);
    }

    const int ret = dmx512_cuse_lowlevel_main(
        sizeof(cusearg)/sizeof(cusearg[0]),
        (char**) &cusearg,
        cards, num_cards,
        NULL, 0);

    dmx512_core_exit();

    return ret;
}
