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

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <xenctrl.h>
#include <surfman.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/glx.h>
#include <GL/gl.h>
#include "glgfx.h"

#define MAX_MONITORS 32
#define MAX_SURFACES 256
#define MAX_DISPLAY_CONFIGS 64

#define XORG_TEMPLATE "/etc/X11/xorg.conf-glgfx-nvidia"

static int g_attributes[] = {
    GLX_RGBA, GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    None
};
Display *g_display = NULL;
static GLXContext g_context = NULL;
static Colormap g_colormap;
static int g_have_colormap = 0;
static Window g_window = None;
static int g_have_window = 0;
static xc_interface *g_xc = NULL;
static glgfx_surface *g_current_surface = NULL;
static struct event hotplug_timer;

typedef struct {
    /* taken from xinerama */
    int xoff,yoff,w,h;
} xinemonitor_t;


xinemonitor_t g_monitors[MAX_MONITORS];
int g_num_monitors = 0;

static int g_num_surfaces = 0;
static glgfx_surface* g_surfaces[MAX_SURFACES];

/* current display configuration */
static surfman_display_t g_dispcfg[MAX_DISPLAY_CONFIGS];
static int g_num_dispcfgs = 0;

static int stop_X();

static int
get_gpu_busid(int *b, int *d, int *f)
{
    FILE *p;
    char buff[1024];
    int id = -1;

    *b = *d = *f = 0;

    p = popen("nvidia-xconfig --query-gpu-info", "r");
    if (!p)
    {
        warning("nvidia-xconfig --query-gpu-info failed\n");
        return 0;
    }

    while (!feof(p))
    {
        if (!fgets(buff, 1024, p))
            break;

        sscanf(buff, "GPU #%d:\n", &id);

        if (id == 0)
        {
            if (sscanf(buff, "  PCI BusID : PCI:%d:%d:%d\n", b, d, f) == 3)
                break;
        }
    }
    pclose(p);

    return (*b != 0 || *d != 0 || *f != 0);
}

static char *
generate_xorg_config()
{
    int b, d, f;
    FILE *in, *out;
    char buff[1024];
    int id = -1;
    int fd = -1;
    int generated = 0;
    char filename[] = "/tmp/xorg.conf-glgfx-nvidia-XXXXXX";

    if (!get_gpu_busid(&b, &d, &f))
    {
        error("Can't get GPU#0 busid\n");
        return NULL;
    }

    info("Found Nvidia GPU#0 %04x:%02x.%01x", b, d, f);


    info("Temp fd %d\n", fd);
    fd = mkstemp(filename);
    info("Temp fd %d %s\n", fd, filename);
    out = fdopen(fd, "w");
    info("Generated config file %s\n", filename);

    in = fopen(XORG_TEMPLATE, "r");

    if (!in || !out)
        goto out;

    while (!feof(in))
    {
        if (!fgets(buff, 1024, in))
            break;

        info(buff);

        if (strcmp(buff, "	BusID       \"PCI:b:d:f\"\n") == 0)
        {
            fprintf(out, "	BusID       \"PCI:%d:%d:%d\"\n",
                    b, d, f);
            generated = 1;
        }
        else
            fprintf(out, buff);
    }
out:
    if (out)
        fclose(out);
    if (in)
        fclose(in);

    if (!generated)
        return NULL;

    return strdup(filename);
}

static void
resize_gl( int w, int h )
{
    info("resize_gl");
    h = h == 0 ? 1 : h;
    glViewport( 0, 0, w, h );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, w-1, h-1, 0, -10, 10 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void
init_gl()
{
    info("init_gl");
    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT  );
    glEnable( GL_TEXTURE );
    glEnable( GL_TEXTURE_2D );
    glFlush();
}

static void
resize_pbo( GLuint id, size_t sz )
{
    GLubyte *ptr;
    info( "sizing PBO %d to %d bytes", id, sz );
    glBindBuffer( GL_PIXEL_UNPACK_BUFFER, id );
    glBufferData( GL_PIXEL_UNPACK_BUFFER, sz, 0, GL_DYNAMIC_DRAW );

    ptr = (GLubyte*) glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
    info( "PBO %d seems to map onto %p", id, ptr );
    glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );
}

static GLuint
create_pbo( size_t sz )
{
    GLuint id;
    glGenBuffers( 1, &id );
    info( "created PBO %d", id );
    return id;
}

static GLuint
create_texobj()
{
    GLuint id;
    info("creating texture object");
    glGenTextures( 1, &id );
    glBindTexture( GL_TEXTURE_2D, id );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    info( "created texture object %d", id );
    return id;
}

static int get_gl_formats( int surfman_surface_fmt, GLenum *gl_fmt, GLenum *gl_typ )
{
    switch (surfman_surface_fmt) {
    case SURFMAN_FORMAT_BGRX8888:
        *gl_fmt = GL_BGRA;
        *gl_typ = GL_UNSIGNED_BYTE;
        return 0;
    case SURFMAN_FORMAT_RGBX8888:
        *gl_fmt = GL_RGBA;
        *gl_typ = GL_UNSIGNED_BYTE;
        return 0;
    case SURFMAN_FORMAT_BGR565:
        *gl_fmt = GL_BGR;
        *gl_typ = GL_UNSIGNED_SHORT_5_6_5;
        return 0;
    default:
        error("unsupported surfman surface format %d", surfman_surface_fmt );
        return -1;
    }
}

static void
upload_pbo_to_texture(
    GLuint pbo,
    GLenum pbo_format,
    GLenum pbo_type,
    GLuint tex,
    int recreate_texture,
    int x,
    int y,
    int w,
    int h,
    int stride_in_bytes )
{
    glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo );
    glBindTexture( GL_TEXTURE_2D, tex );

    glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, stride_in_bytes / 4 ); // assume 4bpp for now
    if (recreate_texture) {
        glTexImage2D( GL_TEXTURE_2D, 0, 4, w, h, 0, pbo_format, pbo_type, 0 );
    } else {
        glTexSubImage2D( GL_TEXTURE_2D, 0, x, y, w, h, pbo_format, pbo_type, 0 );
    }
}

static int
copy_to_pbo( GLuint pbo, GLubyte *src, size_t len, uint8_t *dirty_bitmap )
{
    GLubyte *ptr;
    glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo );
    ptr = (GLubyte*) glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
    if (!ptr) {
        error("copy_to_pbo %d %p %d: map buffer FAILED", pbo, src, len);
        return -1;
    }
    if (!dirty_bitmap) {
        memcpy( ptr, src, len );
    } else {
        /* FIXME: this is bit suboptimal */
        int num_pages = (len+XC_PAGE_SIZE-1) / XC_PAGE_SIZE;
        int num_bytes = num_pages/8;
        uint8_t* p = dirty_bitmap;
        int i = 0, j;
        while (i < num_bytes) {
            switch (*p) {
            case 0x00:
                break;
            default:
                memcpy( ptr + XC_PAGE_SIZE*i*8, src + XC_PAGE_SIZE*i*8, XC_PAGE_SIZE*8 );
                break;
            }
            ++p;
            ++i;
        }
        ptr = ptr + XC_PAGE_SIZE*i*8;
        src = src + XC_PAGE_SIZE*i*8;
        i = 0;
        while (i < num_pages%8) {
            if (*p & (1<<i)) {
                memcpy( ptr + XC_PAGE_SIZE*i, src + XC_PAGE_SIZE*i, XC_PAGE_SIZE );
            }
            ++i;
        }
    }

    glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );
    return 0;

}

static void
upload_to_gpu( surfman_surface_t *src, glgfx_surface *dst, uint8_t *dirty_bitmap )
{
    GLenum fb_format, fb_type;
    int recreate=0, rv;
    if ( !dst->initialised ) {
        error("gpu surface not initialised");
        return;
    }
    if ( !dst->mapped_fb ) {
        error("framebuffer does not appear to be mapped");
        return;
    }
    if ( dst->mapped_fb_size < dst->stride * dst->h ) {
        error("framebuffer mapping appears to be too short");
        return;
    }
    if ( get_gl_formats( src->format, &fb_format, &fb_type ) < 0 ) {
        error("unsupported/unknown surface format");
        return;
    }
    if ( dst->last_w != dst->w || dst->last_h != dst->h ) {
        recreate = 1;
    }
    if ( recreate ) {
        info("stride: %d, height: %d", dst->stride, dst->h);
        resize_pbo( dst->pbo, dst->stride*dst->h );
    }
    rv = copy_to_pbo( dst->pbo, dst->mapped_fb, dst->stride * dst->h, recreate ? NULL : dirty_bitmap );
    upload_pbo_to_texture( dst->pbo, fb_format, fb_type, dst->tex, recreate, 0, 0, dst->w, dst->h, dst->stride );
    /* only overwrite if success from previous ops */
    if (rv == 0) {
        dst->last_w = dst->w;
        dst->last_h = dst->h;
    }
}

static void
map_fb( surfman_surface_t *src, glgfx_surface *dst )
{
    if ( !( dst->mapped_fb = surface_map( src ) ) ) {
        error("failed to map framebuffer pages");
    }
}

static int
init_surface_resources( glgfx_surface *surface )
{
    GLubyte *buf;

    info("init surface resources for %p", surface);
    if (surface->initialised) {
        error("surface already initialised");
        return -1;
    }
    surface->tex = create_texobj();
    surface->pbo = create_pbo( surface->w*surface->stride );
    map_fb( surface->src, surface );
    surface->initialised = 1;
    return 0;
}

static void
free_surface_resources( glgfx_surface *surf )
{
    if (surf) {
        info("free surface resources %p", surf);
        if (surf->anim_next && surf->anim_next->anim_prev == surf) {
            surf->anim_next->anim_prev = NULL;
        }
        if (surf->anim_prev && surf->anim_prev->anim_next == surf) {
            surf->anim_prev->anim_next = NULL;
        }
        glDeleteTextures( 1, &surf->tex );
        glBufferData( GL_PIXEL_UNPACK_BUFFER, 0, NULL, GL_DYNAMIC_DRAW );
        glDeleteBuffers( 1, &surf->pbo );
        surface_unmap( surf );
        surf->initialised = 0;
    }
}

static void
render( glgfx_surface *surface, int w, int h )
{
    glBindTexture( GL_TEXTURE_2D, surface->tex );
    glColor3f( 1,1,1 );
    glBegin( GL_QUADS );
    glTexCoord2f( 0,0 ); glVertex2f( 0, 0 );
    glTexCoord2f( 1,0 ); glVertex2f( w-1, 0 );
    glTexCoord2f( 1,1 ); glVertex2f( w-1, h-1 );
    glTexCoord2f( 0,1 ); glVertex2f( 0, h-1 );
    glEnd();
    glFlush();
}

static void
render_as_tiles( glgfx_surface *surface, int xtiles, int ytiles, float z, int rev,
                 int w, int h, float phase )
                 
{
    int x,y;
    float xstep = 1.0 / xtiles;
    float ystep = 1.0 / ytiles;
    float xc = xstep*xtiles/2;
    float yc = ystep*ytiles/2;
    glBindTexture( GL_TEXTURE_2D, surface->tex );
    glColor3f( 1,1,1 );
    for (y = 0; y < ytiles; ++y) {
        for (x = 0; x < xtiles; ++x) {
            float x0 = xstep*x;
            float y0 = ystep*y;
            float x1 = x0+xstep;
            float y1 = y0+xstep;
            float dst = sqrtf(sqrtf((x1-xc)*(x1-xc)+(y1-yc)*(y1-yc)));
            float speed = cos(dst*3.14159/2)*2;
            if (speed<1) speed=1;
            float rot = phase * speed * 180;
            if (rev) {
                rot = 180-rot;
            }
            glLoadIdentity();
            glTranslatef( x0*(w-1), y0*(h-1), z );
            if (rot > 180) rot=180;
            if (rot < 0) rot=0;
            glRotatef( rot, 1, 1, 0 );
            glBegin( GL_QUADS );
            glTexCoord2f( x0,y0 ); glVertex3f( 0,0,0 );
            glTexCoord2f( x1,y0 ); glVertex3f( xstep*(w-1),0,0 );
            glTexCoord2f( x1,y1 ); glVertex3f( xstep*(w-1),ystep*(h-1),0 );
            glTexCoord2f( x0,y1 ); glVertex3f( 0,ystep*(h-1),0 );
            glEnd();
        }
    }
}

static void
render_animated( glgfx_surface *surface, int w, int h )
{
    if ( !surface->anim_active ) {
        render( surface, w, h );
    } else {
        glPushAttrib( GL_DEPTH_BUFFER_BIT );
        glEnable( GL_CULL_FACE );
        glClearColor( 0, 0, 0, 0 );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT  );

        glFrontFace( GL_CW );
        render_as_tiles( surface, 16, 16, 1, 1, w, h, surface->anim_phase );

        if ( surface->anim_prev ) {
            glFrontFace( GL_CW );
            render_as_tiles( surface->anim_prev, 16, 16, -1, 0, w, h, surface->anim_phase );
        }

        /* update animation frame */
        surface->anim_phase += 0.02;
        if (surface->anim_phase > 1) {
            surface->anim_phase = 0;
            surface->anim_active = 0;
            surface->anim_next = NULL;
            if (surface->anim_prev) {
                surface->anim_prev->anim_next = NULL;
            }
        }
        glPopAttrib();
        glDisable( GL_CULL_FACE );
    }
    glFlush();
}

static void
hide_x_cursor(Display *display, Window window)
{
    Cursor invis_cur;
    Pixmap empty_pix;
    XColor black;
    static char empty_data[] = { 0,0,0,0,0,0,0,0 };
    black.red = black.green = black.blue = 0;
    empty_pix = XCreateBitmapFromData(display, window, empty_data, 8, 8);
    invis_cur = XCreatePixmapCursor(
        display, empty_pix, empty_pix, 
        &black, &black, 0, 0);
    XDefineCursor(display, window, invis_cur);
    XFreeCursor(display, invis_cur);
}

static void
dump_visuals(Display *display)
{
    XVisualInfo template;
    XVisualInfo *vi = NULL;
    int count;
    vi = XGetVisualInfo( display, 0, &template, &count );
    if (vi) {
        int i;
        for ( i = 0; i < count; ++i ) {
            XVisualInfo *v = &vi[i];
            info( "visual %d: depth=%d bits_per_rgb=%d, screen=%d, class=%d", i, v->depth, v->bits_per_rgb, v->screen, v->class );
        }
        XFree( vi );
    }
}

static int
init_window(int w, int h)
{
    int screen;
    XVisualInfo *vi;
    XSetWindowAttributes winattr;

    info( "create X window");
    if (!g_display) {
        error("init_window: no display");
        return -1;
    }
    screen = DefaultScreen(g_display);
    info( "screen=%d", screen);
    dump_visuals( g_display );
    vi = glXChooseVisual( g_display, screen, g_attributes );
    if (!vi) {
        error("failed to choose appropriate visual");
        return -1;
    }
    g_context = glXCreateContext( g_display, vi, 0, GL_TRUE );
    g_colormap = XCreateColormap( g_display, RootWindow(g_display, vi->screen), vi->visual, AllocNone );
    g_have_colormap = 1;

    winattr.colormap = g_colormap;
    winattr.border_pixel = 0;
    winattr.event_mask = ExposureMask | StructureNotifyMask;
    g_window = XCreateWindow(
        g_display, RootWindow(g_display, vi->screen),
        0, 0, w, h, 0, vi->depth, InputOutput, vi->visual,
        CWBorderPixel | CWColormap | CWEventMask, &winattr
        );
    XMapRaised( g_display, g_window );
    glXMakeCurrent( g_display, g_window, g_context );
    if ( glXIsDirect( g_display, g_context ) ) {
        info( "DRI enabled");
    } else {
        info( "no DRI available");
    }

    hide_x_cursor( g_display, g_window );

    info ("GL extensions: %s", glGetString( GL_EXTENSIONS ));
    init_gl();
    resize_gl( w, h );

    info ("window init completed");

    XFree( vi );

    return 0;
}

static void
update_monitors()
{
    if (!g_display) {
        error("update_monitors: NO DISPLAY");
        return;
    }
    if (XineramaIsActive(g_display)) {
        int num_screens = 0, i;
        XineramaScreenInfo *info = XineramaQueryScreens( g_display, &num_screens );
        XineramaScreenInfo *scr = info;
        g_num_monitors = 0;
        for (i = 0; i < num_screens && i < MAX_MONITORS; ++i) {
            xinemonitor_t *m = &g_monitors[i];
            info( "Xinerama screen %d: x_org=%d y_org=%d width=%d height=%d",
                  i, scr->x_org, scr->y_org, scr->width, scr->height );
            m->xoff = scr->x_org;
            m->yoff = scr->y_org;
            m->w = scr->width;
            m->h = scr->height;
            ++scr;
            ++g_num_monitors;
        }
        XFree( info );
    } else {
        /* one monitor then */
        int scr;
        xinemonitor_t *m;
        info("XINERAMA is inactive");
        scr = XDefaultScreen(g_display);
        if (!scr) {
            error("update_monitors w/o xinerama: NO DEFAULT SCREEN?");
            return;
        }
        
        m = &g_monitors[0];
        m->xoff = m->yoff = 0;
        m->w = DisplayWidth(g_display, scr);
        m->h = DisplayHeight(g_display, scr);
        g_num_monitors = 1;
    }

}

static int
start_X()
{
    Display *display = NULL;
    int i;
    int rv;
    const int TRIES = 10;
    char *config_filename;
    char cmd[1024];

    config_filename = generate_xorg_config();
    if (!config_filename)
    {
        error("Can't generate xorg config file");
        return SURFMAN_ERROR;
    }

    info( "starting X server (config:%s)", config_filename);
    snprintf(cmd, 1024, "start-stop-daemon -S -b --exec /usr/bin/X -- -config %s",
            config_filename);
    free(config_filename);
    unlink(config_filename);

    rv = system(cmd);
    if (rv < 0) {
        return rv;
    }
    for (i = 0; i < TRIES; ++i) {
        display = XOpenDisplay( ":0" );
        if (display) {
            info( "opened X display");
            break;
        }
        sleep(1);
    }

    if (!display) {
        error("failed to open X display");
        return SURFMAN_ERROR;
    }

    g_display = display;

    if (XineramaIsActive(display)) {
        info("XINERAMA is active");
        int num_screens = 0;
        XineramaScreenInfo *info = XineramaQueryScreens( display, &num_screens );
        XineramaScreenInfo *scr = info;
        for (i = 0; i < num_screens; ++i) {
            info( "Xinerama screen %d: x_org=%d y_org=%d width=%d height=%d",
                  i, scr->x_org, scr->y_org, scr->width, scr->height );
            ++scr;
        }
        XFree( info );
    } else {
        info("XINERAMA is inactive");
    }

    return SURFMAN_SUCCESS;
}

static int
start_X_and_create_window()
{
    int rv = start_X();
    if (rv < 0) {
        return rv;
    }
    if (!g_have_window) {
        int screen, w, h;
        screen = DefaultScreen(g_display);
        w = DisplayWidth(g_display, screen);
        h = DisplayHeight(g_display, screen);
        info("creating window %d %d screen=%d", w, h, screen);
        rv = init_window(w,h);
        if (rv < 0) {
            error("failed to create window");
            return SURFMAN_ERROR;
        }
        g_have_window = 1;
    }
    return 0;
}

static int
stop_X()
{
    int rv, tries=0,i;
    info( "stopping X server");

    /* free all surface resources */
    for (i = 0; i < g_num_surfaces; ++i) {
        info("freeing surface %d", i);
        free_surface_resources( g_surfaces[i] );
    }

    if (g_context) {
        info("freeing context");
        glXMakeCurrent( g_display, None, NULL );
        glXDestroyContext( g_display, g_context );
        g_context = NULL;
    }
    if (g_window != None) {
        info("freeing window");
        XUnmapWindow( g_display, g_window );
        XDestroyWindow( g_display, g_window );
        g_window = None;
        g_have_window = 0;
    }
    if (g_have_colormap) {
        info("freeing colormap");
        XFreeColormap( g_display, g_colormap );
        g_have_colormap = 0;
    }
    if (g_display) {
        info("closing display");
        XCloseDisplay( g_display );
        g_display = NULL;
    }

    rv = system("start-stop-daemon -K --exec /usr/bin/X");
    if (rv != 0) {
        return SURFMAN_ERROR;
    }
    while (tries < 10) {
        rv = system("pidof X");
        if (rv == 0) {
            info("waiting for X server to disappear..");
            sleep(1);
            ++tries;
        } else {
            info("X server disappeared");
            return SURFMAN_SUCCESS;
        }
    }
    error("timeout waiting for X server to disappear");
    return SURFMAN_ERROR;
}



static int
check_monitor_hotplug(surfman_plugin_t *plugin)
{
    static int monitors = 1;
    int l_monitor;
    int id;
    FILE *f = NULL;
    char buff[1024];
    struct timeval tv = {1, 0};

    event_add(&hotplug_timer, &tv);

    f = popen("nvidia-xconfig --query-gpu-info", "r");
    if (!f)
    {
        warning("nvidia-xconfig --query-gpu-info failed\n");
        return monitors;
    }

    while (!feof(f))
    {
        if (!fgets(buff, 1024, f))
            break;

        if (sscanf(buff, "GPU #%d:\n", &id) == 1 &&
                id != 0)
            break;

        if (sscanf(buff, "  Number of Display Devices: %d\n", &l_monitor) == 1)
        {
            if (monitors != l_monitor)
            {
                if (monitors != -1)
                {
                    info("Detect monitor hotplug, %d monitors, before was %d\n",
                            l_monitor, monitors);
                    stop_X();
                    start_X_and_create_window();
                    plugin->notify = SURFMAN_NOTIFY_MONITOR_RESCAN;
                }
                monitors = l_monitor;
                break;
            }
        }
    }
    pclose(f);
    return monitors;
}

static void
check_monitor_hotplug_cb(int fd, short event, void *opaque)
{
    check_monitor_hotplug(opaque);
}

static int
have_nvidia()
{
    return system("lspci -d 10de:* -mm -n | cut -d \" \" -f 2 | grep 0300") == 0;
}

static int
glgfx_init (surfman_plugin_t * p)
{
    int rv;
    struct timeval tv = {1, 0};

    info( "glgfx_init");
    if (!have_nvidia()) {
        error("NVIDIA device not present");
        return SURFMAN_ERROR;
    }
    g_xc = xc_interface_open(NULL, NULL, 0);
    if (!g_xc) {
        error("failed to open XC interface");
        return SURFMAN_ERROR;
   }
    rv = start_X_and_create_window();
    if (rv < 0) {
        error("starting X failed");
        return SURFMAN_ERROR;
    }

    event_set(&hotplug_timer, -1, EV_TIMEOUT,
            check_monitor_hotplug_cb, p);
    event_add(&hotplug_timer, &tv);

    return SURFMAN_SUCCESS;
}

static void
glgfx_shutdown (surfman_plugin_t * p)
{
    int rv;
    info("shutting down");
    if ( g_context ) {
        if ( !glXMakeCurrent(g_display, None, NULL)) {
            error("could not release drawing context");
        }
        glXDestroyContext( g_display, g_context );
    }
    if ( g_display ) {
        XCloseDisplay( g_display );
        g_display = NULL;
    }
    rv = stop_X();
    if (rv < 0) {
        error("stopping X failed");
    }
    xc_interface_close(g_xc);
    g_xc = NULL;
}

static int
glgfx_display (surfman_plugin_t * p,
               surfman_display_t * config,
               size_t size)
{
    int rv;
    glgfx_surface *surf = NULL;
    size_t i;

    info("glgfx_display, num config=%d", (int)size);
    
    /* initialise surfaces should that be necessary */
    for (i = 0; i < size; ++i) {
        surfman_display_t *d = &config[i];
        info("surface %p on monitor %p", d->psurface, d->monitor);
        if (d->psurface) {
            surf = (glgfx_surface*) d->psurface;
            if (!surf->initialised) {
                info("initialising surface");
                if (init_surface_resources(surf) < 0) {
                    error("FAILED to initialise surface!");
                    return SURFMAN_ERROR;
                }
            }

            /* perhaps begin animation sequence if we changed surface on i-th display */
            if (i < (size_t)g_num_dispcfgs) {
                glgfx_surface *prev_surf = (glgfx_surface*) g_dispcfg[i].psurface;
                if (prev_surf && prev_surf != surf) {
                    /* yes */
                    prev_surf->anim_next = surf;
                    prev_surf->anim_prev = NULL;
                    prev_surf->anim_active = 0;

                    surf->anim_prev = prev_surf;
                    surf->anim_next = NULL;
                    surf->anim_phase = 0;
                    surf->anim_active = 1;
                }
            }
        }
    }

    /* cache the display config for actual rendering done during refresh callback */
    memcpy( g_dispcfg, config, sizeof(surfman_display_t) * size );
    g_num_dispcfgs = size;

    return SURFMAN_SUCCESS;
}

static int
glgfx_get_monitors (surfman_plugin_t * p,
                    surfman_monitor_t * monitors,
                    size_t size)
{
    static surfman_monitor_t m = NULL;
    int i;
    info( "glgfx_get_monitors");
    update_monitors();
    for (i = 0; i < g_num_monitors && i < (int)size; ++i) {
        monitors[i] = &g_monitors[i];
    }
    info( "found %d monitors", g_num_monitors );
    return g_num_monitors;
}

static int
glgfx_set_monitor_modes (surfman_plugin_t * p,
                         surfman_monitor_t monitor,
                         surfman_monitor_mode_t * mode)
{
    info( "glgfx_set_monitor_modes");
    return SURFMAN_SUCCESS;
}

static int
glgfx_get_monitor_info_by_monitor (surfman_plugin_t * p,
                                   surfman_monitor_t monitor,
                                   surfman_monitor_info_t * info,
                                   unsigned int modes_count)
{
    int w,h,screen;
    xinemonitor_t *m;

    // info( "glgfx_get_monitor_info_by_monitor" );
    m = (xinemonitor_t*) monitor;
    w = m->w; h = m->h;

    info->modes[0].htimings[SURFMAN_TIMING_ACTIVE] = w;
    info->modes[0].htimings[SURFMAN_TIMING_SYNC_START] = w;
    info->modes[0].htimings[SURFMAN_TIMING_SYNC_END] = w;
    info->modes[0].htimings[SURFMAN_TIMING_TOTAL] = w;

    info->modes[0].vtimings[SURFMAN_TIMING_ACTIVE] = h;
    info->modes[0].vtimings[SURFMAN_TIMING_SYNC_START] = h;
    info->modes[0].vtimings[SURFMAN_TIMING_SYNC_END] = h;
    info->modes[0].vtimings[SURFMAN_TIMING_TOTAL] = h;

    info->prefered_mode = &info->modes[0];
    info->current_mode = &info->modes[0];
    info->mode_count = 1;
    return SURFMAN_SUCCESS;
}

static int
glgfx_get_monitor_edid_by_monitor (surfman_plugin_t * p,
                                   surfman_monitor_t monitor,
                                   surfman_monitor_edid_t * edid)
{
    info( "glgfx_get_edid_by_monitor");
    return SURFMAN_SUCCESS;
}

static surfman_psurface_t
glgfx_get_psurface_from_surface (surfman_plugin_t * p,
                                 surfman_surface_t * surfman_surface)
{
    glgfx_surface *surface = NULL;
    info("glgfx_get_psurface_from_surface");

    if (g_num_surfaces >= MAX_SURFACES) {
        error("too many surfaces");
        return NULL;
    }
    surface = calloc(1, sizeof(glgfx_surface));
    if (!surface) {
        return NULL;
    }
    surface->last_w = 0;
    surface->last_h = 0;
    surface->w = surfman_surface->width;
    surface->h = surfman_surface->height;
    surface->stride = surfman_surface->stride;
    surface->src = surfman_surface;
    surface->initialised = 0;
    surface->mapped_fb = NULL;
    surface->anim_next = NULL;
    surface->anim_prev = NULL;
    surface->anim_active = 0;
    surface->anim_phase = 0;
    info("allocated psurface %p", surface);

    map_fb( surfman_surface, surface );

    g_surfaces[g_num_surfaces++] = surface;

    return surface;
}

static void
glgfx_update_psurface (surfman_plugin_t *plugin,
                       surfman_psurface_t psurface,
                       surfman_surface_t *surface,
                       unsigned int flags)
{
    glgfx_surface *glsurf = (glgfx_surface*)psurface;
    info( "glgfx_update_psurface %p", surface);
    if (!psurface) {
        return;
    }
    glsurf->last_w = 0;
    glsurf->last_h = 0;
    glsurf->w = surface->width;
    glsurf->h = surface->height;
    glsurf->stride = surface->stride;
    glsurf->src = surface;
    if (flags & SURFMAN_UPDATE_PAGES) {
        surface_unmap( psurface );
        map_fb( surface, psurface );
    }
}

static void
glgfx_refresh_surface(struct surfman_plugin *plugin,
                      surfman_psurface_t psurface,
                      uint8_t *refresh_bitmap)
{
    glgfx_surface *dst = (glgfx_surface*) psurface;
    int i;

    if (!dst) {
        return;
    }

    glLoadIdentity();

    /* upload the surface which requires refresh into GPU */
    upload_to_gpu( dst->src, dst, refresh_bitmap );

    /* then render all currently visible surfaces */
    for (i = 0; i < g_num_dispcfgs; ++i) {
        surfman_display_t *d = &g_dispcfg[i];
        xinemonitor_t *m = (xinemonitor_t*) d->monitor;
        glgfx_surface *surf = (glgfx_surface*) d->psurface;

        if (surf && surf == psurface && surf->initialised) {
            /* translate onto monitor */
            glLoadIdentity();
            glTranslatef( m->xoff, m->yoff, 0 );
            render_animated( dst, m->w, m->h );
        }
    }
    /* deus ex machina */
    glXSwapBuffers( g_display, g_window );
}


static void
glgfx_free_psurface_pages(surfman_plugin_t * p,
                          surfman_psurface_t psurface)
{
    info( "%s", __func__);
}

static int
glgfx_get_pages_from_psurface (surfman_plugin_t * p,
                               surfman_psurface_t psurface,
                               uint64_t * pages)
{
    info( "glgfx_get_pages_from_psurface");
    return SURFMAN_ERROR;
}

static int
glgfx_copy_surface_on_psurface (surfman_plugin_t * p,
                                surfman_psurface_t psurface)
{
    info( "glgfx_copy_surface_on_psurface");
    return SURFMAN_ERROR;
}

static int
glgfx_copy_psurface_on_surface (surfman_plugin_t * p,
                                surfman_psurface_t psurface)
{
    info( "glgfx_copy_psurface_on_surface");
    return SURFMAN_ERROR;
}

static void
glgfx_free_psurface (surfman_plugin_t * plugin,
                     surfman_psurface_t psurface)
{
    int i;
    glgfx_surface *surf = (glgfx_surface*) psurface;
    info( "glgfx_free_psurface");
    if (surf) {
        free_surface_resources( surf );
        surface_unmap( surf );
        for (i = 0; i < g_num_surfaces; ++i) {
            if (g_surfaces[i] == surf) {
                info ("removing surface at %d", i);
                if (i != MAX_SURFACES-1 ) {
                    memcpy( &g_surfaces[i], &g_surfaces[i+1], (g_num_surfaces-i-1)*sizeof(glgfx_surface*) );
                }
                --g_num_surfaces;
                break;
            }
        }
        free(surf);
    }
}



surfman_plugin_t surfman_plugin = {
    .init = glgfx_init,
    .shutdown = glgfx_shutdown,
    .display = glgfx_display,
    .get_monitors = glgfx_get_monitors,
    .set_monitor_modes = glgfx_set_monitor_modes,
    .get_monitor_info = glgfx_get_monitor_info_by_monitor,
    .get_monitor_edid = glgfx_get_monitor_edid_by_monitor,
    .get_psurface_from_surface = glgfx_get_psurface_from_surface,
    .update_psurface = glgfx_update_psurface,
    .refresh_psurface = glgfx_refresh_surface,
    .get_pages_from_psurface = glgfx_get_pages_from_psurface,
    .free_psurface_pages = glgfx_free_psurface_pages,
    .copy_surface_on_psurface = glgfx_copy_surface_on_psurface,
    .copy_psurface_on_surface = glgfx_copy_psurface_on_surface,
    .free_psurface = glgfx_free_psurface,
    .options = {1, SURFMAN_FEATURE_NEED_REFRESH},
    .notify = SURFMAN_NOTIFY_NONE
};
