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
#include "project.h"

static int
get_bpp (enum surfman_surface_format format)
{
  int ret;

  switch (format)
    {
    case SURFMAN_FORMAT_BGR565:
      ret = 16;
      break;
    case SURFMAN_FORMAT_BGRX8888:
    case SURFMAN_FORMAT_RGBX8888:
      ret = 32;
      break;
    case SURFMAN_FORMAT_UNKNOWN:
    default:
      ret = 0;
    }

  return ret;
}

static inline unsigned long
__ffs(unsigned long word)
{
  asm("bsf %1,%0" : "=r" (word) : "rm" (word));
  return word;
}

static inline unsigned long
__fls(unsigned long word)
{
  asm("bsr %1,%0" : "=r" (word) : "rm" (word));
  return word;
}

static int scan_bitmap_fwd(uint8_t *dirty, unsigned int max)
{
  unsigned long *d = (unsigned long *)dirty;
  unsigned int i, len;
  unsigned int ret = -1;

  len = (max + sizeof (unsigned long) * 8 - 1) /
    (sizeof (unsigned long) * 8);

  for (i = 0; i < len; i++)
    {
      if (d[i] != 0)
        {
          unsigned long bitidx = __ffs(d[i]);

          ret = i * sizeof (unsigned long) * 8 + bitidx;
          break;
        }
    }

  if (ret >= max)
    ret = -1;

  return ret;
}

static int scan_bitmap_rev(uint8_t *dirty, unsigned int max)
{
  unsigned long *d = (unsigned long *)dirty;
  int i, len;
  unsigned int ret = -1;

  len = (max + sizeof (unsigned long) * 8 - 1) /
    (sizeof (unsigned long) * 8);

  i = len - 1;
  if (len)
    {
      unsigned long last = max % (sizeof (unsigned long) * 8);
      unsigned long word = d[i];

      if (last)
        word &= (1 << last) - 1;

      if (word)
        return __fls(word) + i * sizeof (unsigned long) * 8;

      i--;
    }
  for (; i >= 0; i--)
    {
      if (d[i] != 0)
        {
          unsigned long bitidx = __ffs(d[i]);

          ret = i * sizeof (unsigned long) * 8 + bitidx;
          break;
        }
    }

  return ret;
}

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

int
rect_from_dirty_bitmap (uint8_t *dirty,
                        unsigned int width,
                        unsigned int height,
                        unsigned int stride,
                        enum surfman_surface_format format,
                        surfman_rect_t *rect)
{
  int bpp;
  int npages;
  int f, l;
  unsigned int x1, y1, x2, y2;

  bpp = get_bpp (format);
  if (!bpp)
    return -1;

  npages = (height * stride + (XC_PAGE_SIZE - 1)) / XC_PAGE_SIZE;

  f = scan_bitmap_fwd (dirty, npages);
  if (f == -1)
    return -1;
  l = scan_bitmap_rev (dirty, npages);

  y1 = (f * XC_PAGE_SIZE) / stride;
  x1 = ((f * XC_PAGE_SIZE) % stride) / (bpp / 8);
  if (x1 > width)
    {
      x1 = 0;
      y1++;
    }

  y2 = (l * XC_PAGE_SIZE + (XC_PAGE_SIZE - (bpp / 8))) / stride;
  x2 = ((l * XC_PAGE_SIZE + (XC_PAGE_SIZE - (bpp / 8))) % stride) / (bpp / 8);
  if (x2 > width)
    x2 = width;

  rect->x = min (x1, x2);
  rect->y = min (y1, y2);

  rect->w = max (x1, x2) - rect->x;
  rect->h = max (y1, y2) - rect->y;

  return 0;
}


