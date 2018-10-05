/*
 * Copyright (c) 2011 Citrix Systems, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "project.h"


void
scroll (display_t * d)
{
  uint8_t ring[DISPLAY_H_BUF * sizeof (int)];

  bcopy (&d->image[(DISPLAY_W - 1) * DISPLAY_H_BUF], ring,
         DISPLAY_H * sizeof (int));

  memmove (&d->image[DISPLAY_H_BUF], d->image,
           (DISPLAY_W - 1) * DISPLAY_H_BUF * sizeof (int));
  bcopy (ring, d->image, DISPLAY_H * sizeof (int));
}

void
clear (display_t * d)
{
  int is = DISPLAY_H_BUF * DISPLAY_W_BUF * sizeof (int);
  bzero (d->image, is);
}


display_t *
display_create (void)
{
  int is = DISPLAY_H_BUF * DISPLAY_W_BUF * sizeof (int);
  display_t *ret = malloc (sizeof (display_t));
  ret->image = malloc (is);
  bzero (ret->image, is);


  ret->w = DISPLAY_W;
  ret->h = DISPLAY_H;

  return ret;
}
