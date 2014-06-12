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
#define __PROJECT_H__


#include "config.h"

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_SYS_INT_TYPES_H)
#include <sys/int_types.h>
#endif

#include <sys/mman.h>

#include <syslog.h>

#include <dlfcn.h>

#include <time.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <dbus/dbus.h>
#include <xenstore.h>

#include <fb2if.h>
#include <fbtap.h> /* kernel module */
#include <xenbackend.h>

#ifdef HAVE_XENCTRL_H
#include <xenctrl.h>
#endif

#include <surfman.h>
#include <stdlib.h>
#include <libdmbus.h>
#include <edid.h>
#include <pciaccess.h>
#include <libpciemu.h>

#include "surface.h"
#include "domain.h"
#include "plugin.h"
#include "vgpu.h"
#include "xenstore-helper.h"
#include "display.h"
#include "splashscreen.h"

#ifdef PROTOS
#define NOPROTO static
#else
#define NOPROTO
#endif

#include "prototypes.h"
#include "version.h"

#endif /* __PROJECT_H__ */
