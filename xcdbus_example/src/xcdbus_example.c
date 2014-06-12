/*
 * xcdbus_example.c:
 *
 * Copyright (c) 2011 James McKenzie <20@madingley.org>,
 * All rights reserved.
 *
 */

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


static char rcsid[] = "$Id:$";

/*
 * $Log:$
 */



#include "project.h"

#define PERIODIC_TIMER 5
#define UDP_PORT 1921

int
open_socket (void)
{
  int fd;
  struct sockaddr_in sin;

  fd = socket (PF_INET, SOCK_DGRAM, 0);

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  sin.sin_port = htons (PORT);

  bind (fd, (struct sockaddr *) &sin, sizeof (sin));

  return fd;
}

void
socket_msg (int fd)
{
  char buf[8192];
  int len = recv (fd, buf, sizeof (buf), 0);

  printf ("Received %d bytes from the socket\n", len);
}

void
periodic_thing (void)
{
  printf ("Tick\n");
}


int
main (int argc, char *argv[])
{
  DBusConnection *dbus_conn;
  DBusGConnection *dbus_g_conn;
  xcdbus_conn_t xcdbus_conn;
  fd_set rfds, wfds;
  int socket_fd;
  struct timeval tv = { 0 };

  g_type_init ();

  dbus_g_conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
  dbus_conn = dbus_g_connection_get_connection (dbus_g_conn);
  xcdbus_conn = xcdbus_init2 (SURFMAN_DBUS_SERVICE, dbus_g_conn);

  socket_fd = open_socket ();

  for (;;)
    {
      int n = 0;

      FD_ZERO (&rfds);
      FD_ZERO (&wfds);

      n = socket_fd + 1;
      FD_SET (socket_fd, &rfds);

      /*Slight subtlety, we pass in the currently used width of fd_set */
      /*and it returns the new width */
      n = xcdbus_pre_select (xcdbus_conn, n, &rfds, &wfds, NULL);

      if (select (n, &rfds, &wfds, NULL, &tv) == -1)
        {
          perror ("select");
          break;
        }

      xcdbus_post_select (xcdbus_conn, n, &rfds, &wfds, NULL);

      if (FD_ISSET (socket_fd, &rfds))
        {
          socket_msg (socket_fd);
        }


      if (!tv.tv_usec && !tv.tv_sec)
        {
          /*Timed out */
          periodic_thing ();
          tv.tv_sec = PERIODIC_TIMER;
        }

    }

  return 0;
}
