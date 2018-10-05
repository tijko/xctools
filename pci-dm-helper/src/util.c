/*
 * Copyright (c) 2012 Citrix Systems, Inc.
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

#define _BSD_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include "util.h"

static char *prefix = NULL;

void prefix_set (const char *p)
{
    closelog ();
    if (prefix)
        free (prefix);
    prefix = strdup (p);
    if (!prefix)
        fatal ("Unable to set a new prefix log");
    openlog (prefix, LOG_CONS, LOG_USER);
}

void
message (int flags, const char *file, const char *function, int line,
         const char *fmt, ...)
{
  va_list ap;
  char *level = NULL;

  if (flags & MESSAGE_INFO)
    {
      level = "Info";
    }
  else if (flags & MESSAGE_WARNING)
    {
      level = "Warning";
    }
  else if (flags & MESSAGE_ERROR)
    {
      level = "Error";
    }
  else if (flags & MESSAGE_FATAL)
    {
      level = "Fatal";
    }

  fprintf (stderr, "%s:%s:%s:%s:%d:", (prefix) ? prefix :"",
           level, file, function, line);

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);

  fprintf (stderr, "\n");
  fflush (stderr);
  if (flags & (MESSAGE_INFO | MESSAGE_WARNING | MESSAGE_ERROR | MESSAGE_FATAL))
    {
      va_start (ap, fmt);
      vsyslog (LOG_ERR, fmt, ap);
      va_end (ap);
    }

  if (flags & MESSAGE_FATAL)
    {
      abort ();
    }
}

void *
xcalloc (size_t n, size_t s)
{
  void *ret = calloc (n, s);
  if (!ret)
    fatal ("calloc failed");
  return ret;
}

void *
xmalloc (size_t s)
{
  void *ret = malloc (s);
  if (!ret)
    fatal ("malloc failed");
  return ret;
}

void *
xrealloc (void *p, size_t s)
{
  p = realloc (p, s);
  if (!p)
    fatal ("realloc failed");
  return p;
}

char *
xstrdup (const char *s)
{
  char *ret = strdup (s);
  if (!ret)
    fatal ("strdup failed");
  return ret;
}

