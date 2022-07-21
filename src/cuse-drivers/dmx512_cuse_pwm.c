#include <linux/dmx512/dmx512frame.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include "uio.h"
#include "parse_dmx_uio_args.h"

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



struct pwmdmx_card
{
    char  customer_name[64];
    char  port_label[64]; // no interface has more than 4 ports.
    char  uio_name[64];
    struct uio_handle * uio;
};



static const enum dmx4linux_tuple_id supported_card_tuples[] =
{
  DMX4LINUX2_ID_MANUFACTURER,
  DMX4LINUX2_ID_PRODUCT,
  DMX4LINUX2_ID_SERIAL_NUMBER,
  // DMX4LINUX2_ID_CUSTOMER_NAME,
};
static const int supported_card_tuples_count =
    sizeof(supported_card_tuples)/sizeof(*supported_card_tuples);

static int pwmdmx_queryCardInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_card_info *cardinfo)
{
    LOG ("pwmdmx_queryCardInfo\n");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);
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
                strcpy(cardinfo->tuples[i].value, "PWM");
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



static int pwmdmx_changeCardInfo (struct dmx512_cuse_card *card,
                                   struct dmx4linux2_card_info *cardinfo)
{
    LOG ("pwmdmx_changeCardInfo");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);

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

static int pwmdmx_queryPortInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_port_info *portinfo,
                                  int portIndex)
{
    LOG ("pwmdmx_queryPortInfo %d", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);

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


static int pwmdmx_changePortInfo (struct dmx512_cuse_card * card,
                                   struct dmx4linux2_port_info *portinfo,
                                   int portIndex)
{
    LOG ("pwmdmx_changePortInfo %d", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);

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

static int pwmdmx_init          (struct dmx512_cuse_card * card)
{
    LOG ("pwmdmx_init");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);
    LOG ("open port %s for pwm dmx", user->uio_name);
    user->uio = uio_open(user->uio_name, 0x1000);
    if (user->uio == 0)
      return -1;
    return 0;
}

static int pwmdmx_cleanup       (struct dmx512_cuse_card * card)
{
    LOG ("pwmdmx_cleanup");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);
    if (user->uio)
    {
        LOG ("close port %s for pwm dmx", user->uio_name);
        uio_close(user->uio);
    }
}


static int pwmdmx_sendFrame     (struct dmx512_cuse_card * card,
                                  struct dmx512frame *frame)
{
    if (frame)
    {
        struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
        struct pwmdmx_card * user = (struct pwmdmx_card *)(cc ? cc->userpointer : 0);

        if (user->uio && (frame->port == 0) && (frame->payload_size >0) && (frame->data[0] == 0))
        {
            int i;
            for (i = 0; (i < 512) && (i < frame->payload_size-1); ++i)
                uio_poke(user->uio,  i * 4, frame->data[i+1]);
            return 0;
        }
    }
    return -1;
}

static struct dmx512_cuse_card_ops pwmdmx_ops =
{
    .queryCardInfo  =     pwmdmx_queryCardInfo,
    .changeCardInfo =     pwmdmx_changeCardInfo,
    .queryPortInfo  =     pwmdmx_queryPortInfo,
    .changePortInfo =     pwmdmx_changePortInfo,
    .init          =      pwmdmx_init,
    .cleanup       =      pwmdmx_cleanup,
    .sendFrame     =      pwmdmx_sendFrame
};

int main(int argc, char** argv)
{
    const int first_cardno = getenv("DMXOFFSET") ? atoi(getenv("DMXOFFSET")) : 0;
    // -f: run in foreground, -d: debug ouput
    // Compile official example and use -h
    const char* cusearg[] = { "test", "-f" /*, "-d"*/ };

    static struct pwmdmx_card pwmdmx_card;
    struct dmx512_cuse_card_config card;
    bzero(&pwmdmx_card, sizeof(pwmdmx_card));
    bzero(&card, sizeof(card));

    strcpy(pwmdmx_card.uio_name, "/dev/");

    if (parse_uio_args(argc,
		       argv,
		       pwmdmx_card.uio_name,
		       &card.cardno,
		       "pwmctrl@40007000"))
      {
	  LOG ("uio device not found\n");
      }

    dmx512_core_init();

    card.userpointer = &pwmdmx_card;
    card.ops = &pwmdmx_ops;

    pwmdmx_card.uio = 0;
    strcpy(pwmdmx_card.customer_name, "customer label");
    snprintf(pwmdmx_card.port_label, 63, "PWM%d", card.cardno);

    const int ret = dmx512_cuse_lowlevel_main(
        sizeof(cusearg)/sizeof(cusearg[0]),
        (char**) &cusearg,
        &card, 1,
        NULL, 0);

    dmx512_core_exit();

    return ret;
}
