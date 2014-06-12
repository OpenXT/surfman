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
#include <png.h>

static void *
map_surface(surfman_surface_t *s, size_t *surface_len)
{
  unsigned int i;
  void *p;
  size_t page_count;

  if (*surface_len)
    page_count = (*surface_len + (XC_PAGE_SIZE - 1)) >> XC_PAGE_SHIFT;
  else
    page_count = s->page_count;

  if (page_count > s->page_count)
    return NULL;

  p = surface_map (s);

  *surface_len = page_count * XC_PAGE_SIZE;
  return p;
}

int
surface_snapshot (surfman_surface_t *surf, const char *filename)
{
  png_structp png = NULL;
  png_infop png_info = NULL;
  FILE *output;
  void *fb;
  size_t mapping_len;
  unsigned int h = 0;
  unsigned int w = 0;
  uint32_t *row;
  int ret = SURFMAN_ERROR;

  if (surf->page_count == 0)
  {
    surfman_error ("Surface doesn't have any pages");
    return SURFMAN_ERROR;
  }

  /* Don't care about complex surface format */
  if (surf->format != SURFMAN_FORMAT_BGRX8888 &&
      surf->format != SURFMAN_FORMAT_RGBX8888)
    {
      surfman_error ("Unsupported format: %x", surf->format);
      return SURFMAN_ERROR;
    }

  png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL,
                                 NULL, NULL);
  if (!png)
    {
      surfman_error ("Failed to create PNG structure");
      return SURFMAN_ERROR;
    }

  png_info = png_create_info_struct (png);
  if (!png_info)
    {
      surfman_error ("Failed to create PNG info structure");
      goto fail_0;
    }

  output = fopen (filename, "wb");
  if (!output)
    {
      surfman_error ("Failed to create PNG output file %s: %s",
             filename, strerror (errno));
      goto fail_0;
    }

  png_init_io (png, output);
  png_set_IHDR (png, png_info, surf->width, surf->height,
                8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_write_info (png, png_info);

  mapping_len = surf->stride * surf->height;
  fb = map_surface (surf, &mapping_len);
  if (!fb)
    {
      surfman_error ("Failed to CPU map surface.");
      goto fail_1;
    }

  if (NULL == (row = malloc (surf->width * 4)))
    {
      surfman_error ("malloc'ing %d bytes failed", surf->width * 4);
      goto fail_2;
    }

  for (h = 0; h < surf->height; h++)
    {
      uint32_t *pix = (uint32_t *)((char *)fb + h * surf->stride);

      for (w = 0; w < surf->width; w++)
        {
          row[w] = pix[w] | 0xff000000;

          /* Swap B and R channel */
          if (surf->format == SURFMAN_FORMAT_BGRX8888)
            row[w] = (row[w] & 0xff00ff00) |
                     ((row[w] & 0xff0000) >> 16) |
                     ((row[w] & 0xff) << 16);
        }

      png_write_row (png, (png_bytep)row);
    }

  free (row);
  png_write_end(png, NULL);
  surfman_info ("Successfuly wrote %s", filename);
  ret = SURFMAN_SUCCESS;

fail_2:
  /* do not unmap as this causes the surface to be unmapped (no ref counting)
     see XC-8638
  surface_unmap (surf); */

fail_1:
  fclose (output);

fail_0:
  png_destroy_write_struct(&png, &png_info);

  if (SURFMAN_SUCCESS != ret)
    surfman_error ("failed to write %s", filename);

  return ret;
}

