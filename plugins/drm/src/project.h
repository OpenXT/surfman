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

#ifndef __PROJECT_H__
# define __PROJECT_H__

# include "config.h"

# ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
#  ifdef TIME_WITH_SYS_TIME
#   include <time.h>
#  endif
# else
#  ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#  endif
#  include <time.h>
# endif

# ifdef HAVE_STDIO_H
#  include <stdio.h>
# endif

# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif

# ifdef HAVE_STRING_H
#  include <string.h>
# endif

# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

# ifdef HAVE_STDINT_H
#  include <stdint.h>
# elif defined(HAVE_SYS_INT_TYPES_H)
#  include <sys/int_types.h>
# endif

# ifdef HAVE_ERRNO_H
#  include <errno.h>
# endif

# ifdef HAVE_ASSERT_H
#  include <assert.h>
# endif

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# endif

# ifdef HAVE_STDARG_H
#  include <stdarg.h>
# endif

# ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
# endif

# ifdef HAVE_SYSLOG_H
#  include <syslog.h>
# endif

# ifdef HAVE_EXECINFO_H
#  include <execinfo.h>
# endif

# ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif

# ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
# endif

# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif

# ifdef HAVE_SYS_USER_H
#  include <sys/user.h>
# endif

/* Auto export */
# if defined(INT_PROTOS)
#  define INTERNAL
#  define EXTERNAL
# elif defined(EXT_PROTOS)
#  define INTERNAL static
#  define EXTERNAL
# else
#  define INTERNAL
#  define EXTERNAL
# endif

# include <xf86drm.h>
# include <xf86drmMode.h>

/* XXX: We use headers provided by the local build of libdrm we have, not the headers
 *      Linux export with its user interface (that does not include our modifications
 *      for foreign). */
# include <libdrm/drm.h>
# include <libdrm/i915_drm.h>

# include <event.h>

# include <libudev.h>

# include <xenctrl.h>   /* XXX: Fix surfman ... */
# include <surfman.h>

# include "list.h"
# include "drm-plugin.h"
# include "utils.h"
# include "prototypes.h"

#ifndef XC_PAGE_SIZE
# include <sys/user.h>
# define XC_PAGE_SIZE PAGE_SIZE
#endif

#endif /* __PROJECT_H__*/

