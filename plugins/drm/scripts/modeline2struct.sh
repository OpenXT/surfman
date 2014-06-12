#! /bin/bash
#
# Copyright (c) 2013 Citrix Systems, Inc.
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

if [ $# -ne 2 ]; then
    echo "$0 <file> <variable-name>"
    exit 1
fi

parse()
{
    comment="$1x$2 @$3Hz"
    vrefresh=$3
    name=$8

    for i in {1..8}; do
        shift;
    done

    clock=`echo "$1 * 1000" | bc`
    clock=${clock%%.*}
    hdisplay=$2
    hsyncstart=$3
    hsyncend=$4
    htotal=$5

    vdisplay=$6
    vsyncstart=$7
    vsyncend=$8
    vtotal=$9

    for i in {1..9}; do
        shift;
    done

    # Flags (TODO: to be completed)
    if [ "$1" = "+HSync" ]; then
        flags="DRM_MODE_FLAG_PHSYNC"
    elif [ "$1" = "-HSync" ]; then
        flags="DRM_MODE_FLAG_NHSYNC"
    fi
    if [ "$2" = "+VSync" ]; then
        flags="${flags} | DRM_MODE_FLAG_PVSYNC"
    elif [ "$2" = "-VSync" ]; then
        flags="${flags} | DRM_MODE_FLAG_NVSYNC"
    fi
    if [ "$3" = "Interlace" ]; then
        flags="${flags} | DRM_MODE_FLAG_INTERLACE"
    fi

    hskew=0 # not provided in VESA list.
    vscan=1 # not provided in VESA list.

    echo "    /* $comment */"
    echo "    { .name = $name,  .clock = $clock, .vrefresh=$vrefresh,"
    echo "      .hdisplay = $hdisplay, .hsync_start = $hsyncstart, .hsync_end = $hsyncend, .htotal = $htotal,"
    echo "      .vdisplay = $vdisplay, .vsync_start = $vsyncstart, .vsync_end = $hsyncend, .vtotal = $vtotal,"
    echo "      .hskew = $hskew, .vscan = $vscan,"
    echo "      .flags = $flags,"
    echo "      .type = DRM_MODE_TYPE_USERDEF },"
}

echo "static drmModeModeInfo $2[] = {"
while read l; do
    if [ ! -z "$l" ]; then
        parse $l
    fi
done < $1

echo "};"
