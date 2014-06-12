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

#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#define __USE_XOPEN_EXTENDED
#include <ftw.h>

#define ASYNCFLAG "--nowait"

static time_t last_mod_time;


static int mod_filter (const char *path, const struct stat *statbuf, int typeflag, 
                       struct FTW *ftwbuf)
{
    if (typeflag & FTW_DNR) {
        fprintf (stderr, "nftw: could not read directory %s, skipping it: %s\n",
                 path, strerror (errno));

        return 0;
    }

    if (statbuf->st_mtime > last_mod_time) {
        last_mod_time = statbuf->st_mtime;
    }

    return 0;
}



static int
age_of_latest_modification (const char *dirname)
{
    time_t now;

    if (nftw (dirname, &mod_filter, 10, FTW_MOUNT | FTW_PHYS)) {
        perror ("finding time of latest changes failed");
    }

    if ((time_t) -1 == (now = time (NULL))) {
        perror ("time() doesn't work... good night");
        return INT_MAX;
    }

    return now - last_mod_time;
}



int main(int argc, char **argv)
{
    DBusError err;
    DBusConnection *c;
    int rc = 0, async = 0;
    struct stat dirstat;
    char *dir;

    if (argc < 2) {

        fprintf(stderr, 
            "Usage: %s <directory name> [%s]\n\n"
            "\tstores screenshots of all VMs in <directory name>,\n"
            "\t%s: do not wait for completion, return as soon as the request has been sent\n",
            argv[0], ASYNCFLAG, ASYNCFLAG);

        return 1;
    }

    // TODO: getopt if any more options!
    if ((argc == 3) && !strncmp (argv[2], ASYNCFLAG, strlen (ASYNCFLAG))) {
        async = 1;
    }

    dir = argv[1];

    if (stat (dir, &dirstat)) {
        fprintf (stderr, "failed to stat %s: %s", dir, strerror (errno));
        return 2;
    }

    if (! S_ISDIR(dirstat.st_mode)) {
        fprintf (stderr, "%s is not a directory\n", dir);
        return 3;
    }

    fprintf(stderr, 
         "Saving screenshots for all screens, all domains in directory %s\n",
         dir);

    c = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    if (!c) {
        fprintf(stderr, "Can't get connection to dbus service: %s\n",
                err.message);
        return 1;
    }

    DBusMessage *m = dbus_message_new_method_call("com.citrix.xenclient.surfman",
                                                  "/",
                                                  "com.citrix.xenclient.surfman",
                                                  "dump_all_screens");

    if (!m) {
        fprintf(stderr, "Can't allocate dbus message\n");
        rc = 1;
        goto unref_conn;
    }

    dbus_message_append_args(m, DBUS_TYPE_STRING, &dir, DBUS_TYPE_INVALID);

    dbus_connection_send(c, m, NULL);
    dbus_message_unref(m);

    if (!async) {
        int timeout = 10; // seconds we allow surfman to write the screenshots
        int done = 0;
        const int inactivity = 3; // mtime seconds after which a file is 
                                  // considered not being written to anymore

        /* now wait until time is up or no more changes are being made in dir */
        do {
            sleep (1);
            timeout--;

            if (age_of_latest_modification (dir) > inactivity) {
                done = 1;
            }
        } while (!done && (timeout > 0));
    }

unref_conn:
    dbus_connection_unref(c);
    return rc;
}
