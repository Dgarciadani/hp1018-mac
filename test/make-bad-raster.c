/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Gregorio Garcia
 *
 * Writes malformed CUPS raster files to check that rastertozjs rejects
 * them cleanly instead of crashing.
 *
 * Usage: make-bad-raster <case> > file.ras
 *   ok, huge, tiny-bpl, nan-bbox, bad-res, bad-bpp, truncated
 */

#include <cups/raster.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    cups_raster_t	*ras;
    cups_page_header2_t	h;
    unsigned char	*line;
    unsigned		y, rows;
    const char		*c;

    if (argc != 2)
    {
	fputs("Usage: make-bad-raster <case>\n", stderr);
	return 2;
    }
    c = argv[1];

    memset(&h, 0, sizeof(h));
    h.HWResolution[0] = h.HWResolution[1] = 600;
    h.PageSize[0] = 612;
    h.PageSize[1] = 792;
    h.cupsImagingBBox[2] = 612.0f;
    h.cupsImagingBBox[3] = 792.0f;
    h.cupsWidth = 5100;
    h.cupsHeight = 6600;
    h.cupsBitsPerColor = 8;
    h.cupsBitsPerPixel = 8;
    h.cupsColorSpace = CUPS_CSPACE_K;
    h.cupsColorOrder = CUPS_ORDER_CHUNKED;
    h.cupsBytesPerLine = 5100;
    rows = h.cupsHeight;

    if (!strcmp(c, "huge"))
    {
	h.cupsWidth = 4000000;
	h.cupsHeight = 4000000;
	h.cupsBytesPerLine = 4000000;
	rows = 1;
    }
    else if (!strcmp(c, "tiny-bpl"))
    {
	h.cupsBytesPerLine = 16;
	rows = 1;
    }
    else if (!strcmp(c, "nan-bbox"))
    {
	h.cupsImagingBBox[0] = NAN;
	rows = 1;
    }
    else if (!strcmp(c, "bad-res"))
    {
	h.HWResolution[0] = 300;
	rows = 1;
    }
    else if (!strcmp(c, "bad-bpp"))
    {
	h.cupsBitsPerPixel = 16;
	rows = 1;
    }
    else if (!strcmp(c, "truncated"))
	rows = 100;
    else if (strcmp(c, "ok"))
    {
	fprintf(stderr, "Unknown case %s\n", c);
	return 2;
    }

    line = calloc(1, h.cupsBytesPerLine > 65536 ? 65536 : h.cupsBytesPerLine);
    if (!line)
	return 1;
    memset(line, 0x80, h.cupsBytesPerLine > 65536 ? 65536 : h.cupsBytesPerLine);

    ras = cupsRasterOpen(1, CUPS_RASTER_WRITE);
    cupsRasterWriteHeader2(ras, &h);
    if (h.cupsBytesPerLine <= 65536)
	for (y = 0; y < rows; ++y)
	    cupsRasterWritePixels(ras, line, h.cupsBytesPerLine);
    cupsRasterClose(ras);
    free(line);
    return 0;
}
