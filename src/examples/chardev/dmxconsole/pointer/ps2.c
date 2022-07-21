/*
 * Copyright (C) Dirk Jagdmann <doj@cubic.org>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "pointer.h"
#include "pointer_internal.h"

#define SIGNX 16
#define SIGNY 32
#define OVFLX 64
#define OVFLY 128

static FILE *f=0;

static void cleanup()
{
  if(f)
    fclose(f);
}

static int ps2getfd()
{
  if(f==0)
    return -1;
  return fileno(f);
}

enum {
  PS2NORMAL,
  PS2INTELLI,
  PS2INTELLI2,
};

static int ps2mode=PS2NORMAL;

static int ps2sendcommand(unsigned char c)
{
  int r;
  PRINTF("ps2sendcommand: %02x\n", c);
  fwrite(&c, 1, 1, f);
  r=fread(&c, 1, 1, f);
  if(r!=1)
    {
      PRINTF("ERROR: did not read any byte\n");
      return -1;
    }
  if(c!=0xFA)
    {
      PRINTF("ERROR: did not receive Acknoledge byte: %02x\n", c);
      return -1;
    }
  return 0;
}

static int ps2setrate(unsigned char r)
{
  int ret;
  ret=ps2sendcommand(0xF3);
  if(ret<0)
    {
      PRINTF("ps2setrate: could not issue Set Sample Rate Command\n");
      return -1;
    }
  ret=ps2sendcommand(r);
  if(ret<0)
    {
      PRINTF("ps2setrate: could not transmit rate\n");
      return -1;
    }
  return 0;
}

static int ps2getid()
{
  unsigned char c;
  int r;
  if(ps2sendcommand(0xF2) < 0)
    {
      PRINTF("ps2getid: could not send command\n");
      return -1;
    }
  r=fread(&c, 1, 1, f);
  if(r!=1)
    {
      PRINTF("ps2getid: did not read any byte\n");
      return -1;
    }
  return c;
}

static int ps2init(char *device)
{
  unsigned char c;
  int r;

  /* open device */
  f=fopen(device, "r+");
  if(f==0)
    {
      printf("could not open %s: %s\n", device, strerror(errno));
      return -1;
    }

  atexit(cleanup);

  flushdevice(f);

  /* Reset */
  PRINTF("ps2 RESET\n");
  ps2sendcommand(0xFF);
  r=fread(&c, 1, 1, f);
  if(r!=1)
    {
      PRINTF("ERROR: could not read BAT\n");
      return -1;
    }
  if(c==0xAA)
    {
      PRINTF("reset succesful\n");
    }
  else if(c==0xFC)
    {
      PRINTF("ERROR: could not reset\n");
      return -1;
    }
  else
    {
      PRINTF("ERROR: unexpected reset return code: %02x\n", c);
      return -1;
    }

  r=fread(&c, 1, 1, f);
  if(r!=1)
    {
      PRINTF("ERROR: could not read device ID\n");
      return -1;
    }

  PRINTF("ps2 device ID: %02x\n", c);

  /* set mode */
  ps2mode=PS2NORMAL;

  /* try to set intelli mode */
  if(!ps2setrate(200) && !ps2setrate(100) && !ps2setrate(80))
    {
      int b=ps2getid();
      if(b==0x03)
	{
	  ps2mode=PS2INTELLI;
	  PRINTF("entered PS2INTELLI mode\n");
	}
    }

  /* try to set intelli mode + 5 buttons */
  if(!ps2setrate(200) && !ps2setrate(200) && !ps2setrate(80))
    {
      int b=ps2getid();
      if(b==0x04)
	{
	  ps2mode=PS2INTELLI2;
	  PRINTF("entered PS2INTELLI2 mode\n");
	}
    }

  /* set sample rate */
  ps2setrate(200);

  /* enable data polling */
  PRINTF("ps2 Enable Data Reporting\n");
  ps2sendcommand(0xF4);

  return 0;
}

static int ps2poll()
{
  int X=0, Y=0, Z=pointerZ(), B=pointerB();
  int x=0, y=0, xo=0, yo=0;
  unsigned char buf[16];
  int r;
  switch(ps2mode)
    {
    case PS2NORMAL: r=fread(buf, 1, 3, f); break;
    case PS2INTELLI:
    case PS2INTELLI2: r=fread(buf, 1, 4, f); break;
    }

  B=0;
  switch(ps2mode)
    {
    case PS2INTELLI2:
      if(buf[3]&16)
	B|=BUT4;
      if(buf[3]&32)
	B|=BUT5;

    case PS2INTELLI:
      /* force sign extend */
      if(buf[3]&8)
	buf[3]|=0xF0;
      else
	buf[3]&=0x07;
      Z+=((signed char*)buf)[3];

    case PS2NORMAL:
      x=((signed char*)buf)[1];
      y=((signed char*)buf)[2];
      if(buf[0]&OVFLX)
	xo=256;
      if(buf[0]&OVFLY)
	yo=256;
      if(buf[0]&SIGNX)
	xo=-xo;
      if(buf[0]&SIGNY)
	yo=-yo;

      X+=x+xo;
      Y-=y+yo;

      if(buf[0]&1)
	B|=BUT1;
      if(buf[0]&2)
	B|=BUT2;
      if(buf[0]&4)
	B|=BUT3;
      break;
    }

  if(X<-MOUSE_LIMIT)
    X=-MOUSE_LIMIT;
  else if(X>MOUSE_LIMIT)
    X=MOUSE_LIMIT;

  if(Y<-MOUSE_LIMIT)
    Y=-MOUSE_LIMIT;
  else if(Y>MOUSE_LIMIT)
    Y=MOUSE_LIMIT;

  pointersetrawX(pointerrawX()+X);
  pointersetrawY(pointerrawY()+Y);
  pointersetZ(Z);
  pointersetB(B);

  return 0;
}

void ps2register(struct driver *drv)
{
  drv->init=ps2init;
  drv->poll=ps2poll;
  drv->getfd=ps2getfd;
}
