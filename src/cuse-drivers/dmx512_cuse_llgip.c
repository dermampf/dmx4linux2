#define CONFIG_LEARN_RXSIZE
//#define CONFIG_RDM


#include <linux/dmx512/dmx512frame.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include "uio.h"

#include <errno.h>
#include <fcntl.h> // ::open
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> // ::ioctl
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>

#include "dmx512_cuse_dev.h"
#include "uio_dmxcontroller.h"

#ifdef CONFIG_RDM
#include "rdm_priv.h"
#endif


#define LOG(...) do { fprintf(stderr, __VA_ARGS__); puts("\n"); } while (0)

struct llgdmx_port
{
    unsigned int  state;
    struct dmx512frame frame;
    char  port_label[64];


    unsigned short expected_rx_frame_size;

#ifdef CONFIG_LEARN_RXSIZE
    struct
    {
	char enabled : 1;
	char locked : 1;
	unsigned short learnedsize;
	unsigned short lastsize;
	int repetion_count;
	int repetion_count_treshold;
    } learned_rx_framesize;
#endif
};

struct dmxllgip_card
{
    char  customer_name[64];
    char  uio_name[64];
    struct uio_handle * uio;
    struct dmx512_cuse_card *card;
    unsigned int num_ports;
    struct llgdmx_port  ports[32];
};

static struct dmxllgip_card dmxllgip_card;



static unsigned long read_rx_fifo(struct dmxllgip_card * user,
                                  const unsigned int portno)
{
    return read_fifo(user->uio, portno);
}



static const enum dmx4linux_tuple_id supported_card_tuples[] =
{
  DMX4LINUX2_ID_MANUFACTURER,
  DMX4LINUX2_ID_PRODUCT,
  DMX4LINUX2_ID_SERIAL_NUMBER,
  // DMX4LINUX2_ID_CUSTOMER_NAME,
};
static const int supported_card_tuples_count =
    sizeof(supported_card_tuples)/sizeof(*supported_card_tuples);

static int dmxllgip_queryCardInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_card_info *cardinfo)
{
    LOG ("dmxllgip_queryCardInfo");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);
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
                strcpy(cardinfo->tuples[i].value, "LLG");
                break;
            case DMX4LINUX2_ID_PRODUCT:
                strcpy(cardinfo->tuples[i].value, "dmxllgip");
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



static int dmxllgip_changeCardInfo (struct dmx512_cuse_card *card,
                                   struct dmx4linux2_card_info *cardinfo)
{
    LOG ("dmxllgip_changeCardInfo");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);

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

static int dmxllgip_queryPortInfo (struct dmx512_cuse_card *card,
                                  struct dmx4linux2_port_info *portinfo,
                                  int portIndex)
{
    LOG ("dmxllgip_queryPortInfo %d", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if (portIndex >= user->num_ports)
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
                        user->ports[portIndex].port_label,
                        sizeof(portinfo->tuples[i].value));
                portinfo->tuples[i].changable = 0;
                portinfo->tuples[i].maxlength = sizeof(user->ports[0].port_label)-1;
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

static int dmxllgip_changePortInfo (struct dmx512_cuse_card * card,
				    struct dmx4linux2_port_info *portinfo,
				    int portIndex)
{
    LOG ("dmxllgip_changePortInfo %d", portIndex);
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);

    if (user == 0)
        return -1;

    if (portIndex >= user->num_ports)
        return -1;

    if (portinfo->card_index != cc->cardno)
        return -1;

    if (portinfo->tuples && portinfo->tuple_count > 0)
    {
        int i;
        for (i = 0; i < portinfo->tuple_count; ++i)
            switch (portinfo->tuples[i].key)
            {
            case DMX4LINUX2_ID_PORT_LABEL:
                strncpy(user->ports[portIndex].port_label,
                        portinfo->tuples[i].value,
                        sizeof(user->ports[portIndex].port_label));
                break;

            default:
                break;
            }
    }
    return 0;
}


#ifdef CONFIG_RDM
static int calc_rdm_checksum(const unsigned char * data, const int size)
{
  (void)data;
  (void)size;
  return 0;
}

static int handle_rdm_rx_data(struct llgdmx_port * port,
			      struct dmx512frame *frame,
			      const int rxdata)
{
    // We received all data we expected.
    if (frame->payload_size == port->expected_rx_frame_size)
    {
	const unsigned short checksum =
	    frame->data[frame->payload_size-1] +
	    256*frame->data[frame->payload_size];
	const unsigned short calculated_checksum = calc_rdm_checksum(frame->data, frame->payload_size-2);
	if (calculated_checksum != checksum)
	    ; // checksum mismatch, set rdm checksum error flag in header.
	
	//frame->payload_size
    }

    switch (frame->payload_size)
    {
    case RDM_POS_SUBSTARTCODE:
	if (rxdata != SC_SUB_MESSAGE)
	{
	    frame->payload_size = -1;
	    port->state = 0;
	}
	break;

    case RDM_POS_LENGTH:
	port->expected_rx_frame_size = rxdata + 2 - 1;
	break;
    }
    return 0;
}
#endif

static void  llgdmx_check_rx_port ( struct dmxllgip_card * card, const int portno)
{
    struct llgdmx_port * port = &card->ports[portno];
    struct dmx512frame *frame = &port->frame;
    while (1)
    {
        const unsigned long x = read_rx_fifo(card, portno);
        if (DMX_STATUS_RXFIFOEMPTY(x))
           return;

        if (DMX_RXFIFO_BREAK(x))
        {
	    ++frame->unused[0];

#ifdef CONFIG_LEARN_RXSIZE
	    if ((frame->payload_size > 0) && (frame->startcode==0))
	    {
		if (!port->learned_rx_framesize.locked)
		{
		    if (port->learned_rx_framesize.enabled)
		    {
			printf ("framesize learning (repetion_count:%d, repetion_count_treshold:%d lastsize:%d  sizze:%d)\n",
				port->learned_rx_framesize.repetion_count,
				port->learned_rx_framesize.repetion_count_treshold,
				port->learned_rx_framesize.lastsize,
				port->learned_rx_framesize.learnedsize);

			if (frame->payload_size == port->learned_rx_framesize.lastsize)
			    port->learned_rx_framesize.repetion_count++;
			else
			    port->learned_rx_framesize.repetion_count = 0;
		    
			if (port->learned_rx_framesize.repetion_count > port->learned_rx_framesize.repetion_count_treshold)
			{
			    port->learned_rx_framesize.locked = 1;
			    port->learned_rx_framesize.learnedsize = port->learned_rx_framesize.lastsize;
			    printf ("go framesize lock on %d bytes\n",
				    port->learned_rx_framesize.learnedsize
				);
			}
			port->learned_rx_framesize.lastsize = frame->payload_size;
		    }
		}
		else
		{
		    if (frame->payload_size != port->learned_rx_framesize.lastsize)
		    {
			port->learned_rx_framesize.locked = 0;
			port->learned_rx_framesize.learnedsize = 512;
			port->learned_rx_framesize.lastsize = 512;
			port->learned_rx_framesize.repetion_count = 0;
			printf ("lost framesize lock due to mismatching framesize (expect:%d got:%d\n",
				port->learned_rx_framesize.learnedsize,
				frame->payload_size
			    );
		    }
		}
	    }
#endif

            if (frame->payload_size > 0)
	    {
                dmx512_cuse_handle_received_frame(card->card, frame);
		frame->payload_size = 0;
	    }

            port->state = 1;
            frame->port = portno;
            frame->flags = 0;
	    /* size of break in 4us units, can be up to 4*254us */
            frame->breaksize = DMX_RXFIFO_DATA(x) >> 2;
            frame->payload_size = 0;
        }
        else
          switch(port->state)
          {
          case 0:
#ifdef CONFIG_LEARN_RXSIZE
	      if (port->learned_rx_framesize.locked)
	      {
		  port->learned_rx_framesize.locked = 0;
		  port->learned_rx_framesize.learnedsize = 512;
		  port->learned_rx_framesize.lastsize = 512;
		  port->learned_rx_framesize.repetion_count = 0;
		  printf ("lost framesize lock due to overrun\n");
	      }
#endif
	      break;

          case 1: // startcode
             if ((DMX_RXFIFO_DATA(x)) == 0)
             {
                 frame->payload_size = 1;
                 frame->data[0] = DMX_RXFIFO_DATA(x);
                 port->state=2;
#ifdef CONFIG_LEARN_RXSIZE
		 // Can we make this a module, that can be reused.
		 if (port->learned_rx_framesize.locked)
		     port->expected_rx_frame_size = port->learned_rx_framesize.learnedsize;
		 else
		     port->expected_rx_frame_size = 520; // 513;
#endif
             }
#ifdef CONFIG_RDM
	     else if ((DMX_RXFIFO_DATA(x)) == SC_RDM)
	     {
                 frame->payload_size = 1;
                 frame->data[0] = DMX_RXFIFO_DATA(x);
	         port->state = 102;
		 port->expected_rx_frame_size = 255+2-1; // RDM_MAX_FRAME_SIZE;
	     }
#endif
             else
                 port->state = 0;
             break;

         case 2:
	     if (frame->payload_size < 513)
	     {
		 frame->data[frame->payload_size++] = DMX_RXFIFO_DATA(x);
#ifdef CONFIG_LEARN_RXSIZE
		 if (port->learned_rx_framesize.locked &&
		     (frame->payload_size == port->learned_rx_framesize.learnedsize))
		 {
		     dmx512_cuse_handle_received_frame(card->card, frame);
		     frame->payload_size = 0;
		     port->state = 0;
		 }
	     }
#endif
             break;

#ifdef CONFIG_RDM
         case 102:
             if (frame->payload_size >= port->expected_rx_frame_size)
	     {
	         // calculate checksum, compare it to the one received and send frame to framework.
                 port->state = 0;
		 frame->payload_size = 0;
	     }
             else
             {
		 frame->data[frame->payload_size] = DMX_RXFIFO_DATA(x);
		 if (handle_rdm_rx_data(port, frame, frame->data[frame->payload_size]))
		 {
		     frame->payload_size = 0;
		     port->state = 0;
		 }
		 else
		     frame->payload_size++;
             }
             break;
#endif
         default:
             LOG ("internal error\n");
             port->state=0;
             break;
         }
   }
}

static void llgdmx_check_rx (struct dmxllgip_card * card)
{
    const unsigned long irqstat = dmx_read_global_irqstatus(card->uio);
    int i;
    for (i = 0; i < card->num_ports; ++i)
	if (irqstat & (1<<i))
	    llgdmx_check_rx_port (card, i);
}

static int llgdmx_setup_receiver(struct dmxllgip_card * user)
{
    /* 1. Enable per port receiver interrupt cause
     * 2. Enable Global interrupts
     * 3. Enable UIO interrupt handling
     */
    // Enable the receiver on
    int i;
    for (i = 0; i < user->num_ports; ++i)
	// DMX_STATUS_RXIRQ | DMX_STATUS_RXFIFOIDLE | DMX_STATUS_TXFIFO_LEVEL(100) | DMX_STATUS_RXFIFO_LEVEL(513)
	write_fifo_config (user->uio, i, (1<<27) | (1<<26) | (100<<12) | 513 );
    
    // (1<<port_count)-1
    // 1 port:  (1<<1)-1 = 2-1  = 1  = 0x01
    // 2 ports: (1<<2)-1 = 4-1  = 3  = 0x03
    // 3 ports: (1<<3)-1 = 8-1  = 7  = 0x07
    // 4 ports: (1<<4)-1 = 16-1 = 15 = 0x0f
    dmx_write_global_irqenable(user->uio, (1<<user->num_ports)-1);

    uio_enable_interrupt(user->uio);
}

static void llgdmx_cleanup_receiver(struct dmxllgip_card * user)
{
    uio_disable_interrupt(user->uio);
    dmx_write_global_irqenable(user->uio, 0);
}

static int dmxllgip_init          (struct dmx512_cuse_card * card)
{
    LOG ("dmxllgip_init");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);
    if (user == 0)
	return -1;
    if (user->uio == 0)
      return -1;

    user->card = card;
    llgdmx_setup_receiver(user);

    return 0;
}


static int dmxllgip_cleanup       (struct dmx512_cuse_card * card)
{
    LOG ("dmxllgip_cleanup");
    struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
    struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);

    if (user->card)
    {
	if (user->uio)
	{
	    llgdmx_cleanup_receiver(user);
	    // uio_close(user->uio);
	}
        user->card = NULL;
    }
}

static void dmxllgip_ll_waitForTransmitterToBecomeEmpty (struct uio_handle * uio, const int port)
{
    unsigned long s;
    while (DMX_STATUS_TXFIFOEMPTY( (s=read_fifo_status(uio, port)) ) == 0)
    {
	usleep(88+DMX_STATUS_TXFIFO_LEVEL(s)*44);
    }
}

static void dmxllgip_ll_sendFrame (struct uio_handle * uio,
				   struct dmx512frame *frame)
{
    int i;

    if (uio==0 || frame==0)
	return;

    const int breaksize =
	(frame->breaksize<22) ? 22 :
	(frame->breaksize>63) ? 63 :
	frame->breaksize;
    write_fifo(uio, frame->port, 0x100+(breaksize<<2) + 3);
    write_fifo(uio, frame->port, frame->startcode);
    for (i=0; i < frame->payload_size; ++i)
	write_fifo(uio, frame->port, frame->payload[i]);
}


static int dmxllgip_sendFrame     (struct dmx512_cuse_card * card,
				   struct dmx512frame *frame)
{
    // LOG ("uiodmx_sendFrame(card:%p, frame:%p)", card, frame);
    if (frame)
    {
        struct dmx512_cuse_card_config * cc = dmx512_cuse_card_config(card);
        struct dmxllgip_card * user = (struct dmxllgip_card *)(cc ? cc->userpointer : 0);

	if (user->uio && (((unsigned int)frame->port) < 20) && (frame->payload_size > 0))
        {
	    dmxllgip_ll_waitForTransmitterToBecomeEmpty (user->uio, frame->port);
	    dmxllgip_ll_sendFrame (user->uio, frame);
            return 0;
        }
        else
            LOG ("uio:%p frame->port:%d  frame->payload_size: %d frame->data[0]:%02X",
                user->uio, frame->port, frame->payload_size, frame->data[0]);
    }
    else
        LOG ("frame is NULL");
    return -1;
}


static struct dmx512_cuse_card_ops dmxllgip_ops =
{
    .queryCardInfo  =     dmxllgip_queryCardInfo,
    .changeCardInfo =     dmxllgip_changeCardInfo,
    .queryPortInfo  =     dmxllgip_queryPortInfo,
    .changePortInfo =     dmxllgip_changePortInfo,
    .init          =      dmxllgip_init,
    .cleanup       =      dmxllgip_cleanup,
    .sendFrame     =      dmxllgip_sendFrame
};


static void handle_llgdmx_interrupt (int fd, void * user)
{
  struct dmxllgip_card * dmxllgip_card = (struct dmxllgip_card *)user;
  uio_wait_for_interrupt(dmxllgip_card->uio);
  llgdmx_check_rx(dmxllgip_card);
  uio_enable_interrupt(dmxllgip_card->uio);
}


int main(int argc, char** argv)
{
    int i;

    // -f: run in foreground, -d: debug ouput
    // Compile official example and use -h
    const char* cusearg[] = { "test", "-f" /*, "-d"*/ };

    if (argc != 3)
    {
        LOG ("usage: %s dmx512_cuse_llgip <uio device name> <dmx card no>", argv[0]);
	int print_uio_devices (int index, const char * uio, const char *name, void *user)
	{
	    printf ("/dev/%s : %s\n", uio, name);
	    return -1;
	}
	if (argc <= 2)
	{
	    iterate_uio(print_uio_devices, NULL); 
	    return 1;
	}
        return 1;
    }

    dmx512_core_init();
    
    struct dmx512_cuse_card_config card;

    bzero(&dmxllgip_card, sizeof(dmxllgip_card));
    bzero(&card, sizeof(card));

    card.cardno = atoi(argv[2]);
    card.userpointer = &dmxllgip_card;
    card.ops = &dmxllgip_ops;

    dmxllgip_card.uio = 0;
    strcpy(dmxllgip_card.uio_name, argv[1]);

    //== Open DMX-Hardware
    dmxllgip_card.uio = uio_open(dmxllgip_card.uio_name, 0x1000);
    if (dmxllgip_card.uio == 0)
      return 2;

    if (!is_dmx1_ipcore(dmxllgip_card.uio))
    {
	LOG ("not DMX1 ip-core\n");
	return 2;
    }

    const int port_count = dmx1_port_count(dmxllgip_card.uio);
    LOG ("port-count:%d\n", port_count);


    for (i=0; i< port_count; ++i)
    {
	dmxllgip_card.ports[i].learned_rx_framesize.repetion_count_treshold = 20;
	dmxllgip_card.ports[i].learned_rx_framesize.enabled = 1;
    }

    
    //== Create DMX-Device
    strcpy(dmxllgip_card.customer_name, "customer label");
    for (i=0; i< port_count; ++i)
        snprintf(dmxllgip_card.ports[i].port_label, 63, "LLGDMX%d.%d", card.cardno, i+1);
    dmxllgip_card.num_ports = 14; // port_count;

    struct dmx512_cuse_fdwatcher  uio_watcher =
      {
	dmxllgip_card.uio->fd,
	handle_llgdmx_interrupt,
	&dmxllgip_card
      };

    const int ret = dmx512_cuse_lowlevel_main(
        sizeof(cusearg)/sizeof(cusearg[0]),
        (char**) &cusearg,
        &card, 1,
        &uio_watcher, 1);

    dmx512_core_exit();

    return ret;
}
