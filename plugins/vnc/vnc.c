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

#include "project.h"
#include "list.h"

const int X11_to_input[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_SPACE, 0 /* XK_exclam */, 0 /* XK_quotedbl */, 0 /* XK_numbersign */, KEY_DOLLAR, 0 /* XK_percent */, 0 /* XK_ampersand */, KEY_APOSTROPHE, 0 /* XK_parenleft */, 0 /* XK_parenright */, KEY_KPASTERISK /* XK_asterisk */, KEY_KPPLUS /* XK_plus */, KEY_COMMA, KEY_MINUS, KEY_DOT, KEY_SLASH, KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, 0 /* XK_colon */, KEY_SEMICOLON, 0 /* XK_less */, KEY_EQUAL, 0 /* XK_greater */, KEY_QUESTION, 0 /* XK_at */, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_LEFTBRACE /* XK_bracketleft */, KEY_BACKSLASH, KEY_RIGHTBRACE /* XK_bracketright */, 0 /* XK_asciicircum */, 0 /* XK_underscore */, KEY_GRAVE, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_LEFTBRACE /* XK_braceleft */, 0 /* XK_bar */, KEY_RIGHTBRACE /* XK_braceright */};

const int X11ff_to_input[] = {0, 0, 0, 0, 0, 0, 0, 0, KEY_BACKSPACE, KEY_TAB, 0, 0, 0, KEY_ENTER, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_LEFT, KEY_UP, KEY_RIGHT, KEY_DOWN};

typedef struct
{
    surfman_surface_t *surface;
    uint8_t *fb;
} vnc_surface;

typedef struct
{
    int fd;
    struct sockaddr_in addr;
    int addr_len;
} vnc_socket;

typedef struct
{
    int fd;
    struct event ev;
} vnc_client_socket;

struct pending_socket_s
{
    LIST_ENTRY(struct pending_socket_s) next;

    vnc_client_socket *socket;
};

LIST_HEAD (, struct pending_socket_s) pending_sockets;

static int g_monitor = 1;

static struct event vnc_socket_event;

static int
vnc_init (surfman_plugin_t * p)
{
    info("vnc: init");

    LIST_HEAD_INIT(&pending_sockets);

    return SURFMAN_SUCCESS;
}

static void
vnc_shutdown (surfman_plugin_t * p)
{
    info("vnc: shutdown");
}

static int
vnc_display (surfman_plugin_t * p,
                surfman_display_t * config,
                size_t size)
{
    info("vnc: display");

    return SURFMAN_SUCCESS;
}

static int
vnc_get_monitors (surfman_plugin_t * p,
                     surfman_monitor_t * monitors,
                     size_t size)
{
    info("vnc: get_monitors");
    monitors[0] = &g_monitor;

    return 1;
}

static int
vnc_set_monitor_modes (surfman_plugin_t * p,
                          surfman_monitor_t monitor,
                          surfman_monitor_mode_t * mode)
{
    info("vnc: set_monitor_modes");

    return SURFMAN_SUCCESS;
}

static int
vnc_get_monitor_info_by_monitor (surfman_plugin_t * p,
                                    surfman_monitor_t monitor,
                                    surfman_monitor_info_t * info,
                                    unsigned int modes_count)
{
    int w, h;

    w = 1280;
    h = 1024;

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
vnc_get_monitor_edid_by_monitor (surfman_plugin_t * p,
                                    surfman_monitor_t monitor,
                                    surfman_monitor_edid_t * edid)
{
    info("vnc: edid");

    return SURFMAN_SUCCESS;
}

static void
vnc_write_u8(int fd, uint8_t value)
{
    write(fd, &value, 1);
}

static void
vnc_write_u16(int fd, uint16_t value)
{
    uint8_t buf[2];

    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;

    write(fd, buf, 2);
}

static void
vnc_write_u32(int fd, uint32_t value)
{
    uint8_t buf[4];

    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >>  8) & 0xFF;
    buf[3] = value & 0xFF;

    write(fd, buf, 4);
}

static uint32_t
vnc_read_u16(int fd)
{
    uint16_t value;

    read(fd, &value, 2);

    return (((value << 8) & 0xFF00) +
	    ((value >> 8) & 0x00FF));
}

static uint32_t
vnc_read_u32(int fd)
{
    uint32_t value;

    read(fd, &value, 4);

    return (((value << 24) & 0xFF000000) +
	    ((value << 8)  & 0x00FF0000) +
	    ((value >> 8)  & 0x0000FF00) +
	    ((value >> 24) & 0x000000FF));
}

static void
vnc_write_s32(int fd, int32_t value)
{
    vnc_write_u32(fd, *(uint32_t *)&value);
}

static void
pixel_format_message (int fd)
{
    char pad[3] = { 0, 0, 0 };

    vnc_write_u8(fd, 32);       /* bits-per-pixel  */
    vnc_write_u8(fd, 24);       /* depth           */

    vnc_write_u8(fd, 0);        /* big-endian-flag */
    vnc_write_u8(fd, 1);        /* true-color-flag */
    vnc_write_u16(fd, 255);     /* red-max         */
    vnc_write_u16(fd, 255);     /* green-max       */
    vnc_write_u16(fd, 255);     /* blue-max        */
    vnc_write_u8(fd, 0);        /* red-shift       */
    vnc_write_u8(fd, 0);        /* green-shift     */
    vnc_write_u8(fd, 0);        /* blue-shift      */
    write(fd, pad, 3);          /* padding         */
}

static int
protocol_client_init(int fd)
{
    size_t l;
    char domain_name[] = "fish";

    vnc_write_u16(fd, 1280);
    vnc_write_u16(fd, 1024);

    pixel_format_message(fd);

    l = strlen(domain_name); 
    vnc_write_u32(fd, l);        
    write(fd, domain_name, l);

    return 0;
}

static void
vnc_send_input_event(struct input_event* e)
{
    static int input_fd = -1;

    if (input_fd < 0)
    {
	/* Initialize communication with input_server */
	struct sockaddr_un remote;
	input_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (input_fd < 0)
	{
	    info("Failed to create socket to input_server");
	    return;
	}
	memset(&remote, 0, sizeof (remote));
	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, "/tmp/input.socket"); /* FIXME: put
						       * me in a
						       * #define */
	if (connect(input_fd, (struct sockaddr *)&remote, SUN_LEN(&remote)) != 0)
	{
	    info("Failed to connect to input_server");
	    input_fd = -1;
	    return;
	}
    }

    if (input_fd >= 0)
    {
	char to_send[sizeof(struct input_event) + 1];

	*to_send = 'Q'; /* input_server code for input event */
	memcpy(to_send + 1, e, sizeof(struct input_event));
	write(input_fd, to_send, sizeof(struct input_event) + 1);
    }
}

static void
vnc_process_key_event(int fd)
{
    uint8_t down_flag;
    uint32_t key;
    struct input_event e;

    /* Read input event */
    read(fd, &down_flag, 1); /* Down flag */
    read(fd, &key, 2);       /* Padding */

    key = vnc_read_u32(fd);

    if (key < sizeof(X11_to_input) && X11_to_input[key] != 0)
      {
	/* Send it to input_server */
	e.type = EV_KEY;
	e.code = X11_to_input[key];
	e.value = !!down_flag;
	vnc_send_input_event(&e);
      }
    else
      {
	if (key - 0xff00 < sizeof(X11ff_to_input) && X11ff_to_input[key - 0xff00] != 0)
	  {
	    /* Send it to input_server */
	    e.type = EV_KEY;
	    e.code = X11ff_to_input[key - 0xff00];
	    e.value = !!down_flag;
	    vnc_send_input_event(&e);
	  }
      }
}

static void
vnc_process_mouse_event(int fd)
{
    uint8_t buttons;
    static uint16_t prev_x = 0;
    static uint16_t prev_y = 0;
    uint16_t x;
    uint16_t y;
    struct input_event e;
    char to_send[sizeof(struct input_event) + 1];

    read(fd, &buttons, 1); /* Button mask */
    x = vnc_read_u16(fd);  /* x-position (abs) */
    y = vnc_read_u16(fd);  /* y-position (abs) */

    /* Send it to input_server */
    /* e.type = EV_ABS; */
    /* e.code = ABS_X; */
    /* e.value = x * 25.6; /\* 0x7FFF / 1280 *\/ */
    e.type = EV_REL;
    e.code = REL_X;
    e.value = x - prev_x;
    vnc_send_input_event(&e);
    /* e.code = ABS_Y; */
    /* e.value = y * 32; /\* 0x7FFF / 1024 *\/ */
    e.code = REL_Y;
    e.value = y - prev_y;
    vnc_send_input_event(&e);
    if (buttons & 0x01)
    {
	/* Left click */
	e.type = EV_KEY;
	e.code = BTN_LEFT;
	e.value = 1;
	vnc_send_input_event(&e);
	e.value = 0;
	vnc_send_input_event(&e);
    }
    if (buttons & 0x04)
    {
	/* Right click */
	e.type = EV_KEY;
	e.code = BTN_RIGHT;
	e.value = 1;
	vnc_send_input_event(&e);
	e.value = 0;
	vnc_send_input_event(&e);
    }
    e.type = EV_SYN;
    e.code = SYN_REPORT;
    vnc_send_input_event(&e);

    prev_x = x;
    prev_y = y;
}

static void
vnc_socket_client_handler(int fd, short event, void *opaque)
{
    vnc_client_socket *client = (vnc_client_socket*)opaque;
    uint8_t val;
    struct pending_socket_s *pending = NULL;
    char buf[1024];
    int rc;

    if (event & EV_READ)
    {
        rc = read(fd, &val, 1);

	if (rc == 0)
	{
	    /* The client left, closing the socket. FIXME: If the socket is currently pending for a refresh, it needs to be removed from the list */
            /* Peer disconnected */
	    event_del(&client->ev);
	    close(client->fd);
	    free(client);
            return;
	}

        if (rc < 0)
            return;

        switch (val)
        {
        case 0: /* Set Pixel Format */
            read(fd, buf, 19); /* Eat-and-ignore message */
            break;
        case 2: /* Set Encodings */
            read(fd, buf, 1); /* Padding */
            {
                uint16_t i;
                i = read(fd, &i, 2);
                read(fd, buf, i * 4); /* Eat-and-ignore message */
            }
            break;
        case 3: /* Framebuffer Update Request */
            read(fd, buf, 9); /* Eat-and-ignore message */
            pending = calloc(1, sizeof(struct pending_socket_s));
            pending->socket = client;
            LIST_INSERT_HEAD(&pending_sockets, pending, next); /* Should
                                                                * be
                                                                * TAIL!*/
            break;
        case 4: /* Key Event */
            vnc_process_key_event(fd);
            break;
        case 5: /* Pointer Event */
            vnc_process_mouse_event(fd);
            break;
        case 6: /* Client Cut Text */
            read(fd, buf, 3); /* Padding */
            {
                uint32_t i;
                i = read(fd, &i, 4);
                read(fd, buf, i); /* Eat-and-ignore message */
            }
            break;
        default:
          break;
          info("Received invalid command from client : %d", val);
        }
    }
}

static void
vnc_socket_handler(int sockfd, short event, void *opaque)
{
    vnc_socket* socket = (vnc_socket*) opaque;
    int fd;
    int i = 0;
    char buf[13];
    vnc_client_socket* client;
    struct pending_socket_s *pending;

    if (socket == NULL)
    {
	info("vnc: socket NULL!");
	return;
    }

    fd = accept(socket->fd, (struct sockaddr*) &socket->addr, (socklen_t*) &socket->addr_len);

    if (fd <= 0)
      {
	info("vnc: fd <= 0!!");
	return;
      }

    strncpy(buf, "RFB 003.008\n", 13);
    write(fd, buf, 12); /* Send our version */
    read(fd, buf, 12); /* Receive client version */
    buf[12] = '\0';
    if (strncmp(buf, "RFB 003.008\n", 12))
    {
	close(fd);
	info("Incompatible client version : %s", buf);
	return;
    }
    vnc_write_u8(fd, 1); /* Num auth */
    vnc_write_u8(fd, 1); /* No auth */
    read(fd, buf, 1);    /* Read client auth, should be 1 (no auth) */
    if (*buf != 1)
    {
	close(fd);
	info("Client disagreed on no auth, it wants %i", *buf);
	return;
    }
    else
	vnc_write_u32(fd, 0); /* Accept auth completion */

    info("vnc: auth done");

    protocol_client_init(fd);

    client = malloc(sizeof(vnc_client_socket));
    client->fd = fd;
    event_set (&client->ev, fd, EV_READ | EV_PERSIST,
               vnc_socket_client_handler, client);
    event_add (&client->ev, NULL);

    /* First display */
    pending = calloc(1, sizeof(struct pending_socket_s));
    pending->socket = client;
    LIST_INSERT_HEAD(&pending_sockets, pending, next); /* Should
							* be
							* TAIL!*/
}

static uint8_t*
map_surfman_surface(surfman_surface_t *src)
{
    xen_pfn_t *fns = NULL;
    uint8_t *mapped = NULL;
    unsigned int i = 0;

    fns = malloc( src->page_count*sizeof(xen_pfn_t) );
    if (!fns) {
        error("failed to malloc mfns");
        goto out;
    }
    for (i = 0; i < src->page_count; ++i) {
        if (src->guest_base_addr)
            fns[i] = (xen_pfn_t)((src->guest_base_addr >> XC_PAGE_SHIFT) + i);
        else
            fns[i] = (xen_pfn_t) src->mfns[i];
    }
    if (src->guest_base_addr) {
        mapped = xc_map_foreign_batch_cacheattr(
                xch, src->pages_domid,
                PROT_READ | PROT_WRITE, fns, src->page_count,
                XEN_DOMCTL_MEM_CACHEATTR_WB);
    } else {
        mapped = xc_map_foreign_pages( xch, src->pages_domid, PROT_READ | PROT_WRITE, fns, src->page_count );
    }
out:
    if (fns) {
        free(fns);
    }
    return mapped;
}

static void
unmap_surfman_surface(uint8_t *mapped, surfman_surface_t *src)
{
    if ( munmap(mapped, src->page_count * XC_PAGE_SIZE) < 0 ) {
        error("failed to unmap framebuffer pages");
    }
}

static surfman_psurface_t
vnc_get_psurface_from_surface (surfman_plugin_t * p,
                                  surfman_surface_t * surface)
{
    vnc_surface *my_surface = NULL;
    static vnc_socket* vnc_socket = NULL;
    int fd;
    int val = 1;

    info("vnc: get_psurface");

    if (surface == NULL)
      {
	info("surface NULL ?!?!?!");
	return NULL;
      }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
	perror("socket");
	return NULL;
    }

    /* Setup the socket at first call */
    if (vnc_socket == NULL)
      {
	struct sockaddr_in addr;

	addr.sin_addr.s_addr=INADDR_ANY;
	addr.sin_port=htons(5900);
	addr.sin_family=AF_INET;

	/* Try not to block the port if surfman cashes */
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
	  info("setsockopt error");

	if (bind(fd, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) != 0)
	  {
	    perror("bind");
	    return NULL;
	  }

	if (listen(fd, 42) != 0)
	  {
	    perror("listen");
	    return NULL;
	  }

	vnc_socket = calloc(1, sizeof(*vnc_socket));
	vnc_socket->fd = fd;
	vnc_socket->addr = addr;
	vnc_socket->addr_len = sizeof(struct sockaddr_in);


        event_set (&vnc_socket_event, fd, EV_READ | EV_PERSIST,
                   vnc_socket_handler, vnc_socket);
        event_add (&vnc_socket_event, NULL);

      }

    my_surface = calloc(1, sizeof(vnc_surface));
    my_surface->surface = surface;
    my_surface->fb = map_surfman_surface(surface);

    return my_surface;
}

static void
vnc_refresh_psurface(surfman_plugin_t *p,
                        surfman_psurface_t psurface,
                        uint8_t *refresh_bitmap)

{
    unsigned int i, j, n;
    int fd;
    vnc_surface *my_surface = (vnc_surface*) psurface;
    unsigned int w, h, s;
    surfman_surface_t *surf;
    uint8_t *fb;
    struct pending_socket_s *pend, *next_pend;

    surf = my_surface->surface;
    fb = my_surface->fb;
    w = surf->width;
    h = surf->height;
    s = surf->stride;

    LIST_FOREACH_SAFE(pend, next_pend, &pending_sockets, next)
    {
	fd = pend->socket->fd;
	if (fd > 0)
	{
	    vnc_write_u8(fd, 0);  /* Message ID */
	    vnc_write_u8(fd, 0);
	    vnc_write_u16(fd, 1); /* Number of rects */

	    vnc_write_u16(fd, 0); /* x */
	    vnc_write_u16(fd, 0); /* y */
	    vnc_write_u16(fd, w); /* w */
	    vnc_write_u16(fd, h); /* h */

	    vnc_write_s32(fd, 0);
	    for (j = 0; j < h; ++j)
	    	write(fd, fb + j * s, w * 4);
	}
	LIST_REMOVE(pend, next);
	free(pend);
    }
}

static void
vnc_update_psurface (surfman_plugin_t *plugin,
                        surfman_psurface_t psurface,
                        surfman_surface_t *surface,
                        unsigned int flags)
{
    vnc_surface *my_surface = (vnc_surface*) psurface;

    info("vnc: update_psurface");
    if (flags & SURFMAN_UPDATE_PAGES)
    {
	unmap_surfman_surface(my_surface->fb, my_surface->surface);
	my_surface->fb = map_surfman_surface(surface);
    }
    my_surface->surface = surface;
}

static int
vnc_get_pages_from_psurface (surfman_plugin_t * p,
                                surfman_psurface_t psurface,
                                uint64_t * pages)
{
    info("vnc: get_pages_from_psurface");

    return SURFMAN_ERROR;
}

static void
vnc_free_psurface_pages (surfman_plugin_t *p,
			 surfman_psurface_t psurface)
{
    info("vnc: free_psurface_pages");
}

static int
vnc_copy_surface_on_psurface (surfman_plugin_t * p,
                                 surfman_psurface_t psurface)
{
    info("vnc: copy s to p");

    return SURFMAN_ERROR;
}

static int
vnc_copy_psurface_on_surface (surfman_plugin_t * p,
                                 surfman_psurface_t psurface)
{
    info("vnc: copy p to s");

    return SURFMAN_ERROR;
}

static void
vnc_free_psurface (surfman_plugin_t * plugin,
                      surfman_psurface_t psurface)
{
    vnc_surface *my_surface = (vnc_surface*) psurface;

    info("vnc: free p");
    free(my_surface);
}

static void
vnc_pre_s3 (surfman_plugin_t * p)
{
    info("vnc: pre_s3");
}

static void
vnc_post_s3 (surfman_plugin_t * p)
{
    info("vnc: post_s3");
}

static void
vnc_increase_brightness (surfman_plugin_t * p)
{
}

static void
vnc_decrease_brightness (surfman_plugin_t * p)
{
}

static surfman_vgpu_t *
vnc_new_vgpu (surfman_plugin_t * p,
                 surfman_vgpu_info_t * info)
{
    info("vnc: new_vgpu");

    return NULL;
}

static void
vnc_free_vgpu (surfman_plugin_t * p,
                  surfman_vgpu_t * vgpu)
{
    info("vnc: free_vgpu");
}

surfman_plugin_t surfman_plugin = {
  .init = vnc_init,
  .shutdown = vnc_shutdown,
  .display = vnc_display,
  .new_vgpu = vnc_new_vgpu,
  .free_vgpu = vnc_free_vgpu,
  .get_monitors = vnc_get_monitors,
  .set_monitor_modes = vnc_set_monitor_modes,
  .get_monitor_info = vnc_get_monitor_info_by_monitor,
  .get_monitor_edid = vnc_get_monitor_edid_by_monitor,
  .get_psurface_from_surface = vnc_get_psurface_from_surface,
  .update_psurface = vnc_update_psurface,
  .refresh_psurface = vnc_refresh_psurface,
  .get_pages_from_psurface = vnc_get_pages_from_psurface,
  .free_psurface_pages = vnc_free_psurface_pages,
  .copy_surface_on_psurface = vnc_copy_surface_on_psurface,
  .copy_psurface_on_surface = vnc_copy_psurface_on_surface,
  .free_psurface = vnc_free_psurface,
  .pre_s3 = vnc_pre_s3,
  .post_s3 = vnc_post_s3,
  .increase_brightness = vnc_increase_brightness,
  .decrease_brightness = vnc_decrease_brightness,
  .options = { 1, SURFMAN_FEATURE_NEED_REFRESH },
  .notify = SURFMAN_NOTIFY_NONE
};
