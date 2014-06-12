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
/* This lib implements a set Xenstore api. This ensures a unified
   of calling xenstore apis. This is dependant on libevent */

#include <assert.h>
#include "project.h"

static struct xs_handle *xs = NULL;
static struct event xs_ev;
static xs_transaction_t xs_transaction = XBT_NULL;

#define N_WATCHES       100

static struct {
    xenstore_watch_cb_t cb;
    void *opaque;
    char *path;
    bool is_dir; /* enable to watch a directory */
} watches[N_WATCHES];

bool xenstore_transaction_start (void)
{
    /* Disallow nested transaction */
    if (xs_transaction != XBT_NULL)
        return false;

    xs_transaction = xs_transaction_start (xs);

    return (xs_transaction != XBT_NULL);
}

bool xenstore_transaction_end (bool abort)
{
    bool res;

    res = xs_transaction_end (xs, xs_transaction, abort);
    xs_transaction = XBT_NULL;

    return res;
}

bool vxenstore_rm (const char *format, va_list arg)
{
    bool ret;
    char *buff = NULL;

    assert (xs != NULL);

    if (vasprintf (&buff, format, arg) == -1)
        return false;

    ret = xs_rm (xs, xs_transaction, buff);

    free (buff);

    return ret;
}

bool xenstore_rm (const char *format, ...)
{
    va_list arg;
    bool ret;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_rm (format, arg);
    va_end (arg);

    return ret;
}

bool xenstore_mkdir (const char *format, ...)
{
    char *buff = NULL;
    int res = 0;
    bool ret;
    va_list arg;

    va_start (arg, format);
    res = vasprintf (&buff, format, arg);
    va_end (arg);

    if (res == -1)
        return false;

    ret = xs_mkdir (xs, xs_transaction, buff);
    free (buff);

    return ret;
}

/* This function must only be used with c-string */
bool vxenstore_write (const char *data, const char *format, va_list arg)
{
    char *buff = NULL;
    bool ret;
    unsigned int len;

    assert (xs != NULL);

    len = strlen (data);

    if (vasprintf (&buff, format, arg) == -1)
        return false;

    ret = xs_write (xs, xs_transaction, buff, data, len);
    free (buff);

    return ret;
}

bool xenstore_write (const char *data, const char *format, ...)
{
    va_list arg;
    bool ret;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_write (data, format, arg);
    va_end (arg);

    return ret;
}

char **vxenstore_ls (unsigned int *num, const char *format, va_list arg)
{
    char *buff = NULL;
    char **ret;

    assert (xs != NULL);

    vasprintf (&buff, format, arg);
    ret = xs_directory (xs, xs_transaction, buff, num);
    free (buff);

    return ret;
}

char **xenstore_ls (unsigned int *num, const char *format, ...)
{
    va_list arg;
    char **ret;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_ls (num, format, arg);
    va_end (arg);

    return ret;
}

bool vxenstore_write_int (int data, const char *format, va_list arg)
{
    char *buff = NULL;
    bool ret;

    assert (xs != NULL);

    if (asprintf (&buff, "%d", data) == -1)
        return false;

    ret = vxenstore_write (buff, format, arg);
    free (buff);

    return ret;
}

bool xenstore_write_int (int data, const char *format, ...)
{
    va_list arg;
    bool ret;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_write_int (data, format, arg);
    va_end (arg);

    return ret;
}

char *vxenstore_read (const char *format, va_list arg)
{
    char *buff = NULL;
    char *res;

    if (vasprintf (&buff, format, arg) == -1)
        return NULL;

    res = xs_read(xs, xs_transaction, buff, NULL);
    free (buff);

    return res;
}

char *xenstore_read (const char *format, ...)
{
    char *res = NULL;
    va_list arg;

    assert (xs != NULL);

    va_start(arg, format);
    res = vxenstore_read (format, arg);
    va_end(arg);

    return res;
}

bool vxenstore_chmod (const char *perms, unsigned int nbperm,
                      const char *format, va_list arg)
{
    char  *buff = NULL;
    bool ret = false;
    struct xs_permissions *p;

    assert (xs != NULL);

    p = malloc (sizeof (*p) * nbperm);
    if (!p)
        return false;

    if (!xs_strings_to_perms (p, nbperm, perms))
        goto xs_chmod_err;

    if (!vasprintf (&buff, format, arg) == -1)
        goto xs_chmod_err;

    ret = xs_set_permissions (xs, xs_transaction, buff, p, nbperm);

    free (buff);
xs_chmod_err:
    free (p);

    return ret;
}

bool xenstore_chmod (const char *perms, unsigned int nbperm, const char *format, ...)
{
    bool ret;
    va_list arg;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_chmod (perms, nbperm, format, arg);
    va_end (arg);

    return ret;
}

bool vxenstore_dom_write (unsigned int domid, const char *data,
                          const char *format, va_list arg)
{
    char *domain_path = NULL;
    char *buff = NULL;
    bool ret;
    int res = -1;

    assert (xs != NULL);

    domain_path = xs_get_domain_path (xs, domid);
    if (!domain_path)
        return false;

    res = asprintf (&buff, "%s/%s", domain_path, format);
    free (domain_path);

    if (res == -1)
        return false;

    ret = vxenstore_write (data, buff, arg);
    free (buff);

    return ret;
}

bool xenstore_dom_write(unsigned int domid, const char *data,
                        const char *format, ...)
{
    va_list arg;
    bool ret;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_dom_write (domid, data, format, arg);
    va_end (arg);

    return ret;
}

bool vxenstore_dom_write_int (unsigned int domid, int data, const char *format,
                              va_list arg)
{
    char *buff = NULL;
    bool ret;

    assert (xs != NULL);

    if (asprintf (&buff, "%d", data) == -1)
        return false;

    ret = vxenstore_dom_write (domid, buff, format, arg);
    free (buff);

    return ret;
}

bool xenstore_dom_write_int (unsigned int domid, int data,
                             const char *format, ...)
{
    va_list arg;
    bool ret;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_dom_write_int (domid, data, format, arg);
    va_end (arg);

    return ret;
}

char *xenstore_dom_read (unsigned int domid, const char *format, ...)
{
    char *domain_path;
    va_list arg;
    char *ret = NULL;
    char *buff = NULL;
    int res;

    assert (xs != NULL);

    domain_path = xs_get_domain_path (xs, domid);

    if (!domain_path)
        return NULL;

    res = asprintf (&buff, "%s/%s", domain_path, format);
    free (domain_path);

    if (res == -1)
        return NULL;

    va_start (arg, format);
    ret = vxenstore_read (buff, arg);
    va_end (arg);

    free (buff);

    return ret;
}

bool xenstore_dom_chmod (unsigned int domid, const char *perms, int nbperm,
                         const char *format, ...)
{
    char *buff = NULL;
    char *domain_path = NULL;
    va_list arg;
    bool ret = false;

    assert (xs != NULL);

    domain_path = xs_get_domain_path(xs, domid);

    if (!domain_path)
        return false;

    if (asprintf (&buff, "%s/%s", domain_path, format) == -1)
        goto xs_dom_chmod_path;

    va_start (arg, format);
    ret = vxenstore_chmod (perms, nbperm, buff, arg);
    va_end (arg);

    free (buff);

xs_dom_chmod_path:
    free (domain_path);

    return ret;
}

static bool vxenstore_watch_v2 (xenstore_watch_cb_t cb, void *opaque,
                                bool is_dir, const char *format,
                                va_list arg)
{
    char *buff = NULL;
    bool ret = false;
    int i;

    assert (xs != NULL);

    if (vasprintf (&buff, format, arg) == -1) {
        return false;
    }

    /* Check if we try to unwatch, path exists, cb == NULL */
    if (!cb)
    {
        for (i = 0; i < N_WATCHES; i++)
        {
            if (!watches[i].path)
                continue;

            if (strcmp(watches[i].path, buff))
                continue;

            watches[i].cb = NULL;
            free(watches[i].path);
            watches[i].path = NULL;
            ret = xs_unwatch (xs, buff, buff);
        }
    }
    else
    {
        ret = xs_watch (xs, buff, buff);

        if (ret)
        {
            for (i = 0; i < N_WATCHES; i++)
            {
                if (watches[i].cb)
                    continue;

                watches[i].cb = cb;
                watches[i].opaque = opaque;
                watches[i].path = buff;
                watches[i].is_dir = is_dir;

                return true;
            }
            if (i == N_WATCHES)
            {
                ret = false;
                xs_unwatch (xs, buff, buff);
            }
        }
    }

    free (buff);

    return ret;
}

bool xenstore_watch_dir (xenstore_watch_cb_t cb, void *opaque,
                         const char *format, ...)
{
    bool ret;
    va_list arg;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_watch_v2 (cb, opaque, true, format, arg);
    va_end (arg);

    return ret;
}

bool xenstore_watch (xenstore_watch_cb_t cb, void *opaque,
                     const char *format, ...)
{
    bool ret;
    va_list arg;

    assert (xs != NULL);

    va_start (arg, format);
    ret = vxenstore_watch_v2 (cb, opaque, false, format, arg);
    va_end (arg);

    return ret;
}

bool xenstore_dom_watch (unsigned int domid, xenstore_watch_cb_t cb,
                         void *opaque, const char *format, ...)
{
    char *buff = NULL;
    char *domain_path = NULL;
    va_list arg;
    bool ret = false;

    assert (xs != NULL);

    domain_path = xs_get_domain_path (xs, domid);

    if (!domain_path)
        return false;

    if (asprintf (&buff, "%s/%s", domain_path, format) == -1)
        goto xs_dom_watch_dompath;

    va_start (arg, format);
    ret = vxenstore_watch_v2 (cb, opaque, false, buff, arg);
    va_end (arg);

    free (buff);

xs_dom_watch_dompath:
    free (domain_path);

    return ret;
}

static void xenstore_watches_scan (int fd, short event, void *opaque)
{
    char **vec = NULL;
    unsigned int nb;
    int i;
    const char *str;

    (void) fd;
    (void) event;
    (void) opaque;

    assert (xs != NULL);

    if (!(vec = xs_read_watch (xs, &nb)))
        return;

    for (i = 0; i < N_WATCHES; i++)
    {
        if (watches[i].path && watches[i].cb)
        {
            if (watches[i].is_dir)
                str = vec[XS_WATCH_TOKEN]; /* In case of directory watches */
            else
                str = vec[XS_WATCH_PATH];

            if (!strcmp (str, watches[i].path))
                watches[i].cb (vec[XS_WATCH_PATH], watches[i].opaque);
        }
    }

    free (vec);
}

char *xenstore_get_domain_path (unsigned int domid)
{
    assert (xs != NULL);

    return xs_get_domain_path (xs, domid);
}

int xenstore_init (void)
{
    xs = xs_domain_open ();

    if (!xs)
        return -1;

    event_set (&xs_ev,
               xs_fileno(xs),
               EV_READ | EV_PERSIST,
               xenstore_watches_scan,
               NULL);
    event_add (&xs_ev, NULL);

    return 0;
}

void xenstore_release (void)
{
    int i;

    if (xs)
        xs_close (xs);

    for (i = 0; i < N_WATCHES; i++)
    {
        if (watches[i].path)
            free (watches[i].path);
        watches[i].path = NULL;
    }
}
