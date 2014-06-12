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
#include <linux/ioctl.h>
#include "project.h"
#include "list.h"
#include "fbtap.h"

struct fbtap_device
{
	struct device surfman_dev;
	struct surface *s;
	unsigned long fb_numpages;       // number of pages allocated
	unsigned long *fb2m;             // list of page numbers
	int active;
	char * filename;
	int fd;
};

static void
fbtap_device_clear (struct fbtap_device *dev)
{
	dev->active = 0;
	dev->s = NULL;
	dev->fb_numpages = 0;
	dev->fb2m = NULL;
	dev->filename = NULL;
	dev->fd = -1;
}

static int
fbtap_device_activate (struct fbtap_device *dev, char *name, struct fbdim *dimensions)
{
	unsigned int n;
	int fd;
	unsigned long fb_numpages;
	unsigned long *fb2m;

	if (!name) {
		name = "/dev/fbtap";
	}

	if (dev->active) {
		surfman_warning ("fbtap_device_activate: device is already active. "
			"re-initialising, possibly leaking some memory");
	}

	if (0 > (fd = open (name, O_RDWR))) {
		surfman_error ("fbtap_device_activate could not open device!");
		goto activate_fail_0;
	}

	if (-1 == ioctl (fd, FBTAP_IOCALLOCFB, dimensions)) {
		surfman_error ("fbtap_device_activate allocfb ioctl failed");
		goto activate_fail_1;
	}

	if (-1 == ioctl (fd, FBTAP_IOCGSIZE, &fb_numpages)) {
		surfman_error ("fbtap_device_activate get size ioctl failed");
		goto activate_fail_1;
	}

	if (NULL == (fb2m = calloc (fb_numpages, sizeof (unsigned long)))) {
		surfman_error ("fbtap_device_activate calloc() failed\n");
		goto activate_fail_1;
	}

	if (-1 == ioctl (fd, FBTAP_IOCGMADDRS, fb2m)) {
		surfman_error ("fbtap_device_activate get adrs ioctl failed");
		goto activate_fail_2;
	}

	surfman_debug ("fbtap_device_activate there are %ld pages allocated:", fb_numpages);

	for (n = 0; n < 6; n++) {
		surfman_debug ("fbtap_device_activate page %u at %lx", n, fb2m[n]);
	}

	dev->fb2m = fb2m;
	dev->fb_numpages = fb_numpages;
	dev->active = 1;
	dev->filename = name;
	dev->fd = fd;

	return 0;

activate_fail_2:
	free (fb2m);

activate_fail_1:
	close (fd);

activate_fail_0:
	return -1;
}

static void
fbtap_refresh_surface (struct device *dev, struct surface *surf)
{
	/* will be periodically called from surface_refresh_timer. 
	   just redraw completely for now */

	surface_refresh (surf, NULL);
}

static void
fbtap_monitor_update (struct device *dev, int monitor_id, int en)
{
	/* don't handle this at the moment */
}

void
fbtap_takedown (struct device *surf_dev)
{
	struct fbtap_device * dev = (struct fbtap_device *) surf_dev;

	if (dev->active) {
		if (-1 == ioctl (dev->fd, FBTAP_IOCFREEFB)) {
			surfman_error ("fbtap_takedown freefb ioctl for fd %d, failed: %s",
			                                dev->fd, strerror (errno));

			return;
		}

		close (dev->fd);
		dev->active = 0;
		fbtap_device_clear (dev);
		device_destroy (surf_dev);
	}
}

static int
fbtap_set_visible (struct device *surf_dev)
{
	struct fbtap_device * fbtap_dev = (struct fbtap_device *) surf_dev;

	if (! (fbtap_dev && fbtap_dev->active && fbtap_dev->fb_numpages && fbtap_dev->fb2m)) {
		surfman_error ("fbtap_set_visible: not active");
		return -1;
	}

	//TODO: display on other monitors as well!
	return display_prepare_surface (0, surf_dev, fbtap_dev->s, NULL);
}

static struct surface *
fbtap_get_surface (struct device *surf_dev, int monitor_id)
{
	struct fbtap_device *dev = (struct fbtap_device *) surf_dev;
	return dev->s;
}

static int
fbtap_is_active (struct device *surf_dev)
{
	struct fbtap_device * dev = (struct fbtap_device *) surf_dev;
	return dev->active;
}

static struct device_ops fbtap_device_ops = {
	.name = "fbtap",
	.refresh_surface = fbtap_refresh_surface,
	.monitor_update = fbtap_monitor_update,
	.set_visible = fbtap_set_visible,
	.takedown = fbtap_takedown,
	.get_surface = fbtap_get_surface,
	.is_active = fbtap_is_active,
};

int fbtap_device_fd (struct device *surf_dev)
{
	struct fbtap_device * dev = (struct fbtap_device *)surf_dev;
	return dev->fd;
}


struct device *
fbtap_device_create (struct domain *d, int monitor_id, struct fbdim *dims)
{
	struct fbtap_device *dev;
	int format;

	surfman_info ("creating fbtap device for dom id %d, monitor %d", 
		d->domid, monitor_id);

	if (! (dev = device_create (d, &fbtap_device_ops, sizeof (*dev)))) {
		surfman_error ("could not create fbtap device");
		return NULL;
	}

	fbtap_device_clear (dev);

	surfman_info ("activating an fbtap device: res: %ux%u pixels, bpp: %u, linesize: %uB...",
		dims->xres, dims->yres, dims->bpp, dims->linesize);

	format = SURFMAN_FORMAT_BGRX8888;

	if (fbtap_device_activate (dev, NULL, dims)) {
		goto create_fail;
	}

	dev->s = surface_create ((struct device *) dev, NULL);
	surface_update_format (dev->s, dims->xres, dims->yres, dims->linesize, format, 0);
	surface_update_mmap (dev->s, d->domid, dev->fd, dev->fb2m, dev->fb_numpages);

	return (struct device *) dev;

create_fail:
	surfman_error ("could not activate fbtap device");
	fbtap_device_clear (dev);
	device_destroy ((struct device *) dev);
	return NULL;
}
