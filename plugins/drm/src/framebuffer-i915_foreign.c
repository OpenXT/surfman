/*
 * Copyright (c) 2014 Citrix Systems, Inc.
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

/* TODO: Tie up surface and framebuffers. */
#if 0
/* Foreign framebuffers. */
struct drm_framebuffer_foreign {
    struct drm_framebuffer drm;
    unsigned long *mfns;
    unsigned int mfns_count;
};
#endif
static inline struct drm_framebuffer *
__foreign_framebuffer_alloc(const struct framebuffer *source,
                            uint32_t handle, uint32_t id,
                            unsigned long *mfns, unsigned int mfns_count)
{
    struct drm_framebuffer *drm;
    (void) mfns;
    (void) mfns_count;

    drm = calloc(1, sizeof (*drm));
    if (!drm) {
        return NULL;
    }
    drm->ops = &framebuffer_foreign_ops;
    memcpy(&drm->fb, source, sizeof (*source));
    drm->fb.offset = -1;
    drm->fb.map = MAP_FAILED;
    drm->handle = handle;
    drm->id = id;
    return drm;
}

static struct drm_framebuffer *
foreign_framebuffer_create(struct drm_device *device, const struct drm_surface *surface)
{
    const struct framebuffer *sfb = &surface->fb;
    struct drm_framebuffer *dfb;
    struct drm_i915_gem_foreign creq;
    int err;
    unsigned int i;
    uint32_t id;
    int fd;

    fd = open(device->devnode, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        DRM_ERR("Could not open device at \"%s\" (%s).", device->devnode, strerror(errno));
        return NULL;
    }

    if (drmDropMaster(fd)) {
        DRM_DBG("drmDropMaster failed (%s).", strerror(errno));
    }

    memset(&creq, 0, sizeof (creq));
    creq.mfns = malloc(surface->num_mfns * sizeof (*creq.mfns));
    if (!creq.mfns) {
        err = errno;
        DRM_DBG("malloc() failed (%s).", strerror(errno));
        errno = err;
        return NULL;
    }
    for (i = 0; i < surface->num_mfns; ++i) {
        creq.mfns[i] = (uint64_t) surface->mfns[i];
    }
    creq.num_pages = surface->num_mfns;
    creq.flags = 0;     /* TODO: Switch to ballooned pages at some point? */
    if (drmIoctl(fd, DRM_IOCTL_I915_GEM_FOREIGN, &creq)) {
        err = errno;
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_I915_GEM_FOREIGN, [%#lx, ...] %u) failed (%s).",
                device->devnode, surface->mfns[0], surface->num_mfns, strerror(errno));
        free(creq.mfns);
        goto fail_foreign;
    }
    free(creq.mfns);
    if (drmModeAddFB(fd, sfb->width, sfb->height, sfb->depth, sfb->bpp, sfb->pitch,
                     creq.handle, &id)) {
        err = errno;
        DRM_DBG("drmModeAddFB(%s, %ux%u:%u/%u) failed (%s).", device->devnode,
                sfb->width, sfb->height, sfb->depth, sfb->bpp, strerror(errno));
        goto fail_fb;
    }
    dfb = __foreign_framebuffer_alloc(&surface->fb, creq.handle, id,
                                      surface->mfns, surface->num_mfns);
    if (!dfb) {
        err = errno;
        DRM_DBG("foreign_framebuffer_alloc(%s, %ux%u:%u/%u failed (%s).", device->devnode,
                sfb->width, sfb->height, sfb->depth, sfb->bpp, strerror(errno));
        goto fail_alloc;
    }
    dfb->device = device;
    dfb->fd = fd;
    return dfb;

fail_alloc:
    if (drmModeRmFB(fd, id)) {
        DRM_DBG("drmModeRmFB(%s, %u) failed (%s).", device->devnode, id, strerror(errno));
    }
fail_fb:
fail_foreign:
    errno = err;
    return NULL;
}

static int foreign_framebuffer_map(struct drm_framebuffer *framebuffer)
{
    (void) framebuffer;
    /* XXX: We don't need that yet... */
    return 0;
}

static void foreign_framebuffer_unmap(struct drm_framebuffer *framebuffer)
{
    (void) framebuffer;
    /* XXX: We don't need that yet... */
}

static void foreign_framebuffer_refresh(struct drm_framebuffer *framebuffer,
                                        const struct framebuffer *source,
                                        const struct rect *region)
{
    (void) framebuffer;
    (void) source;
    (void) region;
    /* XXX: DRM already renders what's in /source/. */
}

static void foreign_framebuffer_release(struct drm_framebuffer *framebuffer)
{
    if (drmModeRmFB(framebuffer->fd, framebuffer->id)) {
        DRM_DBG("drmModeRmFB(%s, %u) failed (%s).", framebuffer->device->devnode,
                framebuffer->id, strerror(errno));
    }
    /* close of fd releases foreign mappings */
    close(framebuffer->fd);

    free(framebuffer);
}

INTERNAL const struct drm_framebuffer_ops framebuffer_foreign_ops = {
    .name = "i915_foreign",
    .create = &foreign_framebuffer_create,
    .map = &foreign_framebuffer_map,
    .refresh = &foreign_framebuffer_refresh,
    .unmap = &foreign_framebuffer_unmap,
    .release = &foreign_framebuffer_release
};

