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

static drmModeModeInfo vesa_modelines[] = {
    /* 1920x1440 @75Hz */
    { .name = "1920x1440",  .clock = 297000, .vrefresh=75,
      .hdisplay = 1920, .hsync_start = 2064, .hsync_end = 2288, .htotal = 2640,
      .vdisplay = 1440, .vsync_start = 1441, .vsync_end = 2288, .vtotal = 1500,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1920x1440 @60Hz */
    { .name = "1920x1440",  .clock = 234000, .vrefresh=60,
      .hdisplay = 1920, .hsync_start = 2048, .hsync_end = 2256, .htotal = 2600,
      .vdisplay = 1440, .vsync_start = 1441, .vsync_end = 2256, .vtotal = 1500,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1856x1392 @75Hz */
    { .name = "1856x1392",  .clock = 288000, .vrefresh=75,
      .hdisplay = 1856, .hsync_start = 1984, .hsync_end = 2208, .htotal = 2560,
      .vdisplay = 1392, .vsync_start = 1393, .vsync_end = 2208, .vtotal = 1500,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1856x1392 @60Hz */
    { .name = "1856x1392",  .clock = 218300, .vrefresh=60,
      .hdisplay = 1856, .hsync_start = 1952, .hsync_end = 2176, .htotal = 2528,
      .vdisplay = 1392, .vsync_start = 1393, .vsync_end = 2176, .vtotal = 1439,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1792x1344 @75Hz */
    { .name = "1792x1344",  .clock = 261000, .vrefresh=75,
      .hdisplay = 1792, .hsync_start = 1888, .hsync_end = 2104, .htotal = 2456,
      .vdisplay = 1344, .vsync_start = 1345, .vsync_end = 2104, .vtotal = 1417,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1792x1344 @60Hz */
    { .name = "1792x1344",  .clock = 204800, .vrefresh=60,
      .hdisplay = 1792, .hsync_start = 1920, .hsync_end = 2120, .htotal = 2448,
      .vdisplay = 1344, .vsync_start = 1345, .vsync_end = 2120, .vtotal = 1394,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 1600x1200 @85Hz */
//    { .name = "1600x1200",  .clock = 229500, .vrefresh=85,
//      .hdisplay = 1600, .hsync_start = 1664, .hsync_end = 1856, .htotal = 2160,
//      .vdisplay = 1200, .vsync_start = 1201, .vsync_end = 1856, .vtotal = 1250,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
    /* 1600x1200 @75Hz */
    { .name = "1600x1200",  .clock = 202500, .vrefresh=75,
      .hdisplay = 1600, .hsync_start = 1664, .hsync_end = 1856, .htotal = 2160,
      .vdisplay = 1200, .vsync_start = 1201, .vsync_end = 1856, .vtotal = 1250,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1600x1200 @70Hz */
    { .name = "1600x1200",  .clock = 189000, .vrefresh=70,
      .hdisplay = 1600, .hsync_start = 1664, .hsync_end = 1856, .htotal = 2160,
      .vdisplay = 1200, .vsync_start = 1201, .vsync_end = 1856, .vtotal = 1250,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1600x1200 @65Hz */
    { .name = "1600x1200",  .clock = 175500, .vrefresh=65,
      .hdisplay = 1600, .hsync_start = 1664, .hsync_end = 1856, .htotal = 2160,
      .vdisplay = 1200, .vsync_start = 1201, .vsync_end = 1856, .vtotal = 1250,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1600x1200 @60Hz */
    { .name = "1600x1200",  .clock = 162000, .vrefresh=60,
      .hdisplay = 1600, .hsync_start = 1664, .hsync_end = 1856, .htotal = 2160,
      .vdisplay = 1200, .vsync_start = 1201, .vsync_end = 1856, .vtotal = 1250,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 1280x960 @85Hz */
//    { .name = "1280x960",  .clock = 148500, .vrefresh=85,
//      .hdisplay = 1280, .hsync_start = 1344, .hsync_end = 1504, .htotal = 1728,
//      .vdisplay = 960, .vsync_start = 961, .vsync_end = 1504, .vtotal = 1011,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
    /* 1280x960 @60Hz */
    { .name = "1280x960",  .clock = 108000, .vrefresh=60,
      .hdisplay = 1280, .hsync_start = 1376, .hsync_end = 1488, .htotal = 1800,
      .vdisplay = 960, .vsync_start = 961, .vsync_end = 1488, .vtotal = 1000,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 1280x1024 @85Hz */
//    { .name = "1280x1024",  .clock = 157500, .vrefresh=85,
//      .hdisplay = 1280, .hsync_start = 1344, .hsync_end = 1504, .htotal = 1728,
//      .vdisplay = 1024, .vsync_start = 1025, .vsync_end = 1504, .vtotal = 1072,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
    /* 1280x1024 @75Hz */
    { .name = "1280x1024",  .clock = 135000, .vrefresh=75,
      .hdisplay = 1280, .hsync_start = 1296, .hsync_end = 1440, .htotal = 1688,
      .vdisplay = 1024, .vsync_start = 1025, .vsync_end = 1440, .vtotal = 1066,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1280x1024 @60Hz */
    { .name = "1280x1024",  .clock = 108000, .vrefresh=60,
      .hdisplay = 1280, .hsync_start = 1328, .hsync_end = 1440, .htotal = 1688,
      .vdisplay = 1024, .vsync_start = 1025, .vsync_end = 1440, .vtotal = 1066,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1152x864 @75Hz */
    { .name = "1152x864",  .clock = 108000, .vrefresh=75,
      .hdisplay = 1152, .hsync_start = 1216, .hsync_end = 1344, .htotal = 1600,
      .vdisplay = 864, .vsync_start = 865, .vsync_end = 1344, .vtotal = 900,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 1024x768 @87Hz */
//    { .name = "1024x768",  .clock = 44900, .vrefresh=87,
//      .hdisplay = 1024, .hsync_start = 1032, .hsync_end = 1208, .htotal = 1264,
//      .vdisplay = 768, .vsync_start = 768, .vsync_end = 1208, .vtotal = 817,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC | DRM_MODE_FLAG_INTERLACE,
//      .type = DRM_MODE_TYPE_USERDEF },
//    /* 1024x768 @85Hz */
//    { .name = "1024x768",  .clock = 94500, .vrefresh=85,
//      .hdisplay = 1024, .hsync_start = 1072, .hsync_end = 1168, .htotal = 1376,
//      .vdisplay = 768, .vsync_start = 769, .vsync_end = 1168, .vtotal = 808,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
    /* 1024x768 @75Hz */
    { .name = "1024x768",  .clock = 78750, .vrefresh=75,
      .hdisplay = 1024, .hsync_start = 1040, .hsync_end = 1136, .htotal = 1312,
      .vdisplay = 768, .vsync_start = 769, .vsync_end = 1136, .vtotal = 800,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1024x768 @70Hz */
    { .name = "1024x768",  .clock = 75000, .vrefresh=70,
      .hdisplay = 1024, .hsync_start = 1048, .hsync_end = 1184, .htotal = 1328,
      .vdisplay = 768, .vsync_start = 771, .vsync_end = 1184, .vtotal = 806,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 1024x768 @60Hz */
    { .name = "1024x768",  .clock = 65000, .vrefresh=60,
      .hdisplay = 1024, .hsync_start = 1048, .hsync_end = 1184, .htotal = 1344,
      .vdisplay = 768, .vsync_start = 771, .vsync_end = 1184, .vtotal = 806,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 800x600 @85Hz */
//    { .name = "800x600",  .clock = 56300, .vrefresh=85,
//      .hdisplay = 800, .hsync_start = 832, .hsync_end = 896, .htotal = 1048,
//      .vdisplay = 600, .vsync_start = 601, .vsync_end = 896, .vtotal = 631,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
    /* 800x600 @75Hz */
    { .name = "800x600",  .clock = 49500, .vrefresh=75,
      .hdisplay = 800, .hsync_start = 816, .hsync_end = 896, .htotal = 1056,
      .vdisplay = 600, .vsync_start = 601, .vsync_end = 896, .vtotal = 625,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 800x600 @72Hz */
    { .name = "800x600",  .clock = 50000, .vrefresh=72,
      .hdisplay = 800, .hsync_start = 856, .hsync_end = 976, .htotal = 1040,
      .vdisplay = 600, .vsync_start = 637, .vsync_end = 976, .vtotal = 666,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 800x600 @60Hz */
    { .name = "800x600",  .clock = 40000, .vrefresh=60,
      .hdisplay = 800, .hsync_start = 840, .hsync_end = 968, .htotal = 1056,
      .vdisplay = 600, .vsync_start = 601, .vsync_end = 968, .vtotal = 628,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 800x600 @56Hz */
    { .name = "800x600",  .clock = 36000, .vrefresh=56,
      .hdisplay = 800, .hsync_start = 824, .hsync_end = 896, .htotal = 1024,
      .vdisplay = 600, .vsync_start = 601, .vsync_end = 896, .vtotal = 625,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 720x400 @85Hz */
//    { .name = "720x400",  .clock = 35500, .vrefresh=85,
//      .hdisplay = 720, .hsync_start = 756, .hsync_end = 828, .htotal = 936,
//      .vdisplay = 400, .vsync_start = 401, .vsync_end = 828, .vtotal = 446,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
//    /* 640x480 @85Hz */
//    { .name = "640x480",  .clock = 36000, .vrefresh=85,
//      .hdisplay = 640, .hsync_start = 696, .hsync_end = 752, .htotal = 832,
//      .vdisplay = 480, .vsync_start = 481, .vsync_end = 752, .vtotal = 509,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
    /* 640x480 @75Hz */
    { .name = "640x480",  .clock = 31500, .vrefresh=75,
      .hdisplay = 640, .hsync_start = 656, .hsync_end = 720, .htotal = 840,
      .vdisplay = 480, .vsync_start = 481, .vsync_end = 720, .vtotal = 500,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 640x480 @73Hz */
    { .name = "640x480",  .clock = 31500, .vrefresh=73,
      .hdisplay = 640, .hsync_start = 664, .hsync_end = 704, .htotal = 832,
      .vdisplay = 480, .vsync_start = 489, .vsync_end = 704, .vtotal = 520,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
    /* 640x480 @60Hz */
    { .name = "640x480",  .clock = 25180, .vrefresh=60,
      .hdisplay = 640, .hsync_start = 656, .hsync_end = 752, .htotal = 800,
      .vdisplay = 480, .vsync_start = 490, .vsync_end = 752, .vtotal = 525,
      .hskew = 0, .vscan = 1,
      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
      .type = DRM_MODE_TYPE_USERDEF },
//    /* 640x400 @85Hz */
//    { .name = "640x400",  .clock = 31500, .vrefresh=85,
//      .hdisplay = 640, .hsync_start = 672, .hsync_end = 736, .htotal = 832,
//      .vdisplay = 400, .vsync_start = 401, .vsync_end = 736, .vtotal = 445,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
//    /* 640x350 @85Hz */
//    { .name = "640x350",  .clock = 31500, .vrefresh=85,
//      .hdisplay = 640, .hsync_start = 672, .hsync_end = 736, .htotal = 832,
//      .vdisplay = 350, .vsync_start = 382, .vsync_end = 736, .vtotal = 445,
//      .hskew = 0, .vscan = 1,
//      .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC,
//      .type = DRM_MODE_TYPE_USERDEF },
};
