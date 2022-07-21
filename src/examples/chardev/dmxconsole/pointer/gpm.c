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

#include <gpm.h>
#include <stdlib.h>
#include "pointer_internal.h"
#include "pointer.h"

static void cleanup()
{
  Gpm_Close();
}

static int gpmgetfd()
{
  return gpm_fd;
}

static Gpm_Connect conn;
static int gpminit(char *device)
{
  conn.eventMask = GPM_DOWN|GPM_UP|GPM_MOVE|GPM_MFLAG|GPM_SINGLE|GPM_DRAG;
  conn.defaultMask = ~conn.eventMask;
  conn.minMod = 0;
  conn.maxMod = ~0;

  if(Gpm_Open(&conn, 0) < 0)
    {
      printf("can not connec to gpm server\n");
      return -1;
    }

  atexit(cleanup);

  return 0;
}

static int gpmpoll()
{
  Gpm_Event ev;
  if(Gpm_GetEvent(&ev) == 1)
    {
      int B=0;
      if(ev.buttons&0x04)
	B|=BUT1;
      if(ev.buttons&0x02)
	B|=BUT2;
      if(ev.buttons&0x01)
	B|=BUT3;
      pointersetB(B);

      int X=pointerrawX()+ev.dx;
      if(X<-MOUSE_LIMIT)
	X=-MOUSE_LIMIT;
      else if(X>MOUSE_LIMIT)
	X=MOUSE_LIMIT;
      pointersetrawX(X);

      int Y=pointerrawY()+ev.dy;
      if(Y<-MOUSE_LIMIT)
	Y=-MOUSE_LIMIT;
      else if(Y>MOUSE_LIMIT)
	Y=MOUSE_LIMIT;
      pointersetrawY(Y);
    }
  return 0;
}

void gpmregister(struct driver *drv)
{
  drv->init=gpminit;
  drv->poll=gpmpoll;
  drv->getfd=gpmgetfd;
}
