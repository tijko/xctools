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

/* horologist.c */
Display *display;
Window window;
GC gc;
int pixelsize;
int gridsize;
int xpad;
int ypad;
unsigned long red;
unsigned long dark_red;
unsigned long white;
unsigned long black;
void xblit(Display *display, Window window, GC gc, int full_update, display_t *d, display_t *current);
int main(int argc, char *argv[]);
/* version.c */
/* display.c */
void scroll(display_t *d);
void clear(display_t *d);
display_t *display_create(void);
/* roman_clock.c */
unsigned char font[128][6][6];
void roman_clock(display_t *d);
