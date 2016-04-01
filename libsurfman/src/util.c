/*
 * Copyright (c) 2013 Citrix Systems, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "project.h"

void fprint_backtrace(FILE *fstream)
{
    void *bta[256];
    char **fnames;
    int i, n;

    n = backtrace (bta, sizeof (bta) / sizeof (bta[0]));
    fnames = backtrace_symbols (bta, n);
    if (fnames == NULL)
      {
        perror ("backtrace_symbols:");
        abort ();
      }

    for (i = 0; i < n; ++i)
      fprintf (fstream, "%s\n", fnames[i]);

    free (fnames);
}

void syslog_backtrace(int level)
{
    void *bta[256];
    char **fnames;
    int i, n;

    n = backtrace (bta, sizeof (bta) / sizeof (bta[0]));
    fnames = backtrace_symbols (bta, n);
    if (fnames == NULL)
      {
        perror ("backtrace_symbols:");
        abort ();
      }

    for (i = 0; i < n; ++i)
      syslog (level, "%s", fnames[i]);

    free (fnames);
}


void surfman_vmessage(surfman_loglvl level, const char *fmt, va_list ap)
{
  va_list ap2;
  int syslog_lvl = LOG_DEBUG;

  va_copy(ap2, ap);

  switch (level)
    {
      case SURFMAN_DEBUG:
        syslog_lvl = LOG_DEBUG;
        break;
      case SURFMAN_INFO:
        syslog_lvl = LOG_INFO;
        break;
      case SURFMAN_WARNING:
        syslog_lvl = LOG_WARNING;
        break;
      case SURFMAN_ERROR:
        syslog_lvl = LOG_ERR;
        break;
      case SURFMAN_FATAL:
        syslog_lvl = LOG_EMERG;
        break;
    }

  // TODO: Make that configurable (output logfile/syslog or not/stderr or not)
  vsyslog(syslog_lvl, fmt, ap);

  vfprintf(stderr, fmt, ap2);

  if (level == SURFMAN_FATAL)
    {
      syslog_backtrace(LOG_ERR);
      fprint_backtrace(stderr);
      abort();
    }
}

void surfman_message(surfman_loglvl level, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  surfman_vmessage(level, fmt, ap);
  va_end(ap);
}

void *xcalloc (size_t n, size_t s)
{
  void *ret = calloc (n, s);
  if (!ret)
    surfman_fatal ("calloc failed");
  return ret;
}

void *xmalloc (size_t s)
{
  void *ret = malloc (s);
  if (!ret)
    surfman_fatal ("malloc failed");
  return ret;
}

void *xrealloc (void *p, size_t s)
{
  p = realloc (p, s);
  if (!p)
    surfman_fatal ("realloc failed");
  return p;
}

char *xstrdup (const char *s)
{
  char *ret = strdup (s);
  if (!ret)
    surfman_fatal ("strdup failed");
  return ret;
}

