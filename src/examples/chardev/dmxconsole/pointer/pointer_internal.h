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

#ifndef POINTER_INTERNAL__H
#define POINTER_INTERNAL__H

#include <stdio.h>

#define MOUSE_LIMIT 4096

struct driver {
  int (*init) (char *);
  int (*poll) ();
  int (*getfd) ();
};

void flushdevice(FILE *f);

int pointerrawX();
int pointerrawY();

void pointersetrawX(int);
void pointersetrawY(int);

#endif
