/*
 * Copyright (c) 2012, Citrix Systems
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __SURFMAN_H__
# define __SURFMAN_H__

# ifdef __cplusplus
extern "C"
{
# endif

    /*
    ** Utils and debug tools, new version that does not pisses me off. Those reasons are:
    ** - 1) Does not split a log in 2 lines
    ** - 2) Uses syslog log levels instead of LOG_ERR only (so you can actually make use of rsyslog).
    ** - 3) Makes surfman_message usage a bit more smooth.
    ** EC.
    */
    typedef enum {
        SURFMAN_FATAL = 0,
        SURFMAN_ERROR = 3,
        SURFMAN_WARNING = 4,
        SURFMAN_INFO = 6,
        SURFMAN_DEBUG = 7
    } surfman_loglvl;

    void surfman_message(surfman_loglvl level, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
    void surfman_vmessage(surfman_loglvl level, const char *fmt, va_list ap);
# define __log(level, level_string, fmt, ...) \
    surfman_message(level, "%s: " fmt "\n", level_string, ## __VA_ARGS__)

# ifdef NDEBUG
#  define SURFMAN_DEBUG_PRINT 0
# else
#  define SURFMAN_DEBUG_PRINT 1
#endif

# define surfman_debug(fmt, ...) \
    do { \
        if (SURFMAN_DEBUG_PRINT) \
            surfman_message(SURFMAN_DEBUG, "%s:%s:%d: " fmt, \
                __FILE__, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)

# define surfman_info(fmt, ...) __log(SURFMAN_INFO, "Info", fmt, ## __VA_ARGS__)
# define surfman_warning(fmt, ...) __log(SURFMAN_WARNING, "Warning", fmt, ## __VA_ARGS__)
# define surfman_error(fmt, ...) __log(SURFMAN_ERROR, "Error", fmt, ## __VA_ARGS__)
# define surfman_fatal(fmt, ...) __log(SURFMAN_FATAL, "Fatal", fmt, ## __VA_ARGS__)

    /*
    ** Version define
    */
# define SURFMAN_VERSION(major, minor, micro) ((major << 16) | (minor << 8) | (micro))
    typedef uint32_t surfman_version_t;

    /*
    ** Version helpers to print and retrieve a version part
    */
# define SURFMAN_VERSION_MAJOR(version) (((version) >> 16) & 0xff)
# define SURFMAN_VERSION_MINOR(version) (((version) >> 8) & 0xff)
# define SURFMAN_VERSION_MICRO(version) ((version) & 0xff)
# define SURFMAN_VERSION_FMT "%u.%u.%u"
# define SURFMAN_VERSION_ARGS(version) SURFMAN_VERSION_MAJOR(version), SURFMAN_VERSION_MINOR(version), SURFMAN_VERSION_MICRO(version)

    /*
    ** The plugin should create a variable surfman_plugin_version
    ** which its value is SURFMAN_API_VERSION.
    ** Surfman will retrieve this variable to know which API is currently
    ** used by the plugin.
    */
# define SURFMAN_API_VERSION SURFMAN_VERSION(2, 1, 2)

    /*
    ** Type used for storing Page Frame Numbers.
    */
    typedef unsigned long pfn_t;

    /*
    ** Return values
    **
    ** In case of a success, return SURFMAN_SUCCESS
    ** in case of an error, return SURFMAN_ERROR and set errno accordingly
    */
# define SURFMAN_SUCCESS 0
# define SURFMAN_ERROR   -1
# define SURFMAN_NOMEM   -2

    /*
    ** Plugin Abstract type.
    */

    /*
    ** surfman_monitor_t: Internal plugin representation of a physical monitor
    **
    ** This can contain any information that the plugin require to identify a
    ** physical monitor.
    **
    */
    typedef void * surfman_monitor_t;

    /*
    ** surfman_psurface_t: Internal plugin representation of a surface that
    **                     can be applied onto a physical monitor while switching.
    **
    */
    typedef void * surfman_psurface_t;

    /*
    ** Pair of monitor and psurface used to describe the content of a physical monitor
    **
    ** surfman_monitor_t is a plugin representation of a monitor.
    ** psurface is the internal representation in the plugin of a surface.
    **
    ** A psurface can be applied onto a surface through the plugin::display
    ** function.
    **
    ** In case the psurface field is NULL, the plugin is requested to stop
    ** drawing the previous registered psurface on this monitor, and optionnaly
    ** to blank the monitor.
    **
    ** When the psurface is applied onto a monitor some filter/effect can
    ** be applied (opacity, viewport).
    **
    ** The opacity filter could be used to create a fading effect.
    **
    ** The viewport filter could be used to create a moving and scalling effect.
    **
    ** See the example bellow on how this struct gets used.
    */
    typedef struct
    {
        /*
        ** monitor to apply the psurface on
        */
        surfman_monitor_t                           monitor;
        /*
        ** psurface to apply on the monitor
        */
        surfman_psurface_t                          psurface;

        struct
        {
            struct
            {
                /*
                ** opacity filter [0-255]: At 255 the psurface will be
                **                         applied completly.
                */
                uint8_t     opaque;
            }                                       opacity;
            struct
            {
                /*
                ** Viewport of the psurface in pixel: x, y, w, h
                **   This describes a restangle that will be applied on
                **   a destination rectangle on the monitor.
                **
                ** viewport of the monitor in pixel: x, y, w, h
                **   This describe the destination retangle on the monitor
                */
                unsigned int    psurface_x;
                unsigned int    psurface_y;
                unsigned int    psurface_height;
                unsigned int    psurface_width;
                unsigned int    monitor_x;
                unsigned int    monitor_y;
                unsigned int    monitor_height;
                unsigned int    monitor_width;
            }                                       viewport;
        }                                           effects;
    } surfman_display_t;

    enum surfman_surface_format
    {
        SURFMAN_FORMAT_UNKNOWN = 0x00,
        SURFMAN_FORMAT_BGR565 = 0x01,
        SURFMAN_FORMAT_BGRX8888 = 0x10,
        SURFMAN_FORMAT_RGBX8888 = 0x20
    };

    typedef struct
    {
        /*
        ** Information on the psurface
        */

        /*
        ** width in pixel of the psurface
        */
        unsigned int        width;
        /*
        ** height in pixel of the psurface
        */
        unsigned int        height;
    } surfman_psurface_info_t;

    typedef struct
    {
        /*
        ** Description of a surface
        */

        /*
        ** width in pixel of the surface
        */
        unsigned int                width;
        /*
        ** height in pixel of a surface
        */
        unsigned int                height;
        /*
        ** stride: size of a line in bytes
        */
        unsigned int                stride;
        /*
        ** format of the surface, cf enum surfman_surface_format
        */
        enum surfman_surface_format format;
        /*
        ** Number of pages in the array
        */
        unsigned int                page_count;
        /*
        ** Domain ID the pages in the array belong to
        */
        int                         pages_domid;
        /*
        ** Private data reserved for libsurfman use
        */
        uint64_t                    priv;
        /*
        ** Framebuffer offset
        */
        size_t                      offset;
        /*
        ** Array of machine address where this surface is stored
        */
        pfn_t                       mfns[0];
    } surfman_surface_t;

    typedef struct
    {
        /*
        ** bar id
        */
        uint8_t             id;

        /*
        ** Host address of the region, if any (passthrough case),
        **  (uint64_t) -1 otherwise
        */
        uint64_t            hostaddr;

        /*
        ** Address in the guest space where the region is mapped.
        */
        uint64_t            guestaddr;

        /*
        ** Length of the region, in bytes.
        */
        int                 len;
    } surfman_update_bar_t;

# define SURFMAN_TIMING_ACTIVE           0
# define SURFMAN_TIMING_SYNC_START       1
# define SURFMAN_TIMING_SYNC_END         2
# define SURFMAN_TIMING_TOTAL            3
    typedef struct
    {
        /*
        ** Pixel clock in hz of the physical monitor
        */
        unsigned int pix_clock_hz;

        /*
        ** Horizontal timings information of a physical monitor
        */
        unsigned int htimings[4];

        /*
        ** Vertical timings information of a physical monitor
        */
        unsigned int vtimings[4];
    } surfman_monitor_mode_t;

    typedef struct
    {
        /*
        ** Unique connector identifier where the monitor is connected
        ** to. It helps surfman to always bind a specific connector to
        ** the same virtual CRTC in the guest.
        */
        int                                connectorid;

        /*
        ** Pointer a of prefered mode of this physical monitor
        */
        surfman_monitor_mode_t             *prefered_mode;
        surfman_monitor_mode_t             *current_mode;

        /*
        ** Number of element in the modes array
        */
        unsigned int                        mode_count;

        /*
        ** Array of modes supported by this physical monitor
        */
        surfman_monitor_mode_t              modes[0];
    } surfman_monitor_info_t;

    typedef struct
    {
        /*
        ** EDID of a physical monitor
        */
        uint8_t        edid[128];
    } surfman_monitor_edid_t;

    typedef struct
    {
        unsigned int x;
        unsigned int y;
        unsigned int w;
        unsigned int h;
    } surfman_rect_t;

    typedef struct surfman_plugin
    {
        /*
        ** init: Initialize the plugin.
        **
        ** plugin: Self pointer to the plugin
        ** return: SURFMAN_SUCCESS or SURFMAN_ERROR
        */
        int                         (*init)(struct surfman_plugin *plugin);

        /*
        ** shutdown: Destroy the plugin
        */
        void                        (*shutdown)(struct surfman_plugin *plugin);

        /*
        ** display: Main switching function
        **
        ** config: Array of surfman_display_t, describes how to arrange
        **         the psurfaces on the monitors.
        ** size: Number of element in this array.
        ** return: SURFMAN_SUCCESS or SURFMAN_ERROR
        **
        ** This function takes an array of pairs (psurface, monitor).
        ** A psurface can be applied onto a physical monitor so this
        ** list description the configuration we wish to get on the
        ** physical monitors.
        **
        ** If a monitor is not specified in this list its configration
        ** shouldn't changed
        **
        ** A monitor could be specified more that once.
        **
        ** Some effect or filters can be applied for each pair.
        **
        ** Example:
        ** Let take two physical monitors 0 and 1, and 3 psurface a, b, c.
        **
        ** { { 0, a}, {1, b} }
        ** would display a on the monitor 0 and b on the monitor 1.
        */
        int                         (*display)(struct surfman_plugin *plugin,
                                               surfman_display_t *config,
                                               size_t size);

        /*
        ** get_monitors: Enumerate the physical monitors attached to the adapter
        **
        ** monitors: Array of monitors, needs to be filled up.
        ** size: Maximum size of monitor
        ** Return: Number of physical monitor, or SURFMAN_ERROR in case of an
        **         error.
        */
        int                         (*get_monitors)(struct surfman_plugin *plugin,
                                                    surfman_monitor_t *monitors,
                                                    size_t size);

        /*
        ** set_monitor_modes: Set a monitor to a modes
        **
        ** monitor: Physical monitor to set the mode on
        ** mode: Mode to set
        ** Return: SURFMAN_SUCCESS or SURFMAN_ERROR
        */
        int                         (*set_monitor_modes)(struct surfman_plugin *plugin,
                                                         surfman_monitor_t monitor,
                                                         surfman_monitor_mode_t *mode);

        /*
        ** get_monitor_info: get information on a monitor
        **
        ** monitor: Pointer to the monitor we want the info from.
        ** info: Pointer on info, needs to be filled up. cf to the definition of
        **       surfman_monitor_info_t for more information.
        ** Return: SURFMAN_SUCCESS or SURFMAN_ERROR
        */
        int                         (*get_monitor_info)(struct surfman_plugin *plugin,
                                                        surfman_monitor_t monitor,
                                                        surfman_monitor_info_t *info,
                                                        unsigned int modes_count);
        /*
        ** get_monitor_edid: get edid of a monitor
        **
        ** monitor: Pointer to the monitor we want to get the edid from.
        ** edid: EDID
        ** Return: SURFMAN_SUCCESS or SURFMAN_ERROR
        */
        int                         (*get_monitor_edid)(struct surfman_plugin *plugin,
                                                        surfman_monitor_t monitor,
                                                        surfman_monitor_edid_t *edid);

        /*
        ** get_psurface_from_surface: Get a plugin representation of a surface
        **
        ** surface: surface to be based on.
        ** Return: NULL in case of an error, valid psurface on success.
        **
        ** A psurface is the plugin internal representation of a surface.
        ** This psurface is used while switching. A psurface can be applied onto a
        ** monitor.
        */
        surfman_psurface_t          (*get_psurface_from_surface)(struct surfman_plugin *plugin,
                                                                 surfman_surface_t *surface);

        /*
        ** update_psurface_from_surface
        **
        ** TODO: Doc
        */
        #define SURFMAN_UPDATE_PAGES    (1 << 0)
        #define SURFMAN_UPDATE_FORMAT   (1 << 1)
        #define SURFMAN_UPDATE_OFFSET   (1 << 2)
        #define SURFMAN_UPDATE_ALL      0x7
        void                        (*update_psurface)(struct surfman_plugin *plugin,
                                                       surfman_psurface_t psurface,
                                                       surfman_surface_t *surface,
                                                       unsigned int flags);
        /*
        ** refresh_psurface
        **
        ** TODO: Doc
        */
        void                        (*refresh_psurface)(struct surfman_plugin *plugin,
                                                        surfman_psurface_t psurface,
                                                        uint8_t *refresh_bitmap);

        /*
        ** get_pages_from_psurface: get a list of pages to surfman where a surface
        **                          could be mapped to.
        **
        ** pages: Allocated array that needs to be filled up, size of the array would be the
        **        size surface mfns array assosiated with the psurface.
        ** Return: SURFMAN_ERROR or SURFMAN_SUCCESS
        **
        ** This function can only be called on psurface create from
        ** get_psurface_from_surface. If the plugin don't support the
        ** functionnality this function should return SURFMAN_ERROR.
        */
        int                         (*get_pages_from_psurface)(struct surfman_plugin *plugin,
                                                               surfman_psurface_t psurface,
                                                               pfn_t *pages);

        /*
        ** free_psurface_pages
        **
        ** TODO: Doc
        */

        void                        (*free_psurface_pages)(struct surfman_plugin *plugin,
                                                           surfman_psurface_t psurface);
        /*
        ** copy_surface_on_psurface: copy the content of a surface onto a psurface
        **
        ** psurface: destination of the copy, the source being the surface associated with it
        ** Return: SURFMAN_ERROR or SURFMAN_SUCCESS
        **
        ** If the size of the psurface is smaller than the size of the surface
        ** return SURFMAN_ERROR.
        */
        int                         (*copy_surface_on_psurface)(struct surfman_plugin *plugin,
                                                                surfman_psurface_t psurface);

        /*
        ** copy_psurface_on_surface: copy the content of a psurface onto a surface
        **
        ** psurface: source of the copy, the destination being the surface associated with it
        ** surface: destination of the copy
        ** Return: SURFMAN_ERROR or SURFMAN_SUCCESS
        **
        ** If the size of the psurface is smaller than the size of the surface
        ** return SURFMAN_ERROR.
        */
        int                         (*copy_psurface_on_surface)(struct surfman_plugin *plugin,
                                                                surfman_psurface_t psurface);

        /*
        ** free_psurface: Destroy and free all the resource assosiated with a
        **                psurface.
        **
        ** psurface: psurface to destroy
        **
        ** The psurface that can be destroy are the only ones that got created using
        ** get_psurface_from_surface. free_psurface on a psurface that comes from
        ** get_psurface_by_vmonitor should do nothing.
        **
        */
        void                        (*free_psurface)(struct surfman_plugin *plugin,
                                                     surfman_psurface_t psurface);

        /*
        ** pre_s3: Do all the necessary operations for the host prior to entering
        **         in S3 sleep state.
        */
        void                        (*pre_s3)(struct surfman_plugin *plugin);

        /*
        ** post_s3: Do all the necessary operations for the host subsequent to leaving
        **          from S3 sleep state.
        */
        void                        (*post_s3)(struct surfman_plugin *plugin);

        /*
        ** increase_brightness: Increase brightness
        */
        void                        (*increase_brightness)(struct surfman_plugin *plugin);

        /*
        ** decrease_brightness: Decrease_brightness
        */
        void                        (*decrease_brightness)(struct surfman_plugin *plugin);

        struct
        {
                /*
                ** stride alignement in byte
                */
                unsigned int        stride_align;

                /*
                ** Bit ORing of what the plugin supports.
                */
# define SURFMAN_FEATURE_NONE           0
# define SURFMAN_FEATURE_NEED_REFRESH   (1 << 1)
# define SURFMAN_FEATURE_PAGES_MAPPED   (1 << 2)
# define SURFMAN_FEATURE_FB_CACHING     (1 << 3)
                int                 features;
        }                           options;

        /*
        ** Notify a state change in the plugin
        **
        ** The value is read and cleared from surfman. Set for the plugin.
        */
# define SURFMAN_NOTIFY_NONE             0
# define SURFMAN_NOTIFY_MONITOR_RESCAN   (1 << 1)
        int                         notify;

        /*
         * restore_brightness : restore brightness
         * TODO: doc
         */
        void                        (*restore_brightness)(struct surfman_plugin *plugin);

        /*
         * dpms_on : power screen on
         */
        void                        (*dpms_on)(struct surfman_plugin *plugin);

        /*
         * dpms_off : power screen off
         */
        void                        (*dpms_off)(struct surfman_plugin *plugin);

    } surfman_plugin_t;

/* util.c */
extern void *xcalloc (size_t n, size_t s);
extern void *xmalloc (size_t s);
extern void *xrealloc (void *p, size_t s);
extern char *xstrdup (const char *s);
/* xc.c */
void xc_init(void);
int xc_domid_exists(int domid);
int xc_domid_getinfo(int domid, xc_dominfo_t *info);
void *xc_mmap_foreign(void *addr, size_t length, int prot, int domid, xen_pfn_t *pages);
int xc_hvm_get_dirty_vram(int domid, uint64_t base_pfn, size_t n, unsigned long *db);
int xc_hvm_pin_memory_cacheattr(int domid, uint64_t pfn_start, uint64_t pfn_end, uint32_t type);
/* configfile.c */
const char *config_get(const char *prefix, const char *key);
const char *config_dump(void);
int config_load_file(const char *filename);
/* surface.c */
void *surface_map(surfman_surface_t *surface);
xen_pfn_t surface_get_base_gfn(surfman_surface_t * surface);
void surface_unmap(surfman_surface_t *surface);

#ifdef __cplusplus
}
#endif

#endif /* __SURFMAN_H__ */
