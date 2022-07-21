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

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "getarg.h"

#include "debug.h"
#include "pointer.h"
#include "pointer_internal.h"
void microsoftregister(struct driver *drv);
void mousesystemsregister(struct driver *drv);
void mmregister(struct driver *drv);
void jsregister(struct driver *drv);
void ps2register(struct driver *drv);
#ifdef HAVE_GPM
void gpmregister(struct driver *drv);
#endif

static int X=0, Y=0, Z=0, B=0;
static double SCALE=0.1;

void flushdevice(FILE *f)
{
  unsigned char buf[1024];
  fd_set rfds;
  struct timeval tv;
  int retval;
  int fh=fileno(f);

  PRINTF("flush\n");

  FD_ZERO(&rfds);
  FD_SET(fh, &rfds);
  tv.tv_sec=1;
  tv.tv_usec=0;

  retval=select(fh+1, &rfds, NULL, NULL, &tv);
  if(retval)
    {
      if(FD_ISSET(fh, &rfds))
	  read(fh, buf, sizeof(buf));
    }
}


static struct driver drv = { NULL, NULL, NULL };

int pointerinit(int *argc, const char **argv)
{
  const char *f;
  char *device=0;

  if(DMXgetarg(argc, argv, "--js", 1, &f)==1)
    {
      jsregister(&drv);
      device=strdup(f);
      SCALE=1.0/32768.0;
    }
  if(DMXgetarg(argc, argv, "--pc", 1, &f)==1)
    {
      mousesystemsregister(&drv);
      device=strdup(f);
    }
  if(DMXgetarg(argc, argv, "--ms", 1, &f)==1)
    {
      microsoftregister(&drv);
      device=strdup(f);
    }
  if(DMXgetarg(argc, argv, "--mm", 1, &f)==1)
    {
      mmregister(&drv);
      device=strdup(f);
    }
  if(DMXgetarg(argc, argv, "--gpm", 0, NULL)==0)
    {
#ifdef HAVE_GPM
      gpmregister(&drv);
      device="fake";
#else
      fprintf(stderr, "you have used the --gpm command line option, but this program is compiled without gpm support.\n");
      exit(1);
#endif
    }
  if(DMXgetarg(argc, argv, "--ps2", 1, &f)==1)
    {
      ps2register(&drv);
      device=strdup(f);
    }
  if(DMXgetarg(argc, argv, "--mousescale", 1, &f)==1)
    {
      double d=atof(f);
      if(d>0.0)
	SCALE*=1.0/d;
    }

  if(device==0)
    {
      printf("pointerinit(): no mouse device selected\n");
      return 0;
    }

  if(drv.init(device) < 0)
    {
      printf("pointerinit(): could not init device\n");
      return -1;
    }

  free(device);

  return 1;
}

int pointergetfd()
{
  return drv.getfd();
}

int pointerprocess()
{
#if 0
  static int z=0;
  PRINTF("\r");
#endif

  if(drv.poll() < 0)
    {
      printf("\npointerinit(): could not read device\n");
      return -1;
    }

#if 0
  PRINTF("%i %i %i %c %c %c %c %c %i    ", X, Y, Z, (B&BUT1)?'1':'.', (B&BUT2)?'2':'.', (B&BUT3)?'3':'.', (B&BUT4)?'4':'.', (B&BUT5)?'5':'.', ++z);
  fflush(stdout);
#endif
  return 0;
}

int pointerX()
{
  return SCALE*X;
}

int pointerY()
{
  return SCALE*Y;
}

int pointerZ()
{
  return Z;
}

int pointerrawX()
{
  return X;
}

int pointerrawY()
{
  return Y;
}

void pointersetX(int x)
{
  X=(double)x/SCALE;
}

void pointersetY(int y)
{
  Y=(double)y/SCALE;
}

void pointersetrawX(int x)
{
  X=x;
}

void pointersetrawY(int y)
{
  Y=y;
}

void pointersetZ(int z)
{
  Z=z;
}

void pointersetB(int b)
{
  B=b;
}

int pointerB()
{
  return B;
}
