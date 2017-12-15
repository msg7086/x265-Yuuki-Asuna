/*****************************************************************************
 * Copyright (C) 2013-2015 x265 project
 *
 * Authors: Xinyue Lu <i@7086.in>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#ifdef ENABLE_ZIMG
#include "zimgfilter.h"

using namespace X265_NS;
using namespace std;

#if _WIN32
#define strcasecmp _stricmp
#endif

const char* Resizers[] = {"point", "bilinear", "bicubic", "spline16", "spline36", "lanczos"};

uint32_t mod4(uint32_t size)
{
    if (size % 4 == 0)
        return size;
    int denom = 16;
    while (denom >= 4)
    {
        int reminder = size % denom;
        if (reminder <= (denom >> 2)) return size - reminder;
        reminder -= denom;
        if (reminder >= -(denom >> 2)) return size - reminder;
        denom >>= 1;
    }
    /* If it's still non-mod4, cut it */
    return size - size % 4;
}

ZimgFilter::ZimgFilter(char* paramString)
{
    // zimg:crop(a,b,c,d)lanczos(a,b)
    cLeft = cRight = cTop = cBottom = 0.;
    rWidth = rHeight = 0;
    resizer = -1;
    param1 = param2 = NAN;
    bFail = false;
    resizeCtx[0] = NULL;
    depthCtx = NULL;
    planes[0] = NULL;
    upconvBuffer[0] = NULL;
    resizeBuffer[0] = NULL;
    temp = NULL;

    char* begin = paramString;
    char* end = paramString + strlen(paramString);
    char* p = begin;

    while (p < end)
    {
        char pName[1024];
        char pValue[1024];
        int length;
        // Scan (
        while (p[0] != '(' && p < end) p++;
        length = p - begin;
        pName[length] = 0;
        strncpy(pName, begin, length);
        p = begin = p + 1;

        // Scan )
        while (p[0] != ')' && p < end) p++;
        length = p - begin;
        pValue[length] = 0;
        strncpy(pValue, begin, length);
        p = begin = p + 1;

        if (!pName[0])
            continue;
        if (!strcasecmp(pName, "crop"))
        {
            double dLeft, dTop, dRight, dBottom;
            int count = sscanf(pValue, "%lf,%lf,%lf,%lf", &dLeft, &dTop, &dRight, &dBottom);
            if (count < 4)
            {
                general_log(NULL, "zimg", X265_LOG_ERROR, "Crop: invalid parameters: (%s), should be (L,T,W,H) or (L,T,-R,-B)\n", pValue);
                bFail = true;
                return;
            }
            cLeft   = static_cast<int>(1024 * dLeft);
            cTop    = static_cast<int>(1024 * dTop);
            cRight  = static_cast<int>(1024 * dRight);
            cBottom = static_cast<int>(1024 * dBottom);
            continue;
        }
        for (unsigned int i = 0; i < sizeof(Resizers) / sizeof(char*); i++)
            if (!strcasecmp(pName, Resizers[i]))
            {
                resizer = i;
                break;
            }
        if (resizer < 0)
        {
            // Unknown keyword
            general_log(NULL, "zimg", X265_LOG_ERROR, "Unknown keyword: %s\n", pName);
            bFail = true;
            return;
        }
        int count = sscanf(pValue, "%d,%d,%lf,%lf", &rWidth, &rHeight, &param1, &param2);
        if (count < 2)
        {
            general_log(NULL, "zimg", X265_LOG_ERROR, "Resize: invalid parameters: (%s), should be (W,H[,P1,P2])\n", pValue);
            bFail = true;
            return;
        }
    }
}

void ZimgFilter::setParam(x265_param* xParam)
{
    bool doCrop = cLeft != 0 || cRight != 0 || cTop != 0 || cBottom != 0;
    bool doResize = rWidth != 0 || rHeight != 0;
    byPass = !doCrop && !doResize;
    if (byPass)
    {
        general_log(xp, "zimg", X265_LOG_INFO, "Nothing to do. Bypassing\n");
        return;
    }
    xp = xParam;
    sWidth = xp->sourceWidth;
    sHeight = xp->sourceHeight;
    general_log(xp, "zimg", X265_LOG_INFO, "Input: %dx%d\n", sWidth, sHeight);
    if (cLeft < 0 || cTop < 0)
    {
        general_log(NULL, "zimg", X265_LOG_ERROR, "Crop: Left (%d) and Top (%d) must be non-negative\n", cLeft >> 10, cTop >> 10);
        bFail = true;
        return;
    }
    if (cRight <= 0 || cBottom <= 0)
    {
        if (cRight <= 0)
            cRight = (sWidth << 10) - cLeft + cRight;
        if (cBottom <= 0)
            cBottom = (sHeight << 10) - cTop + cBottom;
    }
    if (cRight <= 0 || cBottom <= 0)
    {
        general_log(NULL, "zimg", X265_LOG_ERROR, "Crop: Size after cropping (%dx%d) must be positive\n", cRight >> 10, cBottom >> 10);
        bFail = true;
        return;
    }
    if (doCrop)
    {
        if (cRight % 1024 == 0 && cBottom % 1024 == 0)
            general_log(xp, "zimg", X265_LOG_INFO, "Crop: %dx%d\n", cRight >> 10, cBottom >> 10);
        else
            general_log(xp, "zimg", X265_LOG_INFO, "Crop: %.2lfx%.2lf\n", cRight / 1024., cBottom / 1024.);
    }
    if (doResize)
    {
        if (rWidth == 0)
            rWidth = cRight * rHeight / cBottom;
        else if(rHeight == 0)
            rHeight = cBottom * rWidth / cRight;
    }
    else
    {
        rWidth = cRight >> 10;
        rHeight = cBottom >> 10;
    }
    /* We make sure it's at least mod 4 */
    uint32_t tWidth = mod4(rWidth);
    uint32_t tHeight = mod4(rHeight);
    if (tWidth != rWidth || tHeight != rHeight)
    {
        /* We'll resize */
        rWidth = tWidth;
        rHeight = tHeight;
        if (resizer < 0)
            resizer = ZIMG_RESIZE_LANCZOS;
        doResize = true;
    }
    if (resizer < 0)
        resizer = ZIMG_RESIZE_POINT;
    if (doResize)
        general_log(xp, "zimg", X265_LOG_INFO, "Resize: %dx%d\n", rWidth, rHeight);
    xp->sourceWidth = rWidth;
    xp->sourceHeight = rHeight;

    /* Now create core filter */
    char fail_str[1024];
    csp = xp->internalCsp;
    depthCtx = zimg_depth_create(ZIMG_DITHER_NONE);
    if (!depthCtx)
    {
        zimg_get_last_error(fail_str, sizeof(fail_str));
        general_log(NULL, "zimg", X265_LOG_ERROR, "Depth: %s\n", fail_str);
        bFail = true;
        return;
    }

    for (int i = 0; i < x265_cli_csps[csp].planes; i++)
    {
        int csWidth  = (int)(sWidth  >> x265_cli_csps[csp].width[i]);
        int crWidth  = (int)(rWidth  >> x265_cli_csps[csp].width[i]);
        int csHeight = (int)(sHeight >> x265_cli_csps[csp].height[i]);
        int crHeight = (int)(rHeight >> x265_cli_csps[csp].height[i]);
        double ccLeft   = cLeft   / (double)(1 << x265_cli_csps[csp].width[i]);
        double ccRight  = cRight  / (double)(1 << x265_cli_csps[csp].width[i]);
        double ccTop    = cTop    / (double)(1 << x265_cli_csps[csp].height[i]);
        double ccBottom = cBottom / (double)(1 << x265_cli_csps[csp].height[i]);

        resizeCtx[i] = zimg_resize_create(resizer,
                                          csWidth, csHeight, crWidth, crHeight,
                                          ccLeft / 1024., ccTop / 1024., ccRight / 1024., ccBottom / 1024.,
                                          param1, param2);
        if (!resizeCtx[i])
        {
            zimg_get_last_error(fail_str, sizeof(fail_str));
            general_log(NULL, "zimg", X265_LOG_ERROR, "Resizer: %s\n", fail_str);
            bFail = true;
            return;
        }
    }
}

void ZimgFilter::U16(x265_picture& picture)
{
    int pixelType = picture.bitDepth > 8 ? ZIMG_PIXEL_WORD : ZIMG_PIXEL_BYTE;
    if (!upconvBuffer[0])
    {
        // Create buffer for upconv
        for (int i = 0; i < x265_cli_csps[csp].planes; i++)
        {
            int w = sWidth  >> x265_cli_csps[csp].width[i];
            int h = sHeight >> x265_cli_csps[csp].height[i];
            upconvStride[i] = w * 2;
            upconvBuffer[i] = X265_MALLOC(uint16_t, h * w);
        }
    }
    int err = 0;
    char fail_str[1024];
    bool fullRange = xp->vui.bEnableVideoFullRangeFlag;
    for (int i = 0; i < x265_cli_csps[csp].planes; i++)
    {
        int w = sWidth  >> x265_cli_csps[csp].width[i];
        int h = sHeight >> x265_cli_csps[csp].height[i];
        err = zimg_depth_process(depthCtx,
            /* Planes      */    picture.planes[i], upconvBuffer[i],
            /* Temp buffer */    temp,
            /* Resolution  */    w, h,
            /* Stride      */    picture.stride[i], upconvStride[i],
            /* Pixel Type  */    pixelType,         ZIMG_PIXEL_WORD,
            /* Bitdepth    */    picture.bitDepth,  16,
            /* Range Flag  */    fullRange,         fullRange,
                                 i > 0);
        if (err)
        {
            zimg_get_last_error(fail_str, sizeof(fail_str));
            general_log(NULL, "zimg", X265_LOG_ERROR, "Upsample: %s\n", fail_str);
            bFail = true;
            return;
        }
    }
}

void ZimgFilter::R16(x265_picture&)
{
    if (!resizeBuffer[0])
    {
        // Create buffer for resize
        for (int i = 0; i < x265_cli_csps[csp].planes; i++)
        {
            int w = rWidth  >> x265_cli_csps[csp].width[i];
            int h = rHeight >> x265_cli_csps[csp].height[i];
            resizeStride[i] = w * 2;
            resizeBuffer[i] = X265_MALLOC(uint16_t, h * w);
        }
    }
    int err = 0;
    char fail_str[1024];
    for (int i = 0; i < x265_cli_csps[csp].planes; i++)
    {
        int sw = sWidth  >> x265_cli_csps[csp].width[i];
        int sh = sHeight >> x265_cli_csps[csp].height[i];
        int rw = rWidth  >> x265_cli_csps[csp].width[i];
        int rh = rHeight >> x265_cli_csps[csp].height[i];
        err = zimg_resize_process(resizeCtx[i],
            /* Planes      */     upconvBuffer[i],  resizeBuffer[i],
            /* Temp buffer */     temp,
            /* Resolution  */     sw, sh,           rw, rh,
            /* Stride      */     upconvStride[i],  resizeStride[i],
            /* Pixel Type  */     ZIMG_PIXEL_WORD);
        if (err)
        {
            zimg_get_last_error(fail_str, sizeof(fail_str));
            general_log(NULL, "zimg", X265_LOG_ERROR, "Resize: %s\n", fail_str);
            bFail = true;
            return;
        }
    }
}

void ZimgFilter::Oxx(x265_picture& picture)
{
    int OutputDepth = X265_DEPTH;
    int pixelSize = OutputDepth > 8 ? 2 : 1;
    int pixelType = OutputDepth > 8 ? ZIMG_PIXEL_WORD : ZIMG_PIXEL_BYTE;
    if (!planes[0])
    {
        // Create buffer for output
        for (int i = 0; i < x265_cli_csps[csp].planes; i++)
        {
            int w = rWidth  >> x265_cli_csps[csp].width[i];
            int h = rHeight >> x265_cli_csps[csp].height[i];
            stride[i] = w * pixelSize;
            planes[i] = x265_malloc(h * w * pixelSize);
        }
    }
    int err = 0;
    char fail_str[1024];
    bool fullRange = xp->vui.bEnableVideoFullRangeFlag;
    for (int i = 0; i < x265_cli_csps[csp].planes; i++)
    {
        int w = rWidth  >> x265_cli_csps[csp].width[i];
        int h = rHeight >> x265_cli_csps[csp].height[i];
        err = zimg_depth_process(depthCtx,
            /* Planes      */    resizeBuffer[i],   planes[i],
            /* Temp buffer */    temp,
            /* Resolution  */    w, h,
            /* Stride      */    resizeStride[i],   stride[i],
            /* Pixel Type  */    ZIMG_PIXEL_WORD,   pixelType,
            /* Bitdepth    */    16,                OutputDepth,
            /* Range Flag  */    fullRange,         fullRange,
                                 i > 0);
        if (err)
        {
            zimg_get_last_error(fail_str, sizeof(fail_str));
            general_log(NULL, "zimg", X265_LOG_ERROR, "Downsample: %s\n", fail_str);
            bFail = true;
            return;
        }
    }
    memcpy(picture.stride, stride, sizeof(stride));
    memcpy(picture.planes, planes, sizeof(planes));
    picture.bitDepth = OutputDepth;
}

void ZimgFilter::processFrame(x265_picture& picture)
{
    if (byPass)
        return;
    if (!temp)
    {
        int width = sWidth > rWidth ? sWidth : rWidth;
        int tempSize = zimg_depth_tmp_size(depthCtx, width << 1);
        for (int i = 0; i < x265_cli_csps[csp].planes; i++)
        {
            int size = zimg_resize_tmp_size(resizeCtx[i], ZIMG_PIXEL_WORD);
            if (size > tempSize)
                tempSize = size;
        }
        temp = x265_malloc(tempSize);
    }

    if (!bFail) U16(picture);
    if (!bFail) R16(picture);
    if (!bFail) Oxx(picture);

    return;
}

void ZimgFilter::release()
{
    x265_free(temp);
    temp = NULL;

    if (depthCtx)
    {
        zimg_depth_delete(depthCtx);
        depthCtx = NULL;
    }
    for (int i = 2; i >= 0; i--)
    {
        if (resizeCtx[0])
        {
            zimg_resize_delete(resizeCtx[i]);
            resizeCtx[i] = NULL;
        }
        if (planes[0])
        {
            x265_free(planes[i]);
            planes[i] = NULL;
        }
        if (upconvBuffer[0])
        {
            x265_free(upconvBuffer[i]);
            upconvBuffer[i] = NULL;
        }
        if (resizeBuffer[0])
        {
            x265_free(resizeBuffer[i]);
            resizeBuffer[i] = NULL;
        }
    }
}

#endif
