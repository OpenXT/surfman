/*
 * Copyright (c) 2011 Citrix Systems, Inc.
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

#ifndef GLGFX_H
#define GLGFX_H

typedef struct glgfx_surface_ {
    int initialised;
    GLuint tex; // texture handle
    GLuint pbo; // pbo handle
    GLuint w,h,stride,last_w,last_h;
    GLubyte *mapped_fb;
    unsigned int mapped_fb_size;
    surfman_surface_t *src;

    /* animation stuff*/
    struct glgfx_surface_ *anim_prev, *anim_next;
    int anim_active;
    float anim_phase;
} glgfx_surface;

#endif

