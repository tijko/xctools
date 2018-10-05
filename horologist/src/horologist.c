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

Display *display;
Window window;
GC gc;

int pixelsize;
int gridsize;
int xpad, ypad;

/* Colours */
unsigned long red, dark_red, white, black;
void
xblit (Display * display, Window window, GC gc, int full_update,
       display_t * d, display_t * current)
{
  int x;
  int y;
  int pix;
  int reds = 0;
  int blacks = 0;
  int pos;
  XRectangle recs[DISPLAY_H * DISPLAY_W];
  for (y = 0; y < d->h; y++)

    {
      for (x = 0; x < d->w; x++)

        {
          pix = DISPLAY_PIXEL (d, x, y);
          if (full_update || (pix ^ DISPLAY_PIXEL (current, x, y)))

            {
              DISPLAY_PIXEL (current, x, y) = pix;
              if (!pix)

                {
                  pos = (DISPLAY_H * DISPLAY_W - 1) - blacks;
                  blacks++;
                }

              else

                {
                  pos = reds;
                  reds++;
                }
              recs[pos].x = xpad + x * pixelsize;
              recs[pos].y = ypad + y * pixelsize;
              recs[pos].width = pixelsize - gridsize;
              recs[pos].height = pixelsize - gridsize;
            }                   /* end if pix */
        }
    }
  if (full_update)
    {
      XSetForeground (display, gc, black);
      XFillRectangle (display, window, gc, 0, 0, DISPLAY_W * pixelsize,
                      DISPLAY_H * pixelsize);
    }
  XSetForeground (display, gc, red);
  XFillRectangles (display, window, gc, recs, reds);
  XSetForeground (display, gc, dark_red);
  XFillRectangles (display, window, gc,
                   &recs[(DISPLAY_H * DISPLAY_W) - blacks], blacks);
}

static void
find_colours (Display * display)
{
  XColor exact;
  XColor color;
  Colormap colormap;
  colormap = DefaultColormap (display, DefaultScreen (display));
  black = BlackPixel (display, DefaultScreen (display));
  white = WhitePixel (display, DefaultScreen (display));
  if (XLookupColor (display, colormap, "Red", &exact, &color)
      && XAllocColor (display, colormap, &color))

    {
      red = color.pixel;
    }

  else

    {
      red = white;
    }
  if (XLookupColor (display, colormap, "DarkRed", &exact, &color)
      && XAllocColor (display, colormap, &color))

    {
      dark_red = color.pixel;
    }

  else

    {
      dark_red = black;
    }
}

static void
fit_to_window (Display * display, int w, int h)
{
  int ts;
  ts = w / DISPLAY_W;
  pixelsize = h / DISPLAY_H;
  if (pixelsize > ts)
    pixelsize = ts;
  if (pixelsize < 3)

    {
      gridsize = pixelsize / 10;
      if (pixelsize == 0)
        pixelsize = 1;
    }

  else
    gridsize = (pixelsize / 10) + 1;
  xpad = (w - (DISPLAY_W * pixelsize)) / 2;
  ypad = (h - (DISPLAY_H * pixelsize)) / 2;
  XSetForeground (display, gc, black);
  XFillRectangle (display, window, gc, 0, 0, w, h);
}

static int
dispatch_x (Display * display)
{
  int damaged = 0;
  while (XPending (display))

    {
      XEvent xe;
      if (XNextEvent (display, &xe))
        continue;
      switch (xe.type)

        {
        case ConfigureNotify:

          {
            XConfigureEvent *configEvent = (XConfigureEvent *) & xe;
            fit_to_window (display, configEvent->width, configEvent->height);
            damaged++;
            break;
          }
        case Expose:

          {
            damaged++;
            break;
          }
        case ClientMessage:
          XDestroyWindow (display, window);
          XCloseDisplay (display);
          exit (0);
        }
    }
  return damaged;
}

static void
center_window (void)
{
  int x, y;
  unsigned int w, h, b, d;
  Window root;
  XGetGeometry (display, RootWindow (display, DefaultScreen (display)),
                &root, &x, &y, &w, &h, &b, &d);
  x = (w - pixelsize * DISPLAY_W) >> 1;
  y = 0;
  XMoveWindow (display, window, x, y);
} 

static void request_xccm_close (Display * display, Window window)
{
  // Prosses Window Close Event through event handler so XNextEvent does Not fail
  Atom delWindow = XInternAtom (display, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols (display, window, &delWindow, 1);

  /* select kind of events we are interested in */
  XSelectInput (display, window,
                StructureNotifyMask | ExposureMask | KeyPressMask);
} 

int main (int argc, char *argv[])
{
  int window_damaged = 1;
  fd_set rfds;
  int x_fd;
  display_t *current, *new;

  current = display_create ();
  new = display_create ();

  gridsize = 1;
  pixelsize = 5;

  display = XOpenDisplay (NULL);

  if (!display)
    {
      fprintf (stderr, "Cannot open display\n");
      exit (1);
    }

  find_colours (display);

  {
    XSetWindowAttributes wa = { 0 };
    unsigned long mask = 0;
    mask |= CWOverrideRedirect;
    wa.override_redirect = 1;

    /* create window */
    window =
      XCreateWindow (display, RootWindow (display, DefaultScreen (display)),
                     0, 0, pixelsize * DISPLAY_W, pixelsize * DISPLAY_H, 0,
                     CopyFromParent, InputOutput, CopyFromParent, mask, &wa);
  } 

  center_window ();

  XMapRaised (display, window);

  {
    XGCValues gcv = { 0 };
    unsigned long mask = 0;
    gc = XCreateGC (display, window, mask, &gcv);
  } 

  fit_to_window (display, pixelsize * DISPLAY_W, pixelsize * DISPLAY_H);

  request_xccm_close (display, window);

  XStoreName (display, window, "XC Clock");
  XSync (display, 0);


  x_fd = XConnectionNumber (display);

  FD_ZERO (&rfds);


  for (;;) {
      struct timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      roman_clock (new);

      do {
          center_window ();
          XRaiseWindow (display, window);
          xblit (display, window, gc, window_damaged, new, current);
          window_damaged = 0;
          XFlush (display);
          FD_SET (x_fd, &rfds);


          switch (select (x_fd + 1, &rfds, NULL, NULL, &tv)) {
            case -1:
              perror ("Error with select()");
              break;
            case 1:
              window_damaged += dispatch_x (display);
              break;
            }

        } while (tv.tv_usec || tv.tv_sec);
    }
  return 0;
}
