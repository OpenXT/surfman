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

# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <stddef.h>
# include <stdarg.h>
# include <ctype.h>

# include <inttypes.h>
# include <limits.h>

# include <errno.h>
# include <assert.h>

# include <unistd.h>
# include <fcntl.h>

# include <dlfcn.h>

# include <memory.h>

# include <strings.h>
# include <string.h>

# include <syslog.h>

# include <sys/mman.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/ioctl.h>

# define XC_WANT_COMPAT_MAP_FOREIGN_API
# include <xenctrl.h>
# include <xen/sys/privcmd.h>
# include <xen/hvm/ioreq.h>

# include <execinfo.h>

# include <pthread.h>

# include "list.h"
# include "surfman.h"

# include "prototypes.h"

#endif /* __PROJECT_H__ */
