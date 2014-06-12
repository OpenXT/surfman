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

/* Monitoring helper. */
INTERNAL void drm_monitor_info(const struct drm_monitor *m)
{
    drmModeConnectorPtr con;
    drmModeEncoderPtr enc;
    drmModeCrtcPtr crtc;
    int rc;

    assert(m);
    assert(m->device);

    con = drmModeGetConnector(m->device->fd, m->connector);
    if (!con) {
        return; /* Silently give up. */
    }
    enc = drmModeGetEncoder(m->device->fd, con->encoder_id);
    if (!enc) {
        rc = errno;
        DRM_INF("Connector:%d, %s, %s (%s)",
                con->connector_id, connector_type_str(con->connector_type_id),
                connector_status_str(con->connection),
                ((con->connection == DRM_MODE_CONNECTED) && rc) ? strerror(rc) : "No encoder");
    } else {
        crtc = drmModeGetCrtc(m->device->fd, enc->crtc_id);
        if (!crtc) {
            rc = errno;
            DRM_INF("Connector:%d, %s, %s -> Encoder:%d, %s (%s)",
                    con->connector_id, connector_type_str(con->connector_type_id),
                    connector_status_str(con->connection),
                    enc->encoder_id, encoder_type_str(enc->encoder_type),
                    rc ? strerror(rc) : "No CRTC");
        } else {
            if (!m->surface) {
                DRM_INF("Connector:%d, %s, %s -> Encoder:%d, %s -> CRTC:%d, (%u,%u) %ux%u, mode:%s (%ux%u@%uHz)",
                        con->connector_id, connector_type_str(con->connector_type_id),
                        connector_status_str(con->connection),
                        enc->encoder_id, encoder_type_str(enc->encoder_type),
                        crtc->crtc_id, crtc->x, crtc->y, crtc->width, crtc->height,
                        crtc->mode.name, crtc->mode.hdisplay, crtc->mode.vdisplay, crtc->mode.vrefresh);
            } else {
                DRM_INF("Connector:%d, %s, %s -> Encoder:%d, %s -> CRTC:%d, (%u,%u) %ux%u, mode:%s (%ux%u@%uHz), surface: dom%u",
                        con->connector_id, connector_type_str(con->connector_type_id),
                        connector_status_str(con->connection),
                        enc->encoder_id, encoder_type_str(enc->encoder_type),
                        crtc->crtc_id, crtc->x, crtc->y, crtc->width, crtc->height,
                        crtc->mode.name, crtc->mode.hdisplay, crtc->mode.vdisplay, crtc->mode.vrefresh,
                        m->surface->domid);
            }
            drm_mode_print(&(crtc->mode));
            drmModeFreeCrtc(crtc);
        }
        drmModeFreeEncoder(enc);
    }
    drmModeFreeConnector(con);
}

/* Probes libDRM for monitors (connected connectors) and fill /monitors/ with it. */
INTERNAL int drm_monitors_scan(struct drm_device *device)
{
    drmModeResPtr r;
    unsigned int max;
    int i, rc = 0;

    r = drmModeGetResources(device->fd);
    if (!r) {
        rc = -errno;
        DRM_WRN("Could not retrieve device \"%s\" resources (%s).",
                device->devnode, strerror(errno));
        return rc;
    }
    max = min(r->count_crtcs, r->count_connectors);
    /* Those missing unsigned ... */
    for (i = 0; (i < r->count_connectors) && (max != 0); ++i) {
        drmModeConnector *c;

        c = drmModeGetConnector(device->fd, r->connectors[i]);
        if (!c) {
            DRM_WRN("Could not access connector %u on device \"%s\" (%s).",
                    r->connectors[i], device->devnode, strerror(errno));
            continue;
        }
        /* TODO: ok there's not many monitors for now... But complexity is pretty high
         *       there... Should be hash-tables really... */
        if (c->connection == DRM_MODE_CONNECTED) {
            /* Either known or newly connected monitor. */
            if (!drm_device_add_monitor(device, c->connector_id, &c->modes[0])) {
                rc = -ENOMEM;
                break; /* Give up on memory errors. */
            }
            --max;
        } else {
            /* Unplugged monitor, check if we knew about that. */
            drm_device_del_monitor(device, c->connector_id);
            /* XXX: Since we rescan the entire libDRM list, don't touch max_monitors! */
        }
        drmModeFreeConnector(c);  /* libDRM id is enough for now. */
    }
    drmModeFreeResources(r);
    return rc;
}

/*
 * Helper to get the DPMS property ID for this connector.
 */
static uint32_t drmModeGetDpmsPropID(int fd, drmModeConnector *connector)
{
    drmModePropertyPtr p;
    uint32_t pid;
    int i;

    for (i = 0; i < connector->count_props; i++) {
        p = drmModeGetProperty(fd, connector->props[i]);
        if (!p)
            continue;

        if ((p->flags & DRM_MODE_PROP_ENUM) && strcmp(p->name, "DPMS") == 0) {
            pid = p->prop_id;
            drmModeFreeProperty(p);
            return pid;
        }
        drmModeFreeProperty(p);
    }
    return 0;
}

/*
 * Helper to set the DPMS property for this connector.
 */
static int drmModeSetDpmsProp(int fd, drmModeConnector *connector, int value)
{
    uint32_t pid;

    pid = drmModeGetDpmsPropID(fd, connector);
    if (!pid) {
        return -ENOENT;
    }
    if (drmModeConnectorSetProperty(fd, connector->connector_id, pid, value)) {
        return -errno;
    }
    return 0;
}

static int drm_monitor_disable_dpms(struct drm_monitor *monitor)
{
    drmModeConnector *c;
    int rc;

    c = drmModeGetConnector(monitor->device->fd, monitor->connector);
    if (!c) {
        return -errno;
    }

    /* TODO for now, set DPMS to On for each monitor as it is initialized. I believe
     * this will disable the default power saving timeout. It may end up that this is
     * the best place to initially set DPMS state to On in the end and we manage timing
     * it out into power saving states with xenmgr.
     */
    rc = drmModeSetDpmsProp(monitor->device->fd, c, DRM_MODE_DPMS_ON);
    drmModeFreeConnector(c);
    return rc;
}

/*
 * Check that default connector configuration is suitable for this monitor.
 */
static int drm_monitor_check_pipe(struct drm_monitor *monitor)
{
    drmModeConnector *c;
    drmModeEncoder *e;
    int rc = 0;

    c = drmModeGetConnector(monitor->device->fd, monitor->connector);
    if (!c) {
        return -errno;
    }
    e = drmModeGetEncoder(monitor->device->fd, c->encoder_id);
    if (!e || !e->crtc_id) {
        rc = -errno;
        goto out_con;
    }
    if (drm_device_crtc_is_used(monitor->device, e->crtc_id)) {
        rc = -EBUSY;
        goto out_enc;
    }
    monitor->encoder = e->encoder_id;
    monitor->crtc = e->crtc_id;

out_enc:
    drmModeFreeEncoder(e);
out_con:
    drmModeFreeConnector(c);
    return rc;
}

/*
 * Probe DRM resources for compatible CRTC/Encoder for that connector.
 */
static int drm_monitor_probe_pipe(struct drm_monitor *monitor)
{
    drmModeRes *r;
    drmModeConnector *c;
    drmModeEncoder *e;
    int i, j, rc = 0;

    r = drmModeGetResources(monitor->device->fd);
    if (!r) {
        return -errno;
    }
    c = drmModeGetConnector(monitor->device->fd, monitor->connector);
    if (!c) {
        rc = -errno;
        goto out_res;
    }
    for (i = 0; i < c->count_encoders; ++i) {
        e = drmModeGetEncoder(monitor->device->fd, c->encoders[i]);
        if (!e) {
            continue;   /* It's bad, but lets try the next one. */
        }
        for (j = 0; j < r->count_crtcs; ++j) {
            if (!(e->possible_crtcs & (1 << j))) {
                continue;   /* Incompatible, skip. */
            }
            DRM_DBG("CRTC %u is compatible with encoder %u.", r->crtcs[j], e->encoder_id);
            if (!drm_device_crtc_is_used(monitor->device, r->crtcs[j])) {
                monitor->encoder = e->encoder_id;
                monitor->crtc = r->crtcs[j];
                drm_monitor_dump(monitor);
                drmModeFreeEncoder(e);
                goto out_con;
            }
        }
        drmModeFreeEncoder(e);
    }
    rc = -ENODEV;
out_con:
    drmModeFreeConnector(c);
out_res:
    drmModeFreeResources(r);
    return rc;
}

INTERNAL int drm_monitor_init(struct drm_monitor *monitor)
{
    /* Monitor is valid only for its device. */
    struct drm_device *d = monitor->device;
    int rc;

    /* Prevent screen from blanking. */
    rc = drm_monitor_disable_dpms(monitor);
    if (rc) {
        DRM_WRN("Could not set DPMS On for connector %u on device \"%s\" (%s).",
                monitor->connector, d->devnode, strerror(-rc));
        /* I don't consider it fatal for now. */
    } else {
        DRM_DBG("Dpms successfuly disabled on connector %u device \"%s\".",
                monitor->connector, d->devnode);
    }

    /* Default configuration is fine most of the time. */
    rc = drm_monitor_check_pipe(monitor);
    if (!rc) {
        /* We have a valid pipe. */
        DRM_DBG("Monitor setup: Connector %u -> Encoder %u -> CRTC %u.",
                monitor->connector, monitor->encoder, monitor->crtc);
        return 0;
    }

    DRM_WRN("Default configuration failed for connector %u (%s).",
            monitor->connector, strerror(-rc));
    /* Either of two cases brings us here:
     * - The connector has no encoder bound to it (weird ... Means the connector is useless ...).
     * - The connector is bound to an encoder already used by another CRTC, so unavailable.
     *
     * So we have to find an appropriate encoder or abort if none available. */
    rc = drm_monitor_probe_pipe(monitor);
    if (rc) {
        DRM_WRN("Could not find resources to drive connector %u (%s).",
                monitor->connector, strerror(-rc));
        return rc;
    }
    DRM_DBG("Monitor setup: Connector %u -> Encoder %u -> CRTC %u.",
            monitor->connector, monitor->encoder, monitor->crtc);
    return 0;
}

