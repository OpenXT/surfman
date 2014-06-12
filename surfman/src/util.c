/*
 * Copyright (c) 2012 Citrix Systems, Inc.
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
/*
 * util.c:
 *
 * Copyright (c) 2009 James McKenzie <20@madingley.org>,
 * All rights reserved.
 *
 */

static char rcsid[] = "$Id:$";

/*
 * $Log:$
 */


#include "project.h"

void helper_exec(const char *bin, ... )
{
    struct stat         s;
    pid_t pid;
    int i = 0;
    va_list vl;
    const char *argv[64];

    va_start(vl, bin);

    argv[i] = bin;
    for (i=1; i < 64 && (argv[i] = va_arg(vl, char *const)); i++)
        ;
    argv[i] = 0;
    if (stat(bin, &s) == -1)
        return;

    switch ((pid=fork())) { //wait is mopped up by sigchld in main
        case 0:
            execv(bin, (char **const)argv);
            surfman_error("execl failed for %s",bin);
            _exit(1);
            break;
        case -1:
            surfman_error("fork failed");
            break;
    }


    waitpid(pid,NULL,0);
}

void
renable_core_dumps (void)
{
  struct rlimit rlp;
  int rc;

  rlp.rlim_cur = RLIM_INFINITY;
  rlp.rlim_max = RLIM_INFINITY;

  rc = setrlimit (RLIMIT_CORE, &rlp);
  if (rc == -1)
    {
      surfman_warning ("Couldn't set core size limit: %s", strerror (errno));
    }
}

static void
misery (int sig, siginfo_t *info, void *context)
{
  surfman_error ("Signal %d caught: %s", sig, strsignal (sig));

  if (open ("/dev/tty", O_RDWR) >= 0)
    {
      char spid[8];
      char cmd[PATH_MAX];
      int len;

      surfman_info ("Process has a controlling tty, try to run gdb.\n");

      snprintf (spid, sizeof (spid), "%d", getpid());
      len = readlink ("/proc/self/exe", cmd, PATH_MAX - 1);
      cmd[(len < 0) ? 0 : len] = '\0';
      helper_exec ("/usr/bin/gdb", cmd, "--pid", spid, NULL);
    }

}

void
trap_segv (void)
{
  struct sigaction act;

  act.sa_flags = SA_SIGINFO | SA_RESETHAND;
  act.sa_sigaction = misery;
  sigemptyset(&act.sa_mask);

  sigaction (SIGSEGV, &act, NULL);
  sigaction (SIGBUS, &act, NULL);
  sigaction (SIGABRT, &act, NULL);
}


