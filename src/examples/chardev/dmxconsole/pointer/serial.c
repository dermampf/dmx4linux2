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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "debug.h"
#include "pointer.h"
#include "pointer_internal.h"

static int fh=-1;
static char lckfile[PATH_MAX];

static void cleanup()
{
  if(fh>=0)
    close(fh);
  unlink(lckfile);
}

static int serialgetfd()
{
  return fh;
}

enum {
  MICROSOFT,
  MOUSESYSTEMS,
  MM,
};

static int mode;

static int serialinit(char *device)
{
  struct termios t;

  struct speed_ {
    int baud;
    int num;
    char *str;
  } speed[] = {
    { B9600, 9600, "*q" },
    { B4800, 4800, "*p" },
    { B2400, 2400, "*o" },
    { B1200, 1200, "*n" },
  };
  int i;
  const int maxi=sizeof(speed)/sizeof(speed[0]);

  /* check lockfile */
  if(!device)
    return -1;
  i=strlen(device);
  if(i<1)
    return -1;
  /* search for devicename */
  char *dev=device+i-1;
  while(dev != device && *dev!='/') --dev;
  snprintf(lckfile, sizeof(lckfile), "/var/lock/LCK..%s", dev);
  PRINTF("checking lockfile %s\n", lckfile);
  /* check lckfile */
  if(access(lckfile, F_OK) == 0)
    {
      printf("%s is locked by %s\n", device, lckfile);
      return -1;
    }
  /* create lckfile */
  fh=open(lckfile, O_RDWR);
  if(fh<0)
    {
      printf("could not create %s: %s\n", lckfile, strerror(errno));
      return -1;
    }
  write(fh, "*", 1);
  close(fh);

  /* open device */
  fh=open(device, O_RDWR | O_NOCTTY);
  if(fh<0)
    {
      printf("could not open %s: %s\n", device, strerror(errno));
      return -1;
    }

  atexit(cleanup);

  for(i=0; i<maxi; ++i)
    {
#ifdef DOJDEBUG
      printf("set rs232 baud rate %i\n", speed[i].num);
#endif

  tcflush(fh, TCIOFLUSH);

  if(tcgetattr(fh, &t) < 0)
    {
      printf("could not get device attributes\n");
      exit(1);
    }

  /* power cycle mouse with control lines. See mouse(4) */

  /* baud rate */
  cfsetispeed(&t, speed[i].baud);
  cfsetospeed(&t, speed[i].baud);

  /* clear bits for char size */
  t.c_cflag &= ~CSIZE;

  /* no software flow control */
  t.c_cflag &= ~(IXON|IXOFF);

  /* set hardware handshake */
  t.c_cflag |= CRTSCTS;

  /* enable receiver */
  t.c_cflag |= CREAD;

  /* ignore modem control lines */
  t.c_cflag |= CLOCAL;

  /* no local flags */
  t.c_lflag = 0;

  /* disable input processing */
  t.c_iflag &= ~(INPCK|PARMRK|BRKINT|INLCR|ICRNL|IUCLC|IXANY);
  /* ignore break */
  t.c_iflag |= IGNBRK;

  /* set read timeout = .1s */
  t.c_cc[VTIME]=1;
  /* set min read bytes */
  t.c_cc[VMIN]=1;

  switch(mode)
    {
    case MICROSOFT:
      /* data bits */
      t.c_cflag |= CS7;
      /* 1 stop bit */
      t.c_cflag &= ~CSTOPB;
      /* no parity */
      t.c_cflag &= ~PARENB;	/* clear parity enable */
      t.c_iflag |= IGNPAR;	/* ignore parity errors */
      t.c_iflag &= ~INPCK;	/* disable parity check */
      break;

    case MOUSESYSTEMS:
      /* data bits */
      t.c_cflag |= CS8;
      /* 2 stop bits */
      t.c_cflag |= CSTOPB;
      /* no parity */
      t.c_cflag &= ~PARENB;	/* clear parity enable */
      t.c_iflag |= IGNPAR;	/* ignore parity errors */
      t.c_iflag &= ~INPCK;	/* disable parity check */
      break;

    case MM:
      /* data bits */
      t.c_cflag |= CS8;
      /* 1 stop bit */
      t.c_cflag |= ~CSTOPB;
      /* odd parity */
      t.c_cflag |= PARENB|PARODD;
      t.c_iflag |= INPCK;	/* enable parity check */
      break;

    default:
      printf("unknown mode in serialinit(). This should not happen\n");
      exit(1);
    }

  if(tcsetattr(fh, 0, &t) < 0)
    {
      printf("could not set device attributes\n");
      exit(1);
    }

  write(fh, speed[i].str, 2);
  {
    fd_set rfds;
    struct timeval tv;
    int retval;
    FD_ZERO(&rfds);
    FD_SET(fh, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    retval = select(fh+1, &rfds, NULL, NULL, &tv);
    if (retval < 0)
      perror("select()");
    else if (retval==0)
      ;				/* timeout */
    else if (FD_ISSET(fh, &rfds))
      {
	char buf[16];
	int r=read(fh, buf, sizeof(buf));
	if(r>1)
	  {
#ifdef DOJDEBUG
	    int j;
	    printf("read: ");
	    for(j=0; j<r; ++j)
	      putchar(isprint(buf[j])?buf[j]:'.');
	    putchar('\n');
#endif
	    if(*buf=='M')
	      break;
	  }
      }
  }
    }

  if(i>=maxi) i=maxi-1;
  PRINTF("select baud rate %i\n", speed[i].num);

  return 0;
}

static void printbits(const unsigned char b)
{
  PRINTF("%i%i%i%i %i%i%i%i %c\n",
	 (b&128)?1:0,
	 (b&64) ?1:0,
	 (b&32) ?1:0,
	 (b&16) ?1:0,
	 (b&8)  ?1:0,
	 (b&4)  ?1:0,
	 (b&2)  ?1:0,
	 (b&1)  ?1:0,
	 isprint(b)?b:' ');
}

/*
 *  Bit     D7      D6      D5      D4      D3      D2      D1      D0
 *
 *  1.      --      1       LB      RB      Y7      Y6      X7      X6
 *  2.      --      0       X5      X4      X3      X2      X1      X0
 *  3.      --      0       Y5      Y4      Y3      Y2      Y1      Y0
 *( 4.      --      0       MB      --      --      --      --      -- )
 */

static void msstatemachine(const unsigned char c)
{
  static unsigned char start, x, y;
  enum { WAITFORSTART, COLLECTX, COLLECTY, CHECKE };
  static int state=WAITFORSTART;

  printbits(c);

  if(c&64)
    {
      start=c;
      state=COLLECTX;
      return;
    }

  switch(state)
    {
    case COLLECTX:
      x=c;
      state=COLLECTY;
      break;

    case COLLECTY:
      y=c;
      state=CHECKE;
      {
	int X=0, Y=0, B=pointerB();

	/* calc mouse movement */
	x&=0x3F;
	y&=0x3F;
	x|=(start&3) <<6;
	y|=(start&12)<<4;

	/* x movement */
	X+=*((signed char*)&x);
	if(X<-MOUSE_LIMIT)
	  X=-MOUSE_LIMIT;
	else if(X>MOUSE_LIMIT)
	  X=MOUSE_LIMIT;
	pointersetrawX(pointerrawX()+X);

	/* y movement */
	Y+=*((signed char*)&y);
	if(Y<-MOUSE_LIMIT)
	  Y=-MOUSE_LIMIT;
	else if(Y>MOUSE_LIMIT)
	  Y=MOUSE_LIMIT;
	pointersetrawY(pointerrawY()+Y);

	/* buttons */
	B&=~(BUT1|BUT2);
	if(start&32)
	  B|=BUT1;
	if(start&16)
	  B|=BUT2;
	pointersetB(B);
      }
      break;

    case CHECKE:
      state=WAITFORSTART;
      {
	int B=pointerB() & (~BUT3);
	if(c&32)
	  B|=BUT3;
	pointersetB(B);
      }
      break;

    case WAITFORSTART:
      break;
    }
}

static int microsoftpoll()
{
  int i;
  unsigned char buf[16];
  int r=read(fh, buf, sizeof(buf));
  if(r<0)
    {
      PRINTF("microsoftpoll: could not read\n");
      return r;
    }
  for(i=0; i<r; ++i)
    msstatemachine(buf[i]);
  return 0;
}

void microsoftregister(struct driver *drv)
{
  mode=MICROSOFT;
  drv->init=serialinit;
  drv->poll=microsoftpoll;
  drv->getfd=serialgetfd;
}

static void mousesystemsmachine(const signed char b)
{
  /*
    Bit     D7      D6      D5      D4      D3      D2      D1      D0

    1.      1       0       0       0       0       LB      CB      RB
    2.      X7      X6      X5      X4      X3      X2      X1      X0
    3.      Y7      Y6      Y5      Y4      Y3      Y4      Y1      Y0
    4.      X7'     X6'     X5'     X4'     X3'     X2'     X1'     X0'
    5.      Y7'     Y6'     Y5'     Y4'     Y3'     Y4'     Y1'     Y0'
  */

  enum { WAITFORSTART, READX1, READY1, READX2, READY2 };
  static int state=WAITFORSTART, X=0, Y=0;
  const unsigned char u=*((unsigned char*)&b);

  printbits(u);

  if((u&0xF8) == 128)
    {
      /* buttons */
      int B=0;
      if(!(b&4))
	B|=BUT1;
      if(!(b&1))
	B|=BUT2;
      if(!(b&2))
	B|=BUT3;
      pointersetB(B);

      /* movement */
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
      X=Y=0;

      /* next state */
      state=READX1;
      return;
    }

  switch(state)
    {
    case READX1: X+=b; state=READY1; return;
    case READY1: Y-=b; state=READX2; return;
    case READX2: X+=b; state=READY2; return;
    case READY2: Y-=b; state=WAITFORSTART; return;
    }
}

static int mousesystemspoll()
{
  int i;
  signed char buf[16];
  int r=read(fh, buf, sizeof(buf));
  if(r<0)
    {
      PRINTF("mousesystemspoll: could not read\n");
      return r;
    }
  for(i=0; i<r; ++i)
    mousesystemsmachine(buf[i]);
  return 0;
}

void mousesystemsregister(struct driver *drv)
{
  mode=MOUSESYSTEMS;
  drv->init=serialinit;
  drv->poll=mousesystemspoll;
  drv->getfd=serialgetfd;
}

void mmregister(struct driver *drv)
{
  mode=MM;
  drv->init=serialinit;
  drv->poll=mousesystemspoll;
  drv->getfd=serialgetfd;
}
