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

#ifndef POINTER__H
#define POINTER__H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUT1 1
#define BUT2 2
#define BUT3 4
#define BUT4 8
#define BUT5 16

  int pointerinit(int *argc, const char **argv);
  int pointergetfd();
  int pointerprocess();

  int pointerX();
  int pointerY();
  int pointerZ();

  void pointersetX(int);
  void pointersetY(int);
  void pointersetZ(int);

  void pointersetB(int);
  int pointerB();

#ifdef __cplusplus
};
#endif

#endif
