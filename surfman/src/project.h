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

#include <sys/time.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <errno.h>

#include <string.h>
#include <strings.h>

#include <unistd.h>
#include <fcntl.h>

#include <dlfcn.h>

#include <dirent.h>

#include <syslog.h>

#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <dbus/dbus.h>

#include <xenctrl.h>
#include <xenstore.h>
#include <xenbackend.h>

#include <fb2if.h>
#include <fbtap.h> /* kernel module */

#include <surfman.h>
#include <libdmbus.h>
#include <edid.h>
#include <pciaccess.h>
#include <event.h>

#include "surface.h"
#include "domain.h"
#include "plugin.h"
#include "xenstore-helper.h"
#include "display.h"
#include "splashscreen.h"

#include "prototypes.h"

#endif /* __PROJECT_H__ */

