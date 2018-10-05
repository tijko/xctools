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

#ifdef SQUARE
#define DISPLAY_W 48
#define DISPLAY_H 32
#define DISPLAY_W_BUF 64
#define DISPLAY_H_BUF 48
#else
#define DISPLAY_W 96
#define DISPLAY_H 16
#define DISPLAY_W_BUF 128
#define DISPLAY_H_BUF 24
#endif

typedef struct
{
  int *image;
  int w;
  int h;
} display_t;


#define DISPLAY_PIXEL(d,x,y) ((d)->image[((x)*DISPLAY_H_BUF)+(y)])
