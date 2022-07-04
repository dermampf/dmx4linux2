// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>
#include <linux/dmx512/dmx512_ioctls.h>


#include <errno.h>
#include <fcntl.h> // ::open
#include <linux/serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> // ::ioctl
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h> // ::tcgetattr, ::cfmakeraw, ...
#include <time.h>
#include <unistd.h>

#include "dmx512_cuse_dev.h"

#define LOG(...) do { fprintf(stderr, __VA_ARGS__); puts(""); } while (0)



static int uartdmx_set_rts(int fh, int level)
{
    int status;

    if (ioctl(fh, TIOCMGET, &status) == -1)
    {
        perror("setRTS(): TIOCMGET");
        return 0;
    }
    if (level)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;
    if (ioctl(fh, TIOCMSET, &status) == -1)
    {
        perror("setRTS(): TIOCMSET");
        return 0;
    }
    return 1;
}

static int uartdmx_select_dmx_baudrate(const int fh)
{
    struct termios my_tios;
    tcgetattr   (fh, &my_tios);
    cfsetispeed (&my_tios, B38400);
    cfsetospeed (&my_tios, B38400);
    return tcsetattr (fh, TCSANOW, &my_tios);
}

static int uartdmx_select_break_baudrate(const int fh)
{
    struct termios my_tios;
    tcgetattr   (fh, &my_tios);
    cfsetispeed (&my_tios, B19200);
    cfsetospeed (&my_tios, B19200);
    return tcsetattr (fh, TCSANOW, &my_tios);
}



static int uartdmx_waitUntilTransmitterEmpty(const int fh,
                                             const int timeoutMicroseconds)
{
    unsigned int result = 0;
    /* After the write returned without an error, there may
       be bytes left in the outgoing fifo or the tx-buffer
       may not be empty. We need to wait until anything is
       send out.  We may assume that the TX buffer is half
       full. We can sleep for so long and then do some
       sleeps until the TX-Buffer is realy empty.  It is not
       worth going to sleep for a shorter period.  If this
       period is to high, we may */

    int timeoutUs = timeoutMicroseconds;
    while (ioctl (fh, TIOCSERGETLSR, &result)==0 && result != TIOCSER_TEMT)
    {
        if (usleep(500)==0)
            timeoutUs -= 500;
        if (timeoutUs < 0)
            return -1;
    }
    return 0;
}




static int uartdmx_configure_uart(const int fh)
{
    const int DMX_BAUDRATE = 250000;

    struct termios my_tios; /* read about all that stuff
                               with 'man cfsetospeed'    */
    struct serial_struct new_serinfo;

    if (fh==-1)
        return 5;

    // Set up divisor for 38400 Baud
    if (ioctl(fh, TIOCGSERIAL, &new_serinfo) < 0)
    {
        LOG("Cannot get serial info");
        return 1;
    }
    
    if (new_serinfo.baud_base < DMX_BAUDRATE)
    {
        LOG("Baudbase to low can not set DMX baudrate");
        return 2;
    }

    LOG("baud-base: %u", new_serinfo.baud_base);

    // Set divisor to DMX Baudrate
    new_serinfo.custom_divisor = new_serinfo.baud_base / DMX_BAUDRATE;

    LOG("custom-divisor:%u", new_serinfo.custom_divisor);

    // Select custom speed for 38400 Baud.
    new_serinfo.flags &= ~ASYNC_SPD_MASK;
    new_serinfo.flags |= ASYNC_SPD_CUST;

    if (ioctl(fh, TIOCSSERIAL, &new_serinfo) < 0)
    {
        LOG("Cannot set serial info");
        return 3;
    }

    // Select up Baudrate
    tcgetattr   (fh, &my_tios);
    cfmakeraw   (&my_tios);

    /* 250KBaud 8N2, no RTSCTS */
    cfsetispeed (&my_tios, B38400);
    cfsetospeed (&my_tios, B38400);

    /* My clock has been a 22.1184MHz crystal that I have exchanged by a 24MHz crystal.
     * If it does not work, we may use a clock with half of the speed (12MHz) and use B460800.
     */
    my_tios.c_cflag &= ~CSIZE;
    my_tios.c_cflag |= CS8;
    my_tios.c_cflag &= ~(PARENB|PARODD);
    my_tios.c_cflag |=CSTOPB;
    my_tios.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fh, TCSANOW, &my_tios))
        return 4;
    return 0;
}

static void uartdmx_send_break(const int fh)
{
#if 0
    ioctl(fh, TIOCSBRK, 0);
    if (usleep(1000))
    {
        LOG("Break error");
    }
    ioctl(fh, TIOCCBRK, 0);
#else
    unsigned char null_byte = 0;
    uartdmx_select_break_baudrate(fh);
    write(fh, &null_byte, 1);
    uartdmx_waitUntilTransmitterEmpty(fh,
                                      1000);
    uartdmx_select_dmx_baudrate(fh);
#endif
}





struct uartdmx_card
{
    char  customer_name[64];
    char  port_label[64]; // no interface has more than 4 ports.
    char  uartname[64];
    int   uartfd;
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

static int uartdmx_init          (struct dmx512_cuse_card * card)
{
    LOG ("uartdmx_init");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    LOG ("open port %s for uart dmx", user->uartname);
    user->uartfd = open(user->uartname, O_RDWR);
    if (user->uartfd < 0)
      return -1;
    return uartdmx_configure_uart(user->uartfd);
}

static int uartdmx_cleanup       (struct dmx512_cuse_card * card)
{
    LOG ("uartdmx_cleanup");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    if (user->uartfd >= 0)
    {
        LOG ("close port %s for uart dmx", user->uartname);
        close(user->uartfd);
    }
}


static int uartdmx_sendFrame     (struct dmx512_cuse_card * card,
                                  struct dmx512frame *frame)
{
    LOG ("uartdmx_sendFrame");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct uartdmx_card * user = (struct uartdmx_card *)(cc ? cc->userpointer : 0);
    // dmx512_cuse_handle_received_frame(user->other_card, frame);

    if ((user->uartfd >= 0) && (frame->port == 0))
    {
        uartdmx_set_rts(user->uartfd, 1);
        usleep(1000);
        
        if ((frame->flags & DMX512_FLAG_NOBREAK) == 0)
        {
            LOG("send break");
            uartdmx_send_break(user->uartfd);
            // make shure no data is in transmitter (wait for as much time as is nessesary to transmit data we wrote before).
            // sleep_absolute(user->time_data_transmitted);
            
            // Send Break
            // user->time_data_transmitted = now + 100us;
        }
#if 0
        // Send up to 513 bytes
        const int payload_size = (frame->payload_size > 513) ? 513 : frame->payload_size;
        write(user->uartfd, frame->data, frame->payload_size);
        uartdmx_waitUntilTransmitterEmpty(user->uartfd,
                                          frame->payload_size*48);
#endif

        uartdmx_set_rts(user->uartfd, 0);

        // user->time_data_transmitted = (user->time_data_transmitted > now) ? (user->time_data_transmitted + payload_size*44) : (now + payload_size*44);
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

int main(int argc, char** argv)
{
    // -f: run in foreground, -d: debug ouput
    // Compile official example and use -h
    const char* cusearg[] = { "test", "-f" /*, "-d"*/ };

    if (argc == 2 && strcmp(argv[1], "--help")==0)
    {
        LOG ("usage: %s dmx512_cuse_dev <cardno> <cardno>", argv[0]);
        return 1;
    }

    const int max_cards =
        sizeof(uartdmx_cards) / sizeof(*uartdmx_cards);
    const int num_cards = ((argc - 1) > max_cards) ? max_cards : (argc - 1);

    bzero(&uartdmx_cards, sizeof(uartdmx_cards));
    struct dmx512_cuse_card_config cards[max_cards];

    int card;
    for (card = 0; card < num_cards; ++card)
    {
        cards[card].cardno = card;
        cards[card].userpointer = &uartdmx_cards[card];
        cards[card].ops = &uartdmx_ops;

        uartdmx_cards[card].uartfd = -1;
        strcpy(uartdmx_cards[card].uartname, argv[1+card]);
        strcpy(uartdmx_cards[card].customer_name, "customer label");
        snprintf(uartdmx_cards[card].port_label, 63, "UART%d", card);
    }

    const int ret = dmx512_cuse_lowlevel_main(
        sizeof(cusearg)/sizeof(cusearg[0]),
        (char**) &cusearg,
        cards, num_cards,
        NULL, 0);

    return ret;
}
