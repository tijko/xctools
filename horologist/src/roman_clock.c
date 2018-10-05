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

#define FONT_WIDTH 6

unsigned char font[128][6][6] = {
  ['X'] = {{1, 0, 0, 0, 1, 0},
           {0, 1, 0, 1, 0, 0},
           {0, 0, 1, 0, 0, 0},
           {0, 1, 0, 1, 0, 0},
           {1, 0, 0, 0, 1, 0},
           {0, 0, 0, 0, 0, 0}},
  ['L'] = {{0, 1, 0, 0, 0, 0},
           {0, 1, 0, 0, 0, 0},
           {0, 1, 0, 0, 0, 0},
           {0, 1, 0, 0, 0, 0},
           {0, 1, 1, 1, 1, 0}},
  ['I'] = {{0, 1, 1, 1, 0, 0},
           {0, 0, 1, 0, 0, 0},
           {0, 0, 1, 0, 0, 0},
           {0, 0, 1, 0, 0, 0},
           {0, 1, 1, 1, 0, 0},
           {0, 0, 0, 0, 0, 0}},
  ['V'] = {{1, 0, 0, 0, 1, 0},
           {1, 0, 0, 0, 1, 0},
           {1, 0, 0, 0, 1, 0},
           {0, 1, 0, 1, 0, 0},
           {0, 0, 1, 0, 0, 0},
           {0, 0, 0, 0, 0, 0}},
};

static void
write_text (display_t * d, char *string, int x, int y)
{
  int lx, ly;
  for (; *string; string++, x += FONT_WIDTH)
    {
      for (ly = 0; ly < 6; ++ly)
        {
          if ((ly + y) >= DISPLAY_H)
            continue;
          for (lx = 0; lx < 6; ++lx)
            {
              if ((lx + x) >= DISPLAY_W)
                continue;

              DISPLAY_PIXEL (d, x + lx, y + ly) = font[(int) *string][ly][lx];
            }
        }
    }
}

static void
getroman (int n, char *result)
{
  char s[] = "IIIVIXXXLXCCCDCMMM";
  int c[] = { 1, 4, 5, 9, 10, 40, 50, 90, 100, 400, 500, 900, 1000 };
  int i;

  result[0] = 0;

  for (i = 12; i > -1; n %= c[i], i--)
    {
      if (n / c[i] > 0)
        strncat (result, s + (i + (i + 3) / 4),
                 i % 4 ? (i - 2) % 4 ? 2 : 1 : n / c[i]);
    }
}


void
roman_clock (display_t * d)
{
  int h, m, s;
  struct tm *ptr;
  time_t lt;
  char string[20];

  lt = time (NULL);
  ptr = localtime (&lt);

  h = ptr->tm_hour;
  m = ptr->tm_min;
  s = ptr->tm_sec;


  if (!s)
    {
      m--;
      s = 60;
    }

  if (m < 1)
    {
      h--;
      m += 60;
    }

  if (h < 1)
    h += 24;

  clear (d);

  getroman (h, string);
  write_text (d, string, 45 - ((strlen (string) * FONT_WIDTH) / 2), 2);

  getroman (m, string);
  write_text (d, string, 40 - (strlen (string) * FONT_WIDTH), 9);

  getroman (s, string);
  write_text (d, string, 50, 9);

}
