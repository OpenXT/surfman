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

/* TODO: Tie up surface and framebuffers. */
#if 0
struct drm_framebuffer_dumb {
    struct drm_framebuffer drm;
    struct framebuffer *source;
};
#endif

static inline struct drm_framebuffer *
__dumb_framebuffer_alloc(unsigned int width, unsigned int height,
                         unsigned int depth, unsigned int bpp,
                         unsigned int pitch, unsigned int size,
                         uint32_t handle, uint32_t id)
{
    struct drm_framebuffer *drm;

    drm = calloc(1, sizeof (*drm));
    if (!drm) {
        return NULL;
    }
    drm->ops = &framebuffer_dumb_ops;
    drm->handle = handle;
    drm->id = id;

    drm->fb.width = width;
    drm->fb.height = height;
    drm->fb.depth = depth;
    drm->fb.bpp = bpp;
    drm->fb.pitch = pitch;
    drm->fb.size = size;

    drm->fb.offset = -1;

    return drm;
}

/* This one is sourceless. This is just a hack to compose a plane on top of it. */
INTERNAL struct drm_framebuffer *
__dumb_framebuffer_create(struct drm_device *device, unsigned int width, unsigned int height,
                          unsigned int depth, unsigned int bpp)
{
    struct drm_framebuffer *dfb;
    struct drm_mode_create_dumb creq;
    struct drm_mode_destroy_dumb dreq;
    int err;
    uint32_t id;

    memset(&creq, 0, sizeof (creq));
    creq.height = height;
    creq.width = width;
    creq.bpp = bpp;
    if (drmIoctl(device->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq)) {
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_MODE_CREATE_DUMB, %ux%u:%u) failed (%s).", device->devnode,
                width, height, bpp, strerror(errno));
        return NULL;
    }
    if (drmModeAddFB(device->fd, width, height, depth, bpp, creq.pitch, creq.handle, &id)) {
        err = errno;
        DRM_DBG("drmModeAddFB(%s, %ux%u:%u/%u) failed (%s).", device->devnode,
                width, height, depth, bpp, strerror(errno));
        goto fail_fb;
    }
    dfb = __dumb_framebuffer_alloc(width, height, depth, bpp, creq.pitch, creq.size, creq.handle, id);
    if (!dfb) {
        err = errno;
        DRM_DBG("dumb_framebuffer_alloc(%s, %ux%u:%u/%u) failed (%s).", device->devnode,
                width, height, depth, bpp, strerror(errno));
        goto fail_alloc;
    }
    dfb->device = device;
    return dfb;

fail_alloc:
    if (drmModeRmFB(device->fd, id)) {
        DRM_DBG("drmModeRmFB(%s, %u) failed (%s).", device->devnode, id, strerror(errno));
    }
fail_fb:
    memset(&dreq, 0, sizeof (dreq));
    dreq.handle = creq.handle;
    if (drmIoctl(device->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq)) {
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_MODE_DESTROY_DUMB, %u) failed (%s).", device->devnode,
                creq.handle, strerror(errno));
    }
    errno = err;
    return NULL;
}

static struct drm_framebuffer *
dumb_framebuffer_create(struct drm_device *device, const struct drm_surface *surface)
{
    const struct framebuffer *sfb = &surface->fb;
    struct drm_framebuffer *dfb;
    struct drm_mode_create_dumb creq;
    struct drm_mode_destroy_dumb dreq;
    int err;
    uint32_t id;

    memset(&creq, 0, sizeof (creq));
    creq.height = sfb->height;
    creq.width = sfb->width;
    creq.bpp = sfb->bpp;
    if (drmIoctl(device->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq)) {
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_MODE_CREATE_DUMB, %ux%u:%u) failed (%s).", device->devnode,
                sfb->width, sfb->height, sfb->bpp, strerror(errno));
        return NULL;
    }
    if (drmModeAddFB(device->fd, sfb->width, sfb->height, sfb->depth, sfb->bpp,
                     creq.pitch, creq.handle, &id)) {
        err = errno;
        DRM_DBG("drmModeAddFB(%s, %ux%u:%u/%u) failed (%s).", device->devnode,
                sfb->width, sfb->height, sfb->depth, sfb->bpp, strerror(errno));
        goto fail_fb;
    }
    dfb = __dumb_framebuffer_alloc(sfb->width, sfb->height, sfb->depth, sfb->bpp,
                                   creq.pitch, creq.size, creq.handle, id);
    if (!dfb) {
        err = errno;
        DRM_DBG("dumb_framebuffer_alloc(%s, %ux%u:%u/%u) failed (%s).", device->devnode,
                sfb->width, sfb->height, sfb->depth, sfb->bpp, strerror(errno));
        goto fail_alloc;
    }
    dfb->device = device;
    return dfb;

fail_alloc:
    if (drmModeRmFB(device->fd, id)) {
        DRM_DBG("drmModeRmFB(%s, %u) failed (%s).", device->devnode, id, strerror(errno));
    }
fail_fb:
    memset(&dreq, 0, sizeof (dreq));
    dreq.handle = creq.handle;
    if (drmIoctl(device->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq)) {
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_MODE_DESTROY_DUMB, %u) failed (%s).", device->devnode,
                creq.handle, strerror(errno));
    }
    errno = err;
    return NULL;
}

static int dumb_framebuffer_map(struct drm_framebuffer *framebuffer)
{
    struct framebuffer *fb = &framebuffer->fb;
    const struct drm_device *dev = framebuffer->device;
    struct drm_mode_map_dumb mreq;
    int rc;

    memset(&mreq, 0, sizeof (mreq));
    mreq.handle = framebuffer->handle;
    if (drmIoctl(dev->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
        rc = -errno;
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_MODE_MAP_DUMB, %u) failed (%s).", dev->devnode,
                framebuffer->handle, strerror(errno));
        return rc;
    }
    fb->map = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, mreq.offset);
    if (fb->map == MAP_FAILED) {
        rc = -errno;
        DRM_DBG("mmap(%u, RW, SHARED, %s, %llu) failed (%s).", fb->size, dev->devnode,
                mreq.offset, strerror(errno));
        return rc;
    }
    fb->offset = mreq.offset;
    return 0;
}

static void dumb_framebuffer_unmap(struct drm_framebuffer *framebuffer)
{
    if (munmap(framebuffer->fb.map, framebuffer->fb.size)) {
        DRM_DBG("munmap() failed (%s).", strerror(errno));
    }
}

static void dumb_framebuffer_refresh(struct drm_framebuffer *drm,
                                     const struct framebuffer *source,
                                     const struct rect *r)
{
    struct framebuffer *dfb = &drm->fb;
    const struct framebuffer *sfb = source;
    unsigned int i;

    uint8_t *src = sfb->map + r->y * sfb->pitch + r->x * (sfb->bpp / 8);
    uint8_t *dst = dfb->map + r->y * dfb->pitch + r->x * (dfb->bpp / 8);

    /* Those are just safeguards for now in case I fucked up something else. */
    if ((sfb->map == MAP_FAILED || sfb->map == NULL) ||
        (dfb->map == MAP_FAILED || dfb->map == NULL)) {
        DRM_WRN("Trying to refresh unmapped framebuffer: source:%p, sink:%p",
                sfb->map, dfb->map);
        return;
    }
    if ((sfb->depth != dfb->depth) || (sfb->bpp != dfb->bpp)) {
        DRM_WRN("Invalid geometry between Surfman and libDRM: %u/%u vs %u/%u (depth/bpp)",
                sfb->depth, sfb->bpp, dfb->depth, dfb->bpp);
        return;
    }
    if ((r->y + r->h > sfb->height) || (r->x + r->w > sfb->width)) {
        DRM_WRN("Dirty rectangle out of framebuffer bounds: %ux%u vs %u,%u:%ux%u",
                sfb->width, sfb->height, r->x, r->y, r->w, r->h);
        return;
    }

    for (i = 0; i < r->h; ++i) {
        memcpy(dst, src, r->w * (dfb->bpp / 8));
        src += sfb->pitch;
        dst += dfb->pitch;
    }
}


static void dumb_framebuffer_release(struct drm_framebuffer *framebuffer)
{
    struct drm_mode_destroy_dumb dreq = { 0 };

    if (framebuffer->fb.map && framebuffer->fb.map != MAP_FAILED) {
        dumb_framebuffer_unmap(framebuffer);
    }
    if (drmModeRmFB(framebuffer->device->fd, framebuffer->id)) {
        DRM_DBG("drmModeRmFB(%s, %u) failed (%s).",
                framebuffer->device->devnode, framebuffer->id, strerror(errno));
    }
    dreq.handle = framebuffer->handle;
    if (drmIoctl(framebuffer->device->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq)) {
        DRM_DBG("drmIoctl(%s, DRM_IOCTL_MODE_DESTROY_DUMB, %u) failed (%s).",
                framebuffer->device->devnode, framebuffer->handle, strerror(errno));
    }
}

INTERNAL const struct drm_framebuffer_ops framebuffer_dumb_ops = {
    .name = "dumb",
    .create = dumb_framebuffer_create,
    .map = dumb_framebuffer_map,
    .unmap = dumb_framebuffer_unmap,
    .refresh = dumb_framebuffer_refresh,
    .release = dumb_framebuffer_release
};

