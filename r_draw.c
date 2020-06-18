//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 2016-2017 Alexey Khokholov (Nuke.YKT)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//

#include <conio.h>
#include "doomdef.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"

// State.
#include "doomstat.h"

// ?
#define MAXWIDTH 320
#define MAXHEIGHT 200

// status bar height at bottom of screen
#define SBARHEIGHT 32

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//

byte *viewimage;
int viewwidth;
int scaledviewwidth;
int viewheight;
int viewwindowx;
int viewwindowy;
int columnofs[MAXWIDTH];

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//

#define SC_INDEX 0x3C4
#define SC_RESET 0
#define SC_CLOCK 1
#define SC_MAPMASK 2
#define SC_CHARMAP 3
#define SC_MEMMODE 4

#define GC_INDEX 0x3CE
#define GC_SETRESET 0
#define GC_ENABLESETRESET 1
#define GC_COLORCOMPARE 2
#define GC_DATAROTATE 3
#define GC_READMAP 4
#define GC_MODE 5
#define GC_MISCELLANEOUS 6
#define GC_COLORDONTCARE 7
#define GC_BITMASK 8

//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t *dc_colormap;
int dc_x;
int dc_yl;
int dc_yh;
fixed_t dc_iscale;
fixed_t dc_texturemid;

// first pixel in a column (possibly virtual)
byte *dc_source;

void R_DrawSkyFlat(void)
{
    register int count;
    register byte *dest;

    count = dc_yh - dc_yl;

    if (count < 0)
        return;

    outp(SC_INDEX + 1, 1 << (dc_x & 3));

    dest = destview + dc_yl * 80 + (dc_x >> 2);

    do
    {
        *dest = 220;
        dest += SCREENWIDTH / 4;
    } while (count--);
}

void R_DrawSkyFlatLow(void)
{
    register int count;
    register byte *dest;

    count = dc_yh - dc_yl;

    if (count < 0)
        return;

    if (dc_x & 1)
        outp(SC_INDEX + 1, 12);
    else
        outp(SC_INDEX + 1, 3);

    dest = destview + dc_yl * 80 + (dc_x >> 1);

    do
    {
        *dest = 220;
        dest += SCREENWIDTH / 4;
    } while (count--);
}

void R_DrawSpanPotato(void)
{
    int spot;
    int prt;
    int dsp_x1;
    int dsp_x2;
    int countp;
    fixed_t xfrac;
    fixed_t yfrac;
    byte *dest;

    dsp_x1 = ds_x1 >> 1;

    if (ds_x1 & 1)
        dsp_x1++;

    dsp_x2 = ds_x2 >> 1;

    countp = dsp_x2 - dsp_x1;

    if (countp < 0)
        return;

    dest = destview + ds_y * 80 + dsp_x1;
    outp(SC_INDEX + 1, 15);

    xfrac = ds_xfrac;
    yfrac = ds_yfrac;

    prt = (dsp_x1 << 1) - ds_x1;

    xfrac += ds_xstep * prt;
    yfrac += ds_ystep * prt;

    do
    {
        // Current texture index in u,v.
        spot = ((yfrac >> (16 - 6)) & (63 * 64)) + ((xfrac >> 16) & 63);

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        *dest++ = ds_colormap[ds_source[spot]];

        // Next step in u,v.
        xfrac += ds_xstep << 1;
        yfrac += ds_ystep << 1;
    } while (countp--);
}

//
// Spectre/Invisibility.
//
#define FUZZTABLE 50
#define FUZZOFF (SCREENWIDTH / 4)

int fuzzoffset[FUZZTABLE] =
    {
        FUZZOFF, -FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
        FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
        FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF,
        FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
        FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF,
        FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF,
        FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF};

int fuzzpos = 0;

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
void R_DrawFuzzColumn(void)
{
    register int count;
    register byte *dest;

    if (potatoDetail && (dc_x & 1))
        return;

    // Adjust borders. Low...
    if (!dc_yl)
        dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight - 1)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

    if (detailshift)
    {
        if (potatoDetail){
            outpw(GC_INDEX, GC_READMAP + (15 << 8));
            outp(SC_INDEX + 1, 15);
        }else{
            if (dc_x & 1)
            {
                outpw(GC_INDEX, GC_READMAP + (2 << 8));
                outp(SC_INDEX + 1, 12);
            }
            else
            {
                outpw(GC_INDEX, GC_READMAP);
                outp(SC_INDEX + 1, 3);
            }
        }
        dest = destview + dc_yl * 80 + (dc_x >> 1);
    }
    else
    {
        outpw(GC_INDEX, GC_READMAP + ((dc_x & 3) << 8));
        outp(SC_INDEX + 1, 1 << (dc_x & 3));
        dest = destview + dc_yl * 80 + (dc_x >> 2);
    }

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do
    {
        // Lookup framebuffer, and retrieve
        //  a pixel that is either one column
        //  left or right of the current one.
        // Add index from colormap to index.
        *dest = colormaps[6 * 256 + dest[fuzzoffset[fuzzpos]]];

        // Clamp table lookup index.
        if (++fuzzpos == FUZZTABLE)
            fuzzpos = 0;

        dest += SCREENWIDTH / 4;
    } while (count--);
}

void R_DrawFuzzColumnFast(void)
{
    register int count;
    register byte *dest;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

    if (detailshift)
    {
        if (dc_x & 1)
        {
            outpw(GC_INDEX, GC_READMAP + (2 << 8));
            outp(SC_INDEX + 1, 12);
        }
        else
        {
            outpw(GC_INDEX, GC_READMAP);
            outp(SC_INDEX + 1, 3);
        }
        dest = destview + dc_yl * 80 + (dc_x >> 1);
    }
    else
    {
        outpw(GC_INDEX, GC_READMAP + ((dc_x & 3) << 8));
        outp(SC_INDEX + 1, 1 << (dc_x & 3));
        dest = destview + dc_yl * 80 + (dc_x >> 2);
    }

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do
    {
        // Lookup framebuffer, and retrieve
        //  a pixel that is either one column
        //  left or right of the current one.
        // Add index from colormap to index.
        *dest = colormaps[6 * 256 + dest[0]];

        dest += SCREENWIDTH / 4;
    } while (count--);
}

void R_DrawFuzzColumnSaturn(void)
{
    int count;
    byte *dest;
    fixed_t frac;
    fixed_t fracstep;
    int initialdrawpos = 0;

    if (potatoDetail && (dc_x & 1))
        return;

    count = (dc_yh - dc_yl) / 2 - 1;

    // Zero length, column does not exceed a pixel.
    if (count < 0)
        return;

    initialdrawpos = dc_yl + dc_x;

    if (detailshift)
    {
        if (dc_x & 1)
            outp(SC_INDEX + 1, 12);
        else
            outp(SC_INDEX + 1, 3);

        dest = destview + dc_yl * 80 + (dc_x >> 1);
    }
    else
    {
        outp(SC_INDEX + 1, 1 << (dc_x & 3));

        dest = destview + dc_yl * 80 + (dc_x >> 2);
    }

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    if (initialdrawpos & 1)
    {
        dest += SCREENWIDTH / 4;
        frac += fracstep;
    }

    fracstep = 2 * fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do
    {
        // Re-map color indices from wall texture column
        //  using a lighting/special effects LUT.

        *dest = dc_colormap[dc_source[(frac >> FRACBITS) & 127]];

        dest += SCREENWIDTH / 2;
        frac += fracstep;

    } while (count--);

    if ((dc_yh - dc_yl) & 1)
    {
        *dest = dc_colormap[dc_source[(frac >> FRACBITS) & 127]];
    }
    else
    {
        if (!(initialdrawpos & 1))
        {
            *dest = dc_colormap[dc_source[(frac >> FRACBITS) & 127]];
        }
    }
}

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn(void)
{
    register int count;
    register byte *dest;
    fixed_t frac;
    fixed_t fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    if (detailshift)
    {
        if (dc_x & 1)
            outp(SC_INDEX + 1, 12);
        else
            outp(SC_INDEX + 1, 3);

        dest = destview + dc_yl * 80 + (dc_x >> 1);
    }
    else
    {
        outp(SC_INDEX + 1, 1 << (dc_x & 3));

        dest = destview + dc_yl * 80 + (dc_x >> 2);
    }

    // Looks familiar.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Here we do an additional index re-mapping.
    do
    {
        // Translation tables are used
        //  to map certain colorramps to other ones,
        //  used with PLAY sprites.
        // Thus the "green" ramp of the player 0 sprite
        //  is mapped to gray, red, black/indigo.
        *dest = dc_colormap[dc_translation[dc_source[frac >> FRACBITS]]];
        dest += SCREENWIDTH / 4;

        frac += fracstep;
    } while (count--);
}

//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables(void)
{
    int i;

    translationtables = Z_Malloc(256 * 3 + 255, PU_STATIC, 0);
    translationtables = (byte *)(((int)translationtables + 255) & ~255);

    // translate just the 16 green colors
    for (i = 0; i < 256; i++)
    {
        if (i >= 0x70 && i <= 0x7f)
        {
            // map green ramp to gray, brown, red
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        }
        else
        {
            // Keep all other colors as is.
            translationtables[i] = translationtables[i + 256] = translationtables[i + 512] = i;
        }
    }
}

//
// R_DrawSpan
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int ds_y;
int ds_x1;
int ds_x2;

lighttable_t *ds_colormap;

fixed_t ds_xfrac;
fixed_t ds_yfrac;
fixed_t ds_xstep;
fixed_t ds_ystep;

// start of a 64*64 tile image
byte *ds_source;

void R_DrawSpanFlat(void)
{
    register byte *dest;
    int dsp_x1;
    int dsp_x2;
    register int countp;

    lighttable_t color = ds_colormap[0][ds_source];
    int origin_y = (int)destview + ds_y * 80;

    dsp_x1 = (ds_x1) / 4;

    if (dsp_x1 * 4 < ds_x1)
        dsp_x1++;

    dsp_x2 = (ds_x2) / 4;

    countp = dsp_x2 - dsp_x1;

    if (countp >= 0)
    {
        dest = (byte *)origin_y + dsp_x1;
        outp(SC_INDEX + 1, 1 << 0);
        do
        {
            *dest++ = color;
        } while (countp--);
    }

    dsp_x1 = (ds_x1 - 1) / 4;

    if (dsp_x1 * 4 + 1 < ds_x1)
        dsp_x1++;

    dsp_x2 = (ds_x2 - 1) / 4;

    countp = dsp_x2 - dsp_x1;

    if (countp >= 0)
    {
        dest = (byte *)origin_y + dsp_x1;
        outp(SC_INDEX + 1, 1 << 1);
        do
        {
            *dest++ = color;
        } while (countp--);
    }

    dsp_x1 = (ds_x1 - 2) / 4;

    if (dsp_x1 * 4 + 2 < ds_x1)
        dsp_x1++;

    dsp_x2 = (ds_x2 - 2) / 4;

    countp = dsp_x2 - dsp_x1;

    if (countp >= 0)
    {
        dest = (byte *)origin_y + dsp_x1;
        outp(SC_INDEX + 1, 1 << 2);
        do
        {
            *dest++ = color;
        } while (countp--);
    }

    dsp_x1 = (ds_x1 - 3) / 4;

    if (dsp_x1 * 4 + 3 < ds_x1)
        dsp_x1++;

    dsp_x2 = (ds_x2 - 3) / 4;

    countp = dsp_x2 - dsp_x1;

    if (countp >= 0)
    {
        dest = (byte *)origin_y + dsp_x1;
        outp(SC_INDEX + 1, 1 << 3);
        do
        {
            *dest++ = color;
        } while (countp--);
    }
}

void R_DrawSpanFlatLow(void)
{
    register byte *dest;
    int dsp_x1;
    int dsp_x2;
    register int countp;

    lighttable_t color = ds_colormap[0][ds_source];
    int origin_y = (int)destview + ds_y * 80;

    outp(SC_INDEX + 1, 3 << 0);

    dsp_x1 = (ds_x1) / 2;

    if (dsp_x1 * 2 < ds_x1)
        dsp_x1++;

    dsp_x2 = (ds_x2) / 2;

    countp = dsp_x2 - dsp_x1;

    if (countp >= 0)
    {
        dest = (byte *)origin_y + dsp_x1;
        do
        {
            *dest++ = color;
        } while (countp--);
    }

    outp(SC_INDEX + 1, 3 << 2);

    dsp_x1 = (ds_x1 - 1) / 2;

    if (dsp_x1 * 2 < ds_x1 - 1)
        dsp_x1++;

    dsp_x2 = (ds_x2 - 1) / 2;

    countp = dsp_x2 - dsp_x1;

    if (countp >= 0)
    {
        dest = (byte *)origin_y + dsp_x1;
        do
        {
            *dest++ = color;
        } while (countp--);
    }
}

//
// R_InitBuffer
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void R_InitBuffer(int width,
                  int height)
{
    int i;

    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    viewwindowx = (SCREENWIDTH - width) >> 1;

    // Column offset. For windows.
    for (i = 0; i < width; i++)
        columnofs[i] = viewwindowx + i;

    // Samw with base row offset.
    if (width == SCREENWIDTH)
        viewwindowy = 0;
    else
        viewwindowy = (SCREENHEIGHT - SBARHEIGHT - height) >> 1;
}

//
// R_FillBackScreen
// Fills the back screen with a pattern
//  for variable screen sizes
// Also draws a beveled edge.
//
void R_FillBackScreen(void)
{
    byte *src;
    byte *dest;
    int x;
    int y;
    patch_t *patch;
    int i, count;

    // DOOM border patch.
    char name1[] = "FLOOR7_2";

    // DOOM II border patch.
    char name2[] = "GRNROCK";

    char *name;

    if (scaledviewwidth == 320)
        return;

    if (commercial)
        name = name2;
    else
        name = name1;

    src = W_CacheLumpName(name, PU_CACHE);
    dest = screens[1];

    for (y = 0; y < SCREENHEIGHT - SBARHEIGHT; y++)
    {
        for (x = 0; x < SCREENWIDTH / 64; x++)
        {
            memcpy(dest, src + ((y & 63) << 6), 64);
            dest += 64;
        }

        if (SCREENWIDTH & 63)
        {
            memcpy(dest, src + ((y & 63) << 6), SCREENWIDTH & 63);
            dest += (SCREENWIDTH & 63);
        }
    }

    patch = W_CacheLumpName("brdr_t", PU_CACHE);

    for (x = 0; x < scaledviewwidth; x += 8)
        V_DrawPatch(viewwindowx + x, viewwindowy - 8, 1, patch);
    patch = W_CacheLumpName("brdr_b", PU_CACHE);

    for (x = 0; x < scaledviewwidth; x += 8)
        V_DrawPatch(viewwindowx + x, viewwindowy + viewheight, 1, patch);
    patch = W_CacheLumpName("brdr_l", PU_CACHE);

    for (y = 0; y < viewheight; y += 8)
        V_DrawPatch(viewwindowx - 8, viewwindowy + y, 1, patch);
    patch = W_CacheLumpName("brdr_r", PU_CACHE);

    for (y = 0; y < viewheight; y += 8)
        V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy + y, 1, patch);

    // Draw beveled edge.
    V_DrawPatch(viewwindowx - 8,
                viewwindowy - 8,
                1,
                W_CacheLumpName("brdr_tl", PU_CACHE));

    V_DrawPatch(viewwindowx + scaledviewwidth,
                viewwindowy - 8,
                1,
                W_CacheLumpName("brdr_tr", PU_CACHE));

    V_DrawPatch(viewwindowx - 8,
                viewwindowy + viewheight,
                1,
                W_CacheLumpName("brdr_bl", PU_CACHE));

    V_DrawPatch(viewwindowx + scaledviewwidth,
                viewwindowy + viewheight,
                1,
                W_CacheLumpName("brdr_br", PU_CACHE));

    for (i = 0; i < 4; i++)
    {
        outp(SC_INDEX, SC_MAPMASK);
        outp(SC_INDEX + 1, 1 << i);

        dest = (byte *)0xac000;
        src = screens[1] + i;
        do
        {
            *dest++ = *src;
            src += 4;
        } while (dest != (byte *)(0xac000 + (SCREENHEIGHT - SBARHEIGHT) * SCREENWIDTH / 4));
    }
}

//
// Copy a screen buffer.
//
void R_VideoErase(unsigned ofs,
                  int count)
{
    byte *dest;
    byte *source;
    int countp;
    outp(SC_INDEX, SC_MAPMASK);
    outp(SC_INDEX + 1, 15);
    outp(GC_INDEX, GC_MODE);
    outp(GC_INDEX + 1, inp(GC_INDEX + 1) | 1);
    dest = destscreen + (ofs >> 2);
    source = (byte *)0xac000 + (ofs >> 2);
    countp = count / 4;
    while (--countp >= 0)
    {
        dest[countp] = source[countp];
    }

    outp(GC_INDEX, GC_MODE);
    outp(GC_INDEX + 1, inp(GC_INDEX + 1) & ~1);
}

//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void V_MarkRect(int x,
                int y,
                int width,
                int height);

void R_DrawViewBorder(void)
{
    int top;
    int side;
    int ofs;
    int i;

    if (scaledviewwidth == SCREENWIDTH)
        return;

    top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
    side = (SCREENWIDTH - scaledviewwidth) / 2;

    // copy top and one line of left side
    R_VideoErase(0, top * SCREENWIDTH + side);

    // copy one line of right side and bottom
    ofs = (viewheight + top) * SCREENWIDTH - side;
    R_VideoErase(ofs, top * SCREENWIDTH + side);

    // copy sides using wraparound
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    for (i = 1; i < viewheight; i++)
    {
        R_VideoErase(ofs, side);
        ofs += SCREENWIDTH;
    }

    // ?
    //V_MarkRect (0,0,SCREENWIDTH, SCREENHEIGHT-SBARHEIGHT);
}
