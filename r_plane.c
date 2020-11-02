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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//

#include <stdlib.h>

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"
#include "r_data.h"

#include <conio.h>

#define SC_INDEX 0x3C4

planefunction_t floorfunc;
planefunction_t ceilingfunc;

//
// opening
//

// Here comes the obnoxious "visplane".
#define MAXVISPLANES 128
visplane_t visplanes[MAXVISPLANES];
visplane_t *lastvisplane;
visplane_t *floorplane;
visplane_t *ceilingplane;

// ?
#define MAXOPENINGS SCREENWIDTH * 64
short openings[MAXOPENINGS];
short *lastopening;

//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short floorclip[SCREENWIDTH];
short ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int spanstart[SCREENHEIGHT];
int spanstop[SCREENHEIGHT];

//
// texture mapping
//
lighttable_t **planezlight;
fixed_t planeheight;

fixed_t yslope[SCREENHEIGHT];
fixed_t distscale[SCREENWIDTH];
fixed_t basexscale;
fixed_t baseyscale;

fixed_t cachedheight[SCREENHEIGHT];
fixed_t cacheddistance[SCREENHEIGHT];
fixed_t cachedxstep[SCREENHEIGHT];
fixed_t cachedystep[SCREENHEIGHT];

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes(void)
{
    // Doh!
}

//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
void R_MapPlane(int y, int x1)
{
    angle_t angle;
    fixed_t distance;
    fixed_t length;
    unsigned index;

    if (planeheight != cachedheight[y])
    {
        cachedheight[y] = planeheight;
        distance = cacheddistance[y] = FixedMul(planeheight, yslope[y]);
        if (!untexturedSurfaces)
        {
            ds_xstep = cachedxstep[y] = FixedMul(distance, basexscale);
            ds_ystep = cachedystep[y] = FixedMul(distance, baseyscale);
        }
    }
    else
    {
        distance = cacheddistance[y];
        if (!untexturedSurfaces)
        {
            ds_xstep = cachedxstep[y];
            ds_ystep = cachedystep[y];
        }
    }

    if (!untexturedSurfaces)
    {
        length = FixedMul(distance, distscale[x1]);
        angle = (viewangle + xtoviewangle[x1]) >> ANGLETOFINESHIFT;
        ds_xfrac = viewx + FixedMul(finecosine[angle], length);
        ds_yfrac = -viewy - FixedMul(finesine[angle], length);
    }

    if (fixedcolormap)
        ds_colormap = fixedcolormap;
    else
    {
        index = distance >> LIGHTZSHIFT;

        if (index >= MAXLIGHTZ)
            index = MAXLIGHTZ - 1;

        ds_colormap = planezlight[index];
    }

    ds_y = y;
    ds_x1 = x1;

    // high or low detail
    spanfunc();
}

//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes(void)
{
    int i;
    angle_t angle;

    // opening / clipping determination
    for (i = 0; i < viewwidth; i += 8)
    {
        floorclip[i] = viewheight;
        ceilingclip[i] = -1;
        floorclip[i + 1] = viewheight;
        ceilingclip[i + 1] = -1;
        floorclip[i + 2] = viewheight;
        ceilingclip[i + 2] = -1;
        floorclip[i + 3] = viewheight;
        ceilingclip[i + 3] = -1;
        floorclip[i + 4] = viewheight;
        ceilingclip[i + 4] = -1;
        floorclip[i + 5] = viewheight;
        ceilingclip[i + 5] = -1;
        floorclip[i + 6] = viewheight;
        ceilingclip[i + 6] = -1;
        floorclip[i + 7] = viewheight;
        ceilingclip[i + 7] = -1;
    }

    lastvisplane = visplanes;
    lastopening = openings;

    if (flatSurfaces || untexturedSurfaces)
    {
        return;
    }

    // texture calculation
    memset(cachedheight, 0, sizeof(cachedheight));

    // left to right mapping
    angle = (viewangle - ANG90) >> ANGLETOFINESHIFT;

    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedMul(finecosine[angle], iprojection) >> 8;
    baseyscale = -FixedMul(finesine[angle], iprojection) >> 8;
}

//
// R_FindPlane
//
visplane_t *
R_FindPlane(fixed_t height,
            int picnum,
            int lightlevel)
{
    visplane_t *check;

    if (picnum == skyflatnum)
    {
        height = 0; // all skys map together
        lightlevel = 0;
    }

    for (check = visplanes; check < lastvisplane; check++)
    {
        if (height == check->height && picnum == check->picnum && lightlevel == check->lightlevel)
        {
            break;
        }
    }

    if (check < lastvisplane)
        return check;

    lastvisplane++;

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;

    memset(check->top, 0xff, sizeof(check->top));

    check->modified = 0;

    return check;
}

//
// R_CheckPlane
//
visplane_t *
R_CheckPlane(visplane_t *pl,
             int start,
             int stop)
{
    int intrl;
    int intrh;
    int unionl;
    int unionh;
    int x;

    if (start < pl->minx)
    {
        intrl = pl->minx;
        unionl = start;
    }
    else
    {
        unionl = pl->minx;
        intrl = start;
    }

    if (stop > pl->maxx)
    {
        intrh = pl->maxx;
        unionh = stop;
    }
    else
    {
        unionh = pl->maxx;
        intrh = stop;
    }

    for (x = intrl; x <= intrh && pl->top[x] == 0xff; x++) // dropoff overflow
        ;

    if (x > intrh)
    {
        pl->minx = unionl;
        pl->maxx = unionh;

        // use the same one
        return pl;
    }

    // make a new visplane
    lastvisplane->height = pl->height;
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;

    pl = lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;

    memset(pl->top, 0xff, sizeof(pl->top));

    pl->modified = 0;

    return pl;
}

//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes(void)
{
    visplane_t *pl;
    int light;
    int x;
    int stop;
    int angle;

    int lump;
    int ofs;
    int tex;
    int col;

    byte t1, b1, t2, b2;

    for (pl = visplanes; pl < lastvisplane; pl++)
    {
        if (!pl->modified)
            continue;

        if (pl->minx > pl->maxx)
            continue;

        // sky flat
        if (pl->picnum == skyflatnum)
        {
            dc_iscale = pspriteiscale >> detailshift;

            // Sky is allways drawn full bright,
            //  i.e. colormaps[0] is used.
            // Because of this hack, sky is not affected
            //  by INVUL inverse mapping.
            //dc_colormap = colormaps;
            dc_colormap = fixedcolormap ? fixedcolormap : colormaps;
            dc_texturemid = skytexturemid;
            for (x = pl->minx; x <= pl->maxx; x++)
            {
                dc_yl = pl->top[x];
                dc_yh = pl->bottom[x];

                if (dc_yl > dc_yh)
                    continue;

                dc_x = x;

                if (!flatSky)
                {
                    angle = (viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT;

                    tex = skytexture;
                    col = angle;
                    col &= texturewidthmask[tex];
                    lump = texturecolumnlump[tex][col];
                    ofs = texturecolumnofs[tex][col];

                    if (lump > 0)
                    {
                        dc_source = (byte *)W_CacheLumpNum(lump, PU_CACHE) + ofs;
                    }
                    else
                    {
                        if (!texturecomposite[tex])
                            R_GenerateComposite(tex);

                        dc_source = texturecomposite[tex] + ofs;
                    }
                }

                skyfunc();
            }
            continue;
        }

        if (flatSurfaces)
        {
            int planeOffset;
            //dc_iscale = pspriteiscale >> detailshift;
            dc_colormap = colormaps;

            dc_source = W_CacheLumpNum(firstflat +
                                           flattranslation[pl->picnum],
                                       PU_STATIC);

            dc_flatcolor = dc_colormap[dc_source[0]];

            switch (detailshift)
            {
            case 0:
                // Plane 0
                x = pl->minx;

                //outp(SC_INDEX + 1, 1 << (x & 3));
                planeOffset = (x & 3) << 14;

                do
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                    {
                        x += 4;
                        continue;
                    }

                    dc_x = x;
                    x += 4;

                    R_DrawColumnFlat(planeOffset);
                } while (x <= pl->maxx);

                // Plane 1
                x = pl->minx + 1;

                if (x > pl->maxx)
                    continue;

                //outp(SC_INDEX + 1, 1 << (x & 3));
                planeOffset = (x & 3) << 14;

                do
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                    {
                        x += 4;
                        continue;
                    }

                    dc_x = x;
                    x += 4;

                    R_DrawColumnFlat(planeOffset);
                } while (x <= pl->maxx);

                // Plane 2
                x = pl->minx + 2;

                if (x > pl->maxx)
                    continue;

                //outp(SC_INDEX + 1, 1 << (x & 3));
                planeOffset = (x & 3) << 14;

                do
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                    {
                        x += 4;
                        continue;
                    }

                    dc_x = x;
                    x += 4;

                    R_DrawColumnFlat(planeOffset);
                } while (x <= pl->maxx);

                // Plane 3
                x = pl->minx + 3;

                if (x > pl->maxx)
                    continue;

                //outp(SC_INDEX + 1, 1 << (x & 3));
                planeOffset = (x & 3) << 14;

                do
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                    {
                        x += 4;
                        continue;
                    }

                    dc_x = x;
                    x += 4;

                    R_DrawColumnFlat(planeOffset);
                } while (x <= pl->maxx);

                break;
            case 1:
                // Plane 0
                x = pl->minx;

                //outp(SC_INDEX + 1, 3 << ((x & 1) << 1));
                planeOffset = (x & 1) << 14;

                do
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                    {
                        x += 2;
                        continue;
                    }

                    dc_x = x;
                    x += 2;

                    R_DrawColumnFlatLow(planeOffset);
                } while (x <= pl->maxx);

                // Plane 1
                x = pl->minx + 1;

                if (x > pl->maxx)
                    continue;

                //outp(SC_INDEX + 1, 3 << ((x & 1) << 1));
                planeOffset = (x & 1) << 14;

                do
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                    {
                        x += 2;
                        continue;
                    }

                    dc_x = x;
                    x += 2;

                    R_DrawColumnFlatLow(planeOffset);
                } while (x <= pl->maxx);

                break;
            case 2:
                for (x = pl->minx; x <= pl->maxx; x++)
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];

                    if (dc_yl > dc_yh)
                        continue;

                    dc_x = x;
                    R_DrawColumnFlatPotato();
                }
                break;
            };

            Z_ChangeTag(dc_source, PU_CACHE);
        }
        else
        {
            // regular flat

            ds_source = W_CacheLumpNum(firstflat +
                                           flattranslation[pl->picnum],
                                       PU_STATIC);
            planeheight = abs(pl->height - viewz);
            light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

            if (light >= LIGHTLEVELS)
                light = LIGHTLEVELS - 1;

            if (light < 0)
                light = 0;

            planezlight = zlight[light];
            pl->top[pl->maxx + 1] = 0xff;
            pl->top[pl->minx - 1] = 0xff;

            stop = pl->maxx + 1;

            for (x = pl->minx; x <= stop; x++)
            {
                t1 = pl->top[x - 1];
                b1 = pl->bottom[x - 1];
                t2 = pl->top[x];
                b2 = pl->bottom[x];

                ds_x2 = x - 1;

                while (t1 < t2 && t1 <= b1)
                {
                    R_MapPlane(t1, spanstart[t1]);
                    t1++;
                }
                while (b1 > b2 && b1 >= t1)
                {
                    R_MapPlane(b1, spanstart[b1]);
                    b1--;
                }

                while (t2 < t1 && t2 <= b2)
                {
                    spanstart[t2] = x;
                    t2++;
                }
                while (b2 > b1 && b2 >= t2)
                {
                    spanstart[b2] = x;
                    b2--;
                }
            }

            Z_ChangeTag(ds_source, PU_CACHE);
        }
    }
}
