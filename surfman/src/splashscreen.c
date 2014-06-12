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
#include <png.h>
#include <ctype.h>
#include <event.h>
#include <sys/time.h>
#include <math.h>
#include "project.h"
#include "fbtap.h"          // the kernel module header
#include "splashfont.h"

#define swap(a, b) do { __typeof__(a) tmp;  tmp = a; a = b; b = tmp; } while(0)

static struct domain *splashdomain = NULL;
static struct device *splashdev = NULL;
static struct event *spinnerev = NULL;
static uint8_t *spinnerfb = NULL;
static struct timeval spinnerto = { .tv_sec = 0, .tv_usec = 166666 };

static void png_error_fn(png_structp png_ptr,
                         png_const_charp error_msg)
{
  surfman_error("%s", error_msg);
}

static void png_warning_fn(png_structp png_ptr,
                           png_const_charp warning_msg)
{
  surfman_warning("%s", warning_msg);
}

int
get_splash_dims (uint32_t *linesize, uint32_t *screenwidth,
  uint32_t *screenheight, uint32_t *screensize, unsigned int *Bpp, 
  unsigned int *format)
{
  surfman_surface_t *surface;

  if (!splashdomain || !(splashdomain->devices.e_first) || !splashdev)
    {
      surfman_error ("there seems to be no splash screen");
      return -1;
    }

  // monitor 0
  if (!(surface = splashdev->ops->get_surface (splashdev, 0)->surface)) 
    {
      surfman_error ("can't get surface?");
      return -1;
    }

  if (SURFMAN_FORMAT_BGRX8888 != surface->format)
    {
      surfman_error ("format is not what it used to be");
      return -1;
    }

  if (screenwidth) *screenwidth = surface->width;
  if (screenheight) *screenheight = surface->height;
  if (linesize) *linesize = surface->stride;
  if (screensize) *screensize = (surface->stride) * (surface->height);
  if (Bpp) *Bpp = 4;
  if (format) *format = surface->format;

  return 0;
}

int
mmap_splash (uint8_t **fb, uint32_t *linesize, uint32_t *screenwidth,
  uint32_t *screenheight, unsigned int *Bpp)
{
  uint32_t mylinesize, myscreenwidth, myscreenheight, myscreensize;
  unsigned int myformat, myBpp;

  if (get_splash_dims (&mylinesize, &myscreenwidth, &myscreenheight, 
                       &myscreensize, &myBpp, &myformat))
    {
      surfman_error ("getting info about splash surface failed.");
      goto mmap_splash_fail;
    }

  if (MAP_FAILED == (*fb = mmap (0, myscreensize, PROT_WRITE | PROT_READ, 
                MAP_SHARED, fbtap_device_fd ((struct device *) splashdev), 0)))
    {
      surfman_error ("mmap of fbtap failed: %s", strerror (errno));
      goto mmap_splash_fail;
    }

  if (linesize) *linesize = mylinesize;
  if (screenwidth) *screenwidth = myscreenwidth;
  if (screenheight) *screenheight = myscreenheight;
  if (Bpp) *Bpp = myBpp;

  return 0;

mmap_splash_fail:
  *fb = NULL;
  return -1;
}

inline static void
set_BGRX_pixel (uint32_t x, uint32_t y, uint32_t c, uint32_t linesize, uint32_t xres, uint32_t yres, unsigned int thickness)
{
  const unsigned int Bpp = 4;
  uint8_t *buf;
  uint8_t *colourp = (uint8_t *) &c;
  uint32_t xoff, yoff;

  for (xoff = x - thickness; xoff <= x + thickness; xoff++)
    {
      for (yoff = y - thickness; yoff <= y + thickness; yoff++)
        {  
          if ((xoff < xres) && (yoff < yres))
            {
              buf = spinnerfb + linesize * yoff + Bpp * xoff;
              buf[3] = colourp[0];
              buf[2] = colourp[1];
              buf[1] = colourp[2];
              buf[0] = colourp[3];
            }
        }
    }
}

inline static uint32_t
intensity (uint32_t c, float in)
{
  uint8_t *cptr = (uint8_t *) &c;
  unsigned int n;

  for (n = 0; n < 4; n++)
    {
      cptr[n] = cptr[n] & (unsigned int) (0xff * in);
    }

  return c;
}

/* Xiaolin Wu's line algo, copied from wikipedia, 
   doesn't make a lot of sense to use as it is slower than bresenham and we 
   aren't using the antialiasing...
   further slowed down by adding dumb thickness approach - multiple writes */
static void
draw_line (uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t c, 
           uint8_t *fb, uint32_t linesize, uint32_t xres, uint32_t yres, 
           unsigned int thickness)
{
  uint32_t xpxl1, ypxl1, xpxl2, ypxl2, x, y;
  float dx, dy, gradient, intery, xgap, xstart, ystart, xend, yend;
  int xyswap = 0;
  int t;

  dx = 0.0 + x2 - x1;
  dy = 0.0 + y2 - y1;
  if (fabsf (dx) < 0.0000001) dx = copysign (0.0000001, dx);
  if (fabsf (dy) < 0.0000001) dy = copysign (0.0000001, dy);

  if (fabsf (dx) < fabsf (dy))
    {
      swap (x1, y1);
      swap (x2, y2);
      swap (dx, dy);
      xyswap = 1;
    }

   if (x2 < x1)
    {
      swap (x1, x2);
      swap (y1, y2);
    }

  gradient = 1.0 * dy / dx;

  xstart = x1;
  if (xstart < 0.0) xstart = 0.0;
  if (xstart > xres) xstart = xres - 1.0;

  ystart = y1;
  if (ystart < 0.0) ystart = 0.0;
  if (ystart >= yres) ystart = yres - 1.0;

  xgap = 1.0 - x1 + truncf (x1);
  xpxl1 = xstart;
  ypxl1 = truncf (ystart);

  intery = ystart + gradient;

  xend = x2;
  if (xend < 0.0) xend = 0.0;
  if (xend >= xres) xend = xres - 1.0;

  yend = y2 + gradient * (xend - x2);
  if (yend < 0.0) yend = 0.0;
  if (yend >= yres) yend = yres - 1.0;

  xgap = x2 + 0.5 - truncf (x2 + 0.5);
  xpxl2 = xend;
  ypxl2 = truncf (yend);

  if (xyswap)
    {
      set_BGRX_pixel (ypxl1, xpxl1, c, linesize, xres, yres, thickness);
      set_BGRX_pixel (ypxl2, xpxl2, c, linesize, xres, yres, thickness);
    }
  else
    {
      set_BGRX_pixel (xpxl1, ypxl1, c, linesize, xres, yres, thickness);
      set_BGRX_pixel (xpxl2, ypxl2, c, linesize, xres, yres, thickness);
    }

  for (x = xpxl1 + 1; x <= xpxl2 - 1; x += thickness)
    {
      if (xyswap)
        {
          set_BGRX_pixel (truncf (intery), x, c, linesize, xres, yres, thickness);
        }
      else
        {
          set_BGRX_pixel (x, truncf (intery), c, linesize, xres, yres, thickness);
        }

      intery += gradient * thickness;
    }
}

static void
line_coords (uint32_t state, const unsigned int numstates,
             uint32_t xc, uint32_t yc, uint32_t r1, uint32_t r2,
             uint32_t *x1, uint32_t *y1, uint32_t *x2, uint32_t *y2)
{
  float alpha = 2.0 * M_PI * state / numstates;
  *x2 = r2 * cos (alpha) + xc;
  *x1 = r1 * cos (alpha) + xc;
  *y2 = r2 * sin (alpha) + yc;
  *y1 = r1 * sin (alpha) + yc;
}

static void
display_spinner (int r, short s, void *t)
{
  const unsigned int numlines = 8, thickness = 1;
  uint32_t linesize, screenheight, screenwidth, Bpp;
  uint32_t xpos, ypos, r1, r2; // centre, inner / outer radius. r2 includes r1
  unsigned int n, l, c;
  const uint32_t colour_high = 0xffffff00, colour_low = 0x9f9f9f00, invalid = 0xffffffff;
  static uint32_t lastx1 = invalid, lasty1 = invalid, lastx2 = invalid, lasty2 = invalid;
  uint32_t newx1, newx2, newy1, newy2;
  static uint32_t state = invalid;

  (void) r;
  (void) s;
  (void) t;

  evtimer_add (spinnerev, &spinnerto);

  if (!splashdev || !spinnerfb || !(SPLASH_DOM_ID == domain_get_visible()))
    {
      surfman_warning ("was told to spin, but !splash device. unregistering...");
      unregister_spinner ();
      return;
    }

  if (get_splash_dims (&linesize, &screenwidth, &screenheight, NULL, &Bpp, NULL))
    {
      surfman_error ("sth went wrong retrieving splash surface dimensions");
      return;
    }

  xpos = screenwidth / 2;
  ypos = screenheight * 3.2/5;
  r1 = 8;
  r2 = 18;

  if (invalid == state)
    {
      // first time, draw all lines dark
      for (state = 0; state < numlines; state++)
        {
          line_coords (state, numlines, xpos, ypos, r1, r2, &newx1, &newy1, &newx2, &newy2);
          draw_line (newx1, newy1, newx2, newy2, colour_low, spinnerfb, linesize, screenwidth, screenheight, thickness);
        }

      state = 0;
    }
  else
    {
      // only overwrite last (bright) line
      draw_line (lastx1, lasty1, lastx2, lasty2, colour_low, spinnerfb, linesize, screenwidth, screenheight, thickness);
    }

  line_coords (state, numlines, xpos, ypos, r1, r2, &newx1, &newy1, &newx2, &newy2);
  draw_line (newx1, newy1, newx2, newy2, colour_high, spinnerfb, linesize, screenwidth, screenheight, thickness);

  lastx1 = newx1;
  lasty1 = newy1;
  lastx2 = newx2;
  lasty2 = newy2;

  state = (state + 1) % numlines;
}
 
void
register_spinner ()
{
  if (!(spinnerev = calloc (1, sizeof (*spinnerev))))
    {
      surfman_error ("failed to alloc mem for the splash spinner event.");
      return;
    }
  
  if (mmap_splash (&spinnerfb, NULL, NULL, NULL, NULL))
    {
      surfman_error ("mmap_splash() failed - won't spin");
      return;
    }

  evtimer_set (spinnerev, &display_spinner, NULL);
  evtimer_add (spinnerev, &spinnerto);
}

void
unregister_spinner ()
{
  uint32_t screensize;

  if (get_splash_dims (NULL, NULL, NULL, &screensize, NULL, NULL))
    {
      surfman_error ("something failed when trying to get sreen dimension");
    }
  else
    {
      munmap (spinnerfb, screensize);
    }

  spinnerfb = NULL;
  evtimer_del (spinnerev);
}

static int
open_png (const char *fname, struct fbdim *imgdims,
  png_infop *info_ptr, png_structp *png_ptr, FILE **fil)
{
  const unsigned int num_magic_bytes_check = 8;
  unsigned char buf[num_magic_bytes_check];
  unsigned int type;

  *png_ptr = NULL;
  *info_ptr = NULL;

  if (!(*fil = fopen (fname, "rb")))
    {
      surfman_error ("failed to open() %s", fname);
      return -1;
    }

  if (1 != fread (buf, num_magic_bytes_check, 1, *fil))
    {
      surfman_error ("could not read %dB at beginning of %s", 
             num_magic_bytes_check, fname);

      return -1;
    }

  if (png_sig_cmp(buf, 0, num_magic_bytes_check))
    {
      surfman_error ("this is not a PNG file: %s", fname);
      return -1;
    }

  if (!(*png_ptr = png_create_read_struct (
                                     PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
    {
      surfman_error ("png_create_read_struct failed");
      return -1;
    }

  if (!(*info_ptr = png_create_info_struct (*png_ptr)))
    {
      surfman_error ("png_create_info_struct 1 failed");
      goto open_png_fail;
    }

  if (setjmp (png_jmpbuf (*png_ptr)))
    {
      surfman_warning ("PNG longjmp called");
      goto open_png_fail;
    }

  png_set_error_fn (*png_ptr, NULL, png_error_fn, png_warning_fn);

  png_init_io (*png_ptr, *fil);
  png_set_sig_bytes (*png_ptr, num_magic_bytes_check);
  png_read_info (*png_ptr, *info_ptr);
  imgdims->xres = png_get_image_width (*png_ptr, *info_ptr);
  imgdims->yres = png_get_image_height (*png_ptr, *info_ptr);
  imgdims->bpp = png_get_bit_depth (*png_ptr, *info_ptr) * 3;  
  imgdims->linesize = 0;  //unused
  type = png_get_color_type (*png_ptr, *info_ptr);

  surfman_info ("found some image: %ux%u, %ubpp, type (see "
    "http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html): %d", 
    imgdims->xres, imgdims->yres, imgdims->bpp, type);

  return 0;

open_png_fail:
  png_destroy_read_struct (png_ptr, info_ptr, NULL);
  fclose (*fil);
  return -1;
}



static void
load_png (uint8_t *framebuf, FILE *pic, png_infop *info_ptr, 
  png_structp *png_ptr, const struct fbdim *imgdims)
{
  unsigned int y;
  png_bytep *row_ptrs;
  
  if (!(row_ptrs = (png_bytep*) malloc (sizeof (png_bytep) * imgdims->yres)))
    {
      surfman_error ("malloc for row pointers failed");
      return;
    }

  for (y=0; y<imgdims->yres; y++)
    {
      row_ptrs[y] = (png_byte *) (framebuf + y * imgdims->linesize);
    }

  png_set_expand (*png_ptr);
  png_set_bgr (*png_ptr);
  png_set_filler (*png_ptr, 0, PNG_FILLER_AFTER);
  png_read_image (*png_ptr, row_ptrs);
  free (row_ptrs);
}



int
splash_init (void)
{
  if (splashdomain)
    {
      if (splashdomain->devices.e_first)
        {
          fbtap_takedown (splashdomain->devices.e_first);
          splashdomain->devices.e_first = NULL;
          plugin_display_commit (1);
          /* displays are committed now, because if it's done later
             all displays are going to be evicted, which might lead
             to the destruction of a freshly created splash domain */
        }

      splashdev = NULL;
      //domain_destroy (splashdomain);    re-using the old domain
    }
  else if (!(splashdomain = domain_create (SPLASH_DOM_ID)))
    {
      surfman_error ("splashdomain create failed");
      return -1;
    }

  return 0;
}



/* Ugly fix for the moment */
// from fbtap.c (surfman device)
int fbtap_device_fd (struct device *surf_dev);

int
splash_text (const char *text)
{
  uint8_t *fb;
  uint32_t linesize, screenwidth, screenheight;
  surfman_surface_t *surface;
  unsigned int line, col, Bpp;
  const unsigned int xres=8, yres=8;  // geometry of a character
  static unsigned int xpos=0, ypos=0;
  int c;

  if (mmap_splash (&fb, &linesize, &screenwidth, &screenheight, &Bpp))
    {
      surfman_error ("something went wrong in mmap_splash, won't do anything");
      return -1;
    }

  while ((c = *(text++)))
    {
      int nextpos = 0;

      if (0xa == c)
        {
          xpos = 0;
          ypos += yres;
        }
      else if (0xd == c)
        {
          xpos = 0;
        }
      else if (isblank (c))
        {
          nextpos = 1;
        }
      else if ((c >= 0x21) && (c <= 0x7e))
        {
          c -= 0x21;
          nextpos = 1;

          for (line = 0; (line < 8) && ((line+ypos) < screenheight); line++)
            {
              for (col = 0; (col < 8) && ((col+xpos) < screenwidth); col++)
                {
                  uint8_t *pxfb = fb + linesize * (ypos+line) + Bpp * (xpos+col);
                  uint8_t *pxrast = (uint8_t *) &simplefont.ch[c][line][col];

                  pxfb[0] = pxrast[3];
                  pxfb[1] = pxrast[2];
                  pxfb[2] = pxrast[1];
                  pxfb[3] = pxrast[0];
                }
             }
         }
      else
        {
          surfman_error ("ignoring unsupported character: 0x%x", c);
        }

      if (nextpos)
        {
          xpos += xres;
        }

      if (xpos >= screenwidth)
        {
          xpos = 0;
          ypos += yres;
        }

      if (ypos >= screenheight)
        {
          xpos = 0;
          ypos = 0;
        }
    }

  if (munmap (fb, screenheight * linesize))
    {
      surfman_error ("munmap of fbtap failed: %s", strerror (errno));
      return -1;
    }

  return 0;
}



int
splash_picture (const char *fname)
{
  uint32_t screensize, align, misalign, linesize;
  FILE *pic = NULL;
  uint8_t *fb = NULL ;
  struct fbdim imgdims;
  char full_path[PATH_MAX+1];
  png_infop info_ptr;
  png_structp png_ptr;
  const char *imgdir;
  unsigned int n, t;
  char c;

  if (splash_init ())
    {
      surfman_error ("splash_init() failed");
      return -1;
    }

  if (!(imgdir = config_get ("surfman", "splash_images")))
    {
      surfman_warning ("no directory for splash images configured");
      return 0;
    }

  full_path[0] = 0;
  strncat (full_path, imgdir, PATH_MAX);
  strncat (full_path, "/", PATH_MAX);

  for (n = 0, t = strlen (full_path); 
      (n < strlen (fname)) && (t < (PATH_MAX-1)); n++, t++)
    {
      c = fname[n];
      if (!(isalnum (c) || (c == '.') || (c == '_') || (c == '-')))
        {
          surfman_error ("invalid character in file name: '%c'", c);
          return -1;
        }

      full_path[t] = c;
    }

  full_path[t] = 0;

  if (open_png (full_path, &imgdims, &info_ptr, &png_ptr, &pic))
    {
      surfman_error ("cannot open alleged PNG file %s", fname);
      return -1;
    }
  
  if (setjmp (png_jmpbuf (png_ptr)))
    {
      surfman_warning ("PNG longjmp called");
      goto splash_pic_1;
    }

  align = plugin_stride_align();
  imgdims.bpp = 32;
  linesize = imgdims.xres * imgdims.bpp / 8;

  if ((misalign = linesize % align))
    {
      linesize += align - misalign;
    }

  imgdims.linesize = linesize;

  if (!(splashdev = fbtap_device_create (splashdomain, 0, &imgdims)))
    {
      surfman_error ("splashdevice create failed");
      goto splash_pic_1;
    }

  //TODO: image size > fb? also: maybe refactor to use mmap_splash()
  screensize = imgdims.yres * imgdims.linesize;

  fb = (uint8_t *) mmap (0, screensize, PROT_WRITE | PROT_READ, MAP_SHARED,
                         fbtap_device_fd ((struct device *) splashdev), 0);

  if (MAP_FAILED == fb)
    {
      surfman_error ("mmap of fbtap failed: %s", strerror (errno));
      goto splash_pic_1;
    }

  load_png (fb, pic, &info_ptr, &png_ptr, &imgdims);

  if (domain_set_visible (splashdomain, 1))
    {
      surfman_error ("splash domain_set_visible failed");
    }

  if ((munmap (fb, screensize)))
    {
      surfman_error ("fbtap munmap failed: %s", strerror (errno));
    }

splash_pic_1:
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
  fclose (pic);

  return -1;
}
