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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/joystick.h>

#include "pointer.h"
#include "pointer_internal.h"

static FILE *f=0;

static void cleanup()
{
  if(f)
    fclose(f);
}

static int jsgetfd()
{
  if(f==0)
    return -1;
  return fileno(f);
}

static int jsinit(char *device)
{
  /* open device */
  f=fopen(device, "rb");
  if(f==0)
    {
      printf("could not open %s: %s\n", device, strerror(errno));
      return -1;
    }

  atexit(cleanup);

  return 0;
}

static int jspoll()
{
  int X=pointerrawX(), Y=pointerrawY(), B=pointerB();
  struct js_event e;
  fread(&e, sizeof(struct js_event), 1, f);

  e.type &= ~JS_EVENT_INIT;
  switch(e.type)
    {
    case JS_EVENT_BUTTON:
      {
	int b;
	switch(e.number)
	  {
	  default:
	  case 0: b=BUT2; break;
	  case 1: b=BUT3; break;
	  case 2: b=BUT1; break;
	  case 3: b=BUT4; break;
	  case 4: b=BUT5; break;
	  }
	if(e.value)
	  B|=b;
	else
	  B&=~b;
      }
      break;

    case JS_EVENT_AXIS:
      if(e.number==0)
	X+=e.value;
      else if(e.number==1)
	Y+=e.value;
      break;

    default:
      break;
    }

  pointersetrawX(X);
  pointersetrawY(Y);
  pointersetB(B);

  return 0;
}

void jsregister(struct driver *drv)
{
  drv->init=jsinit;
  drv->poll=jspoll;
  drv->getfd=jsgetfd;
}
