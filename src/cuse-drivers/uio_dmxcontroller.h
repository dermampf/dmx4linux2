#ifndef DEFINED_UIO_DMXCONTROLLER_H
#define DEFINED_UIO_DMXCONTROLLER_H

#include "uio.h"

#define DMX_IPCOREIDENT_REGNO (0)
#define DMX_IPCOREIDENT_MAGIC (0x444D5801)
#define DMX_IPCORE_CONFIG_REGNO (1)
#define DMX_IPCORE_CONFIG_PORTCOUNT(x)  ((x)&63)
#define DMX_IRQ_STATUS_REGNO   (2)
#define DMX_IRQ_ENABLE_REGNO   (3)

#define  DMX_FIFODATA_REGNO   (0)
#define  DMX_PORTCONFIG_REGNO (1)
#define  DMX_STATUS_REGNO     (2)
#define  DMX_FIFOCONFIG_REGNO (3)

#define  DMX_RXFIFO_BREAK(s)    (((s)>>8)&1)
#define  DMX_RXFIFO_DATA(s)     ((s)&0xff)


#define  DMX_STATUS_TXIRQ(s)       (((s)>>31)&1)
#define  DMX_STATUS_TXFIFOFULL(s)  (((s)>>29)&1)
#define  DMX_STATUS_TXFIFOEMPTY(s) (((s)>>28)&1)

#define  DMX_STATUS_RXIRQ(s)       (((s)>>27)&1)
#define  DMX_STATUS_RXFIFOIDLE(s)  (((s)>>26)&1)
#define  DMX_STATUS_RXFIFOFULL(s)  (((s)>>25)&1)
#define  DMX_STATUS_RXFIFOEMPTY(s) (((s)>>24)&1)

#define  DMX_STATUS_TXFIFO_LEVEL(s)  (((s)>>12)&0xFFF)
#define  DMX_STATUS_RXFIFO_LEVEL(s)  ((s)&0xFFF)

#define DMX_CONFIG_TX_DISABLE  (1<<31)
#define DMX_CONFIG_RX_DISABLE  (1<<15)
#define DMX_CONFIG_RX_IDLE_DURATION_MASK    (15<<11)
#define DMX_CONFIG_RX_IDLE_DURATION(x)      (((x)&DMX_CONFIG_RX_IDLE_DURATION_MASK)>>11)
#define DMX_CONFIG_RX_IDLE_DURATION_SET(d)  (((d)<<11)&DMX_CONFIG_RX_IDLE_DURATION_MASK)
#define DMX_CONFIG_RX_MIN_BRK_LEN  (0x3F) // in bits (22 = 88us)


static unsigned long write_dmx_reg(struct uio_handle * uio, const int regno, const unsigned long v)
{
    uio_poke(uio, 4*regno, v);
}

static unsigned long read_dmx_reg(struct uio_handle * uio, const int regno)
{
    return uio_peek(uio, 4*regno);
}


static int dmx_portregister_regno(const int portno, const int regno)
{
    return 4*(1+portno) + regno;
}

static int dmx_portregister_offset(const int portno, const int regno)
{
  // return dmx_portregister_regno(portno, regno);
  return 4 * (4*(1+portno) + regno);
}


static unsigned long write_dmx_port_reg(struct uio_handle * uio, const int portno, const int regno, const unsigned long v)
{
  //write_dmx_reg(uio, dmx_portregister_regno(portno, regno), v);
  uio_poke(uio, dmx_portregister_offset(portno, regno), v);
}

static unsigned long read_dmx_port_reg(struct uio_handle * uio, const int portno, const int regno)
{
  // return read_dmx_reg(uio, dmx_portregister_regno(portno, regno));
    return uio_peek(uio, dmx_portregister_offset(portno, regno));
}

static void write_fifo(struct uio_handle * uio, const int portno, const unsigned long v)
{
    write_dmx_port_reg(uio, portno, DMX_FIFODATA_REGNO, v);
}

static unsigned long read_fifo(struct uio_handle * uio, const int portno)
{
    return read_dmx_port_reg(uio, portno, DMX_FIFODATA_REGNO);
}

static unsigned long read_fifo_status (struct uio_handle * uio, const int portno)
{
    return read_dmx_port_reg(uio, portno, DMX_STATUS_REGNO);
}

static unsigned long read_port_config (struct uio_handle * uio, const int portno)
{
    return read_dmx_port_reg(uio, portno, DMX_PORTCONFIG_REGNO);
}

static void write_port_config (struct uio_handle * uio, const int portno, const unsigned long v)
{
    write_dmx_port_reg(uio, portno, DMX_PORTCONFIG_REGNO, v);
}


static unsigned long read_fifo_config (struct uio_handle * uio, const int portno)
{
    return read_dmx_port_reg(uio, portno, DMX_FIFOCONFIG_REGNO);
}

static void write_fifo_config (struct uio_handle * uio, const int portno, const unsigned long v)
{
    write_dmx_port_reg(uio, portno, DMX_FIFOCONFIG_REGNO, v);
}


static int is_dmx1_ipcore(struct uio_handle * uio)
{
  const unsigned long ipcore_ident = read_dmx_reg(uio, DMX_IPCOREIDENT_REGNO);
  if (ipcore_ident != DMX_IPCOREIDENT_MAGIC)
    {
      printf ("illegal ip-core for this driver: %08lX\n", ipcore_ident);
      return 0;
    }
  printf ("IP-Core ok: %c%c%c%d\n",
	  (unsigned char)(ipcore_ident>>24),
	  (unsigned char)(ipcore_ident>>16),
	  (unsigned char)(ipcore_ident>>8),
	  (unsigned char)ipcore_ident
	  );
  return 1;
}


static int dmx1_port_count(struct uio_handle * uio)
{
  return DMX_IPCORE_CONFIG_PORTCOUNT(read_dmx_reg(uio, DMX_IPCORE_CONFIG_REGNO));
}


static void dmx_write_global_irqenable(struct uio_handle * uio, const unsigned long v)
{
  write_dmx_reg(uio, DMX_IRQ_ENABLE_REGNO, v);
}

static unsigned long dmx_read_global_irqenable(struct uio_handle * uio)
{
  return read_dmx_reg(uio, DMX_IRQ_ENABLE_REGNO);
}

static unsigned long dmx_read_global_irqstatus(struct uio_handle * uio)
{
  return read_dmx_reg(uio, DMX_IRQ_STATUS_REGNO);
}



#endif
