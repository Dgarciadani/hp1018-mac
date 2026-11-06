/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Gregorio Garcia
 *
 * rastertozjs - CUPS filter for the HP LaserJet 1018 (ZjStream, host-based)
 *
 * Converts CUPS raster (as produced by macOS cgpdftoraster) into the
 * ZjStream/JBIG protocol understood by the HP LaserJet 1018/1020/1022,
 * and uploads the printer firmware when the printer has just been
 * powered on (it forgets it every time).
 *
 * The ZjStream encoding follows foo2zjs by Rick Richardson
 * (model "-z1", options "-P -L0"), and the JBIG compression is
 * Markus Kuhn's jbig-kit, both GPL. This file is GPL-2.0-or-later too.
 *
 * Options (from the PPD via the raster header, or job options):
 *   cupsInteger1 / HPDensity    print density 1..5 (default 3)
 *   cupsInteger2 / HPEconoMode  0 = off, 1 = toner saving
 *   cupsInteger3 / HPFirmware   0 = auto, 1 = always, 2 = never
 */

#include <cups/cups.h>
#include <cups/raster.h>
#include <cups/sidechannel.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "jbig.h"
#include "zjs.h"

#ifndef FIRMWARE_PATH
#define FIRMWARE_PATH "/Library/Printers/HP1018/Firmware/sihp1018.dl"
#endif

enum { FW_AUTO = 0, FW_ALWAYS = 1, FW_NEVER = 2 };

/* Seconds the printer needs to boot the firmware (orange light blinks) */
#define FW_BOOT_WAIT	12

/* Unprintable border clipped off each side, in 600 dpi pixels (foo2zjs -z1) */
#define CLIP_600	96

/* Largest page accepted, in inches (Legal is 8.5 x 14) */
#define MAX_WIDTH_IN	9
#define MAX_HEIGHT_IN	15

/* Sanity limit for the firmware file (the real one is ~130 KB) */
#define FW_MAX_SIZE	(1024 * 1024)
#define FW_MAGIC	"\033%-12345X@PJL ENTER LANGUAGE=ACL"

/* Size of each JBIG data chunk sent to the printer */
#define BID_SIZE	65536
#define EXTRA_PAD	16

static volatile sig_atomic_t Canceled = 0;
static int PageNum = 0;

/*
 * Job settings
 */
static int Density = 3;
static int EconoMode = 0;
static int FirmwareMode = FW_AUTO;

/*
 * Paper sizes, in points, mapped to the DEVMODE paper codes the
 * printer expects (values for the 1018/1020 family from foo2zjs).
 */
static const struct {
    int w, h, code;
} Papers[] = {
    { 612,  792,  DMPAPER_LETTER },
    { 612,  1008, DMPAPER_LEGAL },
    { 522,  756,  DMPAPER_EXECUTIVE },
    { 595,  842,  DMPAPER_A4 },
    { 420,  595,  DMPAPER_A5 },
    { 297,  420,  262 },		/* A6 */
    { 498,  708,  DMPAPER_B5 },
    { 283,  416,  260 },		/* Postcard */
    { 297,  684,  DMPAPER_ENV_10 },
    { 311,  623,  DMPAPER_ENV_DL },
    { 459,  649,  DMPAPER_ENV_C5 },
    { 279,  540,  DMPAPER_ENV_MONARCH },
    { 558,  774,  257 },		/* 16k 197x273 mm */
};

/*
 * A linked list of compressed JBIG data: the first entry is the 20 byte
 * BIH, the rest are BID_SIZE chunks (the last one is whatever remains).
 */
typedef struct bie_chain {
    unsigned char	*data;
    size_t		len;
    struct bie_chain	*next;
} BIE_CHAIN;

static unsigned char DitherMatrix[8][8];

static void
fatal(const char *msg)
{
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

static void
cancel_job(int sig)
{
    (void) sig;
    Canceled = 1;
}

/*
 * ZjStream records (all big-endian)
 */
static void
put_be32(uint32_t v, FILE *fp)
{
    putc((v >> 24) & 0xff, fp);
    putc((v >> 16) & 0xff, fp);
    putc((v >> 8) & 0xff, fp);
    putc(v & 0xff, fp);
}

static void
put_be16(uint16_t v, FILE *fp)
{
    putc((v >> 8) & 0xff, fp);
    putc(v & 0xff, fp);
}

static void
chunk_write_rsvd(uint32_t type, uint16_t rsvd, uint32_t items, uint32_t size,
		 FILE *fp)
{
    put_be32(sizeof(ZJ_HEADER) + size, fp);
    put_be32(type, fp);
    put_be32(items, fp);
    put_be16(rsvd, fp);
    put_be16(0x5a5a, fp);			/* 'ZZ' */
}

static void
chunk_write(uint32_t type, uint32_t items, uint32_t size, FILE *fp)
{
    chunk_write_rsvd(type, 0, items, size, fp);
}

static void
item_uint32_write(uint16_t item, uint32_t value, FILE *fp)
{
    put_be32(sizeof(ZJ_ITEM_UINT32), fp);
    put_be16(item, fp);
    putc(ZJIT_UINT32, fp);
    putc(0, fp);
    put_be32(value, fp);
}

static void
free_chain(BIE_CHAIN *chain)
{
    BIE_CHAIN *next;

    while (chain)
    {
	next = chain->next;
	free(chain->data);
	free(chain);
	chain = next;
    }
}

static BIE_CHAIN *
new_link(void)
{
    BIE_CHAIN *link = calloc(1, sizeof(BIE_CHAIN));

    if (!link)
	fatal("Out of memory");
    return link;
}

/*
 * jbig-kit output callback: splits the BIE into the chain.
 */
static void
output_jbig(unsigned char *start, size_t len, void *cbarg)
{
    BIE_CHAIN	**root = (BIE_CHAIN **) cbarg;
    BIE_CHAIN	*current;
    size_t	size = BID_SIZE;

    if (*root == NULL)
    {
	if (len != 20)
	    fatal("First JBIG chunk must be the 20 byte BIH");
	*root = new_link();
	size = 20;
    }

    for (current = *root; current->next; current = current->next)
	;

    while (len > 0)
    {
	size_t amt, left;

	if (!current->data)
	{
	    current->data = malloc(size);
	    if (!current->data)
		fatal("Out of memory");
	}
	left = size - current->len;
	amt = len > left ? left : len;
	memcpy(current->data + current->len, start, amt);
	current->len += amt;
	len -= amt;
	start += amt;

	if (current->len == size)
	{
	    current->next = new_link();
	    current = current->next;
	    size = BID_SIZE;
	}
    }
}

static void
start_doc(FILE *fp)
{
    time_t	now = time(NULL);
    char	datetime[14 + 1];

    strftime(datetime, sizeof(datetime), "%Y%m%d%H%M%S", localtime(&now));

    fprintf(fp, "\033%%-12345X@PJL JOB\n");
    fprintf(fp, "@PJL SET JAMRECOVERY=OFF\n");
    fprintf(fp, "@PJL SET DENSITY=%d\n", Density);
    fprintf(fp, "@PJL SET ECONOMODE=%s\n", EconoMode ? "ON" : "OFF");
    fprintf(fp, "@PJL SET RET=MEDIUM\n");
    fprintf(fp, "@PJL INFO STATUS\n");
    fprintf(fp, "@PJL USTATUS DEVICE = ON\n");
    fprintf(fp, "@PJL USTATUS JOB = ON\n");
    fprintf(fp, "@PJL USTATUS PAGE = ON\n");
    fprintf(fp, "@PJL USTATUS TIMED = 30\n");
    fprintf(fp, "@PJL SET JOBATTR=\"JobAttr4=%s\"", datetime);
    putc(0, fp);
    fprintf(fp, "\033%%-12345X");

    fwrite("JZJZ", 1, 4, fp);
    chunk_write_rsvd(ZJT_START_DOC, 0x24, 3, 3 * sizeof(ZJ_ITEM_UINT32), fp);
    item_uint32_write(ZJI_DMCOLLATE, 0, fp);
    item_uint32_write(ZJI_DMDUPLEX, DMDUPLEX_OFF, fp);
    item_uint32_write(ZJI_PAGECOUNT, 0, fp);
}

static void
end_doc(FILE *fp)
{
    chunk_write(ZJT_END_DOC, 0, 0, fp);
    fprintf(fp, "\033%%-12345X@PJL EOJ\n");
    fprintf(fp, "\033%%-12345X");
}

/*
 * Writes one page. 'width' is the real raster width in device pixels,
 * 'bpp' is ResX/600 (2 for 1200x600 dpi).
 */
static void
write_page(BIE_CHAIN *chain, int width, int height, int bpp, int resy,
	   int papercode, FILE *fp)
{
    BIE_CHAIN	*current;
    int		nitems = 13;

    if (!chain || !chain->next || chain->len != 20)
	fatal("Bad JBIG header");

    chunk_write_rsvd(ZJT_START_PAGE, 0x9c,
		     nitems, nitems * sizeof(ZJ_ITEM_UINT32), fp);
    item_uint32_write(ZJI_ECONOMODE,      EconoMode,         fp);
    item_uint32_write(ZJI_VIDEO_X,        width / bpp,       fp);
    item_uint32_write(ZJI_VIDEO_Y,        height,            fp);
    item_uint32_write(ZJI_VIDEO_BPP,      bpp,               fp);
    item_uint32_write(ZJI_RASTER_X,       width,             fp);
    item_uint32_write(ZJI_RASTER_Y,       height,            fp);
    item_uint32_write(ZJI_NBIE,           1,                 fp);
    item_uint32_write(ZJI_RESOLUTION_X,   600,               fp);
    item_uint32_write(ZJI_RESOLUTION_Y,   resy,              fp);
    item_uint32_write(ZJI_DMDEFAULTSOURCE, DMBIN_AUTO,       fp);
    item_uint32_write(ZJI_DMCOPIES,       1,                 fp);
    item_uint32_write(ZJI_DMPAPER,        papercode,         fp);
    item_uint32_write(ZJI_DMMEDIATYPE,    DMMEDIA_STANDARD,  fp);

    /* Single plane, no START_PLANE record (foo2zjs -P) */
    for (current = chain; current && current->len; current = current->next)
    {
	if (current == chain)
	{
	    chunk_write(ZJT_JBIG_BIH, 0, current->len, fp);
	    fwrite(current->data, 1, current->len, fp);
	}
	else
	{
	    size_t	len = current->len;
	    size_t	pad = 0;

	    if (!current->next || !current->next->len)
		pad = EXTRA_PAD + 4 * ((len + 3) / 4) - len;
	    chunk_write(ZJT_JBIG_BID, 0, len + pad, fp);
	    fwrite(current->data, 1, len, fp);
	    while (pad--)
		putc(0, fp);
	}
    }
    chunk_write(ZJT_END_JBIG, 0, 0, fp);
    chunk_write(ZJT_END_PAGE, 0, 0, fp);
}

/*
 * Clustered-dot ordered dither, which a laser engine reproduces much
 * better than error diffusion. Thresholds are 1..254, so pure white
 * stays white and pure black (text) stays solid.
 */
static void
init_dither(void)
{
    double	spot[64];
    int		order[64];
    int		i, j, k;

    for (i = 0; i < 64; ++i)
    {
	double x = ((i % 8) + 0.5) / 4.0 - 1.0;
	double y = ((i / 8) + 0.5) / 4.0 - 1.0;

	spot[i] = (cos(M_PI * x) + cos(M_PI * y)) / 2.0;
	order[i] = i;
    }
    /* Grow the dot from the brightest spot outwards */
    for (i = 1; i < 64; ++i)
	for (j = i; j > 0 && spot[order[j]] > spot[order[j - 1]]; --j)
	{
	    k = order[j];
	    order[j] = order[j - 1];
	    order[j - 1] = k;
	}
    for (i = 0; i < 64; ++i)
	DitherMatrix[order[i] / 8][order[i] % 8] =
	    (unsigned char) (1 + (i * 253) / 63);
}

static int
paper_code(const cups_page_header2_t *h)
{
    int		i, best = 0;
    long	bestdiff = -1;

    for (i = 0; i < (int) (sizeof(Papers) / sizeof(Papers[0])); ++i)
    {
	long dw = (long) h->PageSize[0] - Papers[i].w;
	long dh = (long) h->PageSize[1] - Papers[i].h;
	long diff = dw * dw + dh * dh;

	if (bestdiff < 0 || diff < bestdiff)
	{
	    bestdiff = diff;
	    best = i;
	}
    }
    if (bestdiff > 25)
	fprintf(stderr, "DEBUG: Unknown page size %ux%u, using code %d\n",
		h->PageSize[0], h->PageSize[1], Papers[best].code);
    return Papers[best].code;
}

/*
 * Converts one raster line into 1-bit black=1 pixels, starting at
 * pixel 'x0', 'w' pixels wide.
 */
static void
line_to_mono(const cups_page_header2_t *h, const unsigned char *src,
	     unsigned char *dst, int x0, int w, int y)
{
    int x;
    int invert = h->cupsColorSpace == CUPS_CSPACE_W
		|| h->cupsColorSpace == CUPS_CSPACE_SW
		|| h->cupsColorSpace == CUPS_CSPACE_RGB
		|| h->cupsColorSpace == CUPS_CSPACE_SRGB;
    const unsigned char *row = DitherMatrix[y & 7];

    if (h->cupsBitsPerPixel == 1)
    {
	for (x = 0; x < w; ++x)
	{
	    int sx = x0 + x;
	    int bit = (src[sx >> 3] >> (7 - (sx & 7))) & 1;

	    if (invert)
		bit = !bit;
	    if (bit)
		dst[x >> 3] |= 0x80 >> (x & 7);
	}
    }
    else if (h->cupsBitsPerPixel == 8)
    {
	for (x = 0; x < w; ++x)
	{
	    int v = src[x0 + x];

	    if (invert)
		v = 255 - v;
	    if (v > row[x & 7])
		dst[x >> 3] |= 0x80 >> (x & 7);
	}
    }
    else	/* 24-bit RGB, chunky */
    {
	for (x = 0; x < w; ++x)
	{
	    const unsigned char *p = src + 3 * (x0 + x);
	    int v = 255 - (p[0] * 77 + p[1] * 150 + p[2] * 29) / 256;

	    if (v > row[x & 7])
		dst[x >> 3] |= 0x80 >> (x & 7);
	}
    }
}

/*
 * The raster comes from whatever document the user printed, so check
 * everything the page code relies on before allocating or indexing.
 */
static int
check_header(const cups_page_header2_t *h)
{
    unsigned	minbpl;
    int		i;

    if (h->cupsBitsPerPixel != 1 && h->cupsBitsPerPixel != 8
	&& !(h->cupsBitsPerPixel == 24 && h->cupsColorOrder == CUPS_ORDER_CHUNKED))
    {
	fprintf(stderr, "ERROR: Unsupported raster format (%u bits/pixel)\n",
		h->cupsBitsPerPixel);
	return -1;
    }
    if ((h->HWResolution[0] != 600 && h->HWResolution[0] != 1200)
	|| h->HWResolution[1] != 600)
    {
	fprintf(stderr, "ERROR: Unsupported resolution %ux%u\n",
		h->HWResolution[0], h->HWResolution[1]);
	return -1;
    }
    if (h->cupsWidth == 0 || h->cupsHeight == 0
	|| h->cupsWidth > MAX_WIDTH_IN * h->HWResolution[0]
	|| h->cupsHeight > MAX_HEIGHT_IN * h->HWResolution[1]
	|| h->PageSize[0] == 0 || h->PageSize[0] > MAX_WIDTH_IN * 72
	|| h->PageSize[1] == 0 || h->PageSize[1] > MAX_HEIGHT_IN * 72)
    {
	fprintf(stderr, "ERROR: Unsupported page size (%ux%u pixels, %ux%u pt)\n",
		h->cupsWidth, h->cupsHeight, h->PageSize[0], h->PageSize[1]);
	return -1;
    }
    minbpl = (h->cupsWidth * h->cupsBitsPerPixel + 7) / 8;
    if (h->cupsBytesPerLine < minbpl || h->cupsBytesPerLine > 4 * minbpl)
    {
	fprintf(stderr, "ERROR: Bad bytes per line %u for width %u\n",
		h->cupsBytesPerLine, h->cupsWidth);
	return -1;
    }
    for (i = 0; i < 4; ++i)
    {
	float limit = (float) h->PageSize[i & 1];

	if (!isfinite(h->cupsImagingBBox[i]) || h->cupsImagingBBox[i] < 0.0f
	    || h->cupsImagingBBox[i] > limit)
	{
	    fprintf(stderr, "ERROR: Bad imaging box\n");
	    return -1;
	}
    }
    return 0;
}

static int
print_page(cups_raster_t *ras, cups_page_header2_t *h, FILE *fp)
{
    int			bpp, w, h_out, bpl16, wjbig;
    int			left, right, top, bottom;
    int			clipx, clipr, clipy, clipb;
    unsigned		y;
    unsigned char	*line, *bitmap, *rowp;
    BIE_CHAIN		*chain = NULL;
    struct jbg_enc_state se;

    if (check_header(h) < 0)
	return -1;

    /*
     * The printer can't image the outer 96 pixels (at 600 dpi) of the
     * sheet. cgpdftoraster usually renders just the PPD ImageableArea,
     * which already leaves that border out, so only clip whatever part
     * of the border the raster still contains.
     */
    bpp = h->HWResolution[0] / 600;
    left = (int) lround(h->cupsImagingBBox[0] * h->HWResolution[0] / 72.0);
    right = (int) lround((h->PageSize[0] - h->cupsImagingBBox[2])
			 * h->HWResolution[0] / 72.0);
    top = (int) lround((h->PageSize[1] - h->cupsImagingBBox[3])
		       * h->HWResolution[1] / 72.0);
    bottom = (int) lround(h->cupsImagingBBox[1] * h->HWResolution[1] / 72.0);
    if (h->cupsImagingBBox[2] <= 0.0f || h->cupsImagingBBox[3] <= 0.0f)
	left = right = top = bottom = 0;	/* full-page raster */

    clipx = CLIP_600 * bpp - left;
    clipr = CLIP_600 * bpp - right;
    clipy = CLIP_600 * h->HWResolution[1] / 600 - top;
    clipb = CLIP_600 * h->HWResolution[1] / 600 - bottom;
    clipx = clipx < 0 ? 0 : clipx;
    clipr = clipr < 0 ? 0 : clipr;
    clipy = clipy < 0 ? 0 : clipy;
    clipb = clipb < 0 ? 0 : clipb;

    w = (int) h->cupsWidth - clipx - clipr;
    h_out = (int) h->cupsHeight - clipy - clipb;
    if (w <= 0 || h_out <= 0)
    {
	fprintf(stderr, "ERROR: Page too small (%ux%u)\n",
		h->cupsWidth, h->cupsHeight);
	return -1;
    }
    bpl16 = (((w + 7) / 8) + 15) & ~15;
    wjbig = bpl16 * 8;		/* = (w + 127) & ~127 */

    line = malloc(h->cupsBytesPerLine);
    bitmap = calloc((size_t) bpl16, (size_t) h_out);
    if (!line || !bitmap)
	fatal("Out of memory");

    ++PageNum;
    fprintf(stderr, "INFO: Imprimiendo pagina %d\n", PageNum);

    rowp = bitmap;
    for (y = 0; y < h->cupsHeight; ++y)
    {
	if (Canceled)
	    break;
	if (cupsRasterReadPixels(ras, line, h->cupsBytesPerLine) == 0)
	    break;
	if ((int) y < clipy || (int) y >= clipy + h_out)
	    continue;
	line_to_mono(h, line, rowp, clipx, w, (int) y);
	rowp += bpl16;
    }
    free(line);

    if (Canceled)
    {
	free(bitmap);
	return -1;
    }

    jbg_enc_init(&se, wjbig, h_out, 1, &bitmap, output_jbig, &chain);
    jbg_enc_options(&se, JBG_ILEAVE | JBG_SMID,
		    JBG_DELAY_AT | JBG_LRLTWO | JBG_TPDON | JBG_TPBON | JBG_DPON,
		    128, 16, 0);
    jbg_enc_out(&se);
    jbg_enc_free(&se);
    free(bitmap);

    write_page(chain, w, h_out, bpp, h->HWResolution[1], paper_code(h), fp);
    free_chain(chain);

    fprintf(stderr, "PAGE: %d 1\n", PageNum);
    return 0;
}

/*
 * Firmware handling. The printer reports "FWVER:" in its IEEE-1284
 * device ID once the firmware is loaded.
 *
 * Returns 1 = loaded, 0 = not loaded, -1 = unknown (no side channel).
 */
static int
firmware_loaded(void)
{
    char		id[1024];
    int			len = sizeof(id) - 1;
    cups_sc_status_t	status;

    status = cupsSideChannelDoRequest(CUPS_SC_CMD_GET_DEVICE_ID, id, &len,
				      5.0);
    if (status != CUPS_SC_STATUS_OK || len <= 0)
	return -1;
    id[len] = '\0';
    fprintf(stderr, "DEBUG: Device ID: %s\n", id);
    return strstr(id, "FWVER") != NULL;
}

static int
send_firmware(FILE *fp)
{
    const char	*path = NULL;
    FILE	*fw;
    char	buf[8192];
    unsigned char *data;
    long	size;
    int		i, len, state;

    /* The override is only for tests run outside of CUPS */
    if (!getenv("DEVICE_URI"))
	path = getenv("HP1018_FIRMWARE");
    if (!path)
	path = FIRMWARE_PATH;
    fw = fopen(path, "rb");
    if (!fw)
    {
	fprintf(stderr, "ERROR: No se encuentra el firmware %s: %s\n",
		path, strerror(errno));
	return -1;
    }

    /* Only ever send a plausible firmware image to the printer */
    if (fseek(fw, 0, SEEK_END) != 0 || (size = ftell(fw)) <= 0
	|| size > FW_MAX_SIZE || fseek(fw, 0, SEEK_SET) != 0)
    {
	fprintf(stderr, "ERROR: Firmware %s has an invalid size\n", path);
	fclose(fw);
	return -1;
    }
    data = malloc((size_t) size);
    if (!data)
	fatal("Out of memory");
    if (fread(data, 1, (size_t) size, fw) != (size_t) size
	|| size < (long) strlen(FW_MAGIC)
	|| memcmp(data, FW_MAGIC, strlen(FW_MAGIC)) != 0)
    {
	fprintf(stderr, "ERROR: %s is not an HP LaserJet firmware file\n", path);
	free(data);
	fclose(fw);
	return -1;
    }
    fclose(fw);

    fprintf(stderr, "INFO: Enviando firmware / Sending firmware...\n");
    if (fwrite(data, 1, (size_t) size, fp) != (size_t) size)
    {
	free(data);
	return -1;
    }
    free(data);
    fflush(fp);

    /*
     * Wait for the backend to push it out, then leave the printer alone
     * while it boots the firmware: a device ID request during the boot
     * hangs the 1018 until it is power cycled.
     */
    len = 0;
    cupsSideChannelDoRequest(CUPS_SC_CMD_DRAIN_OUTPUT, buf, &len, 60.0);
    fprintf(stderr, "INFO: Esperando a la impresora / Waiting for the printer...\n");
    for (i = 0; i < FW_BOOT_WAIT && !Canceled; ++i)
	sleep(1);

    for (i = 0; i < 6 && !Canceled; ++i)
    {
	state = firmware_loaded();
	if (state == 1)
	{
	    fprintf(stderr, "INFO: Firmware cargado / Firmware loaded\n");
	    return 0;
	}
	if (state < 0)
	    return 0;
	sleep(3);
    }
    fprintf(stderr, "DEBUG: Printer did not report FWVER, continuing\n");
    return 0;
}

static void
load_settings(const cups_page_header2_t *h, int num_options,
	      cups_option_t *options)
{
    const char *val;

    if (h->cupsInteger[1] >= 1 && h->cupsInteger[1] <= 5)
	Density = (int) h->cupsInteger[1];
    EconoMode = h->cupsInteger[2] != 0;
    if (h->cupsInteger[3] <= FW_NEVER)
	FirmwareMode = (int) h->cupsInteger[3];

    /* Explicit job options win over the raster header */
    if ((val = cupsGetOption("HPDensity", num_options, options)) != NULL)
    {
	int d = atoi(val + strcspn(val, "0123456789"));

	if (d >= 1 && d <= 5)
	    Density = d;
    }
    if ((val = cupsGetOption("HPEconoMode", num_options, options)) != NULL)
	EconoMode = !strcasecmp(val, "On") || !strcasecmp(val, "True")
		    || !strcmp(val, "1");
    if ((val = cupsGetOption("HPFirmware", num_options, options)) != NULL)
    {
	if (!strcasecmp(val, "Always"))
	    FirmwareMode = FW_ALWAYS;
	else if (!strcasecmp(val, "Never"))
	    FirmwareMode = FW_NEVER;
	else
	    FirmwareMode = FW_AUTO;
    }
}

int
main(int argc, char *argv[])
{
    int			fd = 0;
    int			num_options;
    cups_option_t	*options = NULL;
    cups_raster_t	*ras;
    cups_page_header2_t	header;
    struct sigaction	action;
    int			started = 0;
    int			failed = 0;

    setbuf(stderr, NULL);

    if (argc < 6 || argc > 7)
    {
	fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
		argv[0]);
	return 1;
    }
    if (argc == 7 && (fd = open(argv[6], O_RDONLY)) == -1)
    {
	perror("ERROR: Unable to open raster file");
	return 1;
    }

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = cancel_job;
    sigaction(SIGTERM, &action, NULL);

    num_options = cupsParseOptions(argv[5], 0, &options);
    init_dither();

    ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

    while (!Canceled && cupsRasterReadHeader2(ras, &header))
    {
	/* Reject a bad page before touching the printer */
	if (check_header(&header) < 0)
	{
	    failed = 1;
	    break;
	}
	if (!started)
	{
	    load_settings(&header, num_options, options);
	    fprintf(stderr, "DEBUG: %ux%u dpi, %u bpp, cspace %u, density %d, "
		    "econo %d, firmware %d\n",
		    header.HWResolution[0], header.HWResolution[1],
		    header.cupsBitsPerPixel, header.cupsColorSpace,
		    Density, EconoMode, FirmwareMode);

	    if (FirmwareMode == FW_ALWAYS
		|| (FirmwareMode == FW_AUTO && firmware_loaded() != 1))
	    {
		if (send_firmware(stdout) < 0)
		    return 1;
	    }
	    start_doc(stdout);
	    started = 1;
	}
	if (print_page(ras, &header, stdout) < 0)
	{
	    failed = 1;
	    break;
	}
    }

    if (started)
	end_doc(stdout);
    fflush(stdout);

    cupsRasterClose(ras);
    cupsFreeOptions(num_options, options);
    if (fd != 0)
	close(fd);

    if (!started && !failed)
	fputs("ERROR: No pages found\n", stderr);
    return (!started || failed || Canceled) ? 1 : 0;
}
