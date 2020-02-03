/*****************************************************************************
 * Copyright (C) 2013-2020 x265 project
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

uint32_t round_up_64(uint32_t size)
{
    if ((size & 63) == 0) return size;
    return size - (size & 63) + 64;
}

ZimgFilter::ZimgFilter(char* paramString)
{
    // zimg:crop(a,b,c,d)lanczos(a,b)
    cLeft = cRight = cTop = cBottom = 0;
    rWidth = rHeight = 0;
    resizer = -1;
    param1 = param2 = NAN;
    bFail = false;
    graph = NULL;
    planes[0] = NULL;
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

    zimg_image_format_default(&src_format, ZIMG_API_VERSION);
    zimg_image_format_default(&dst_format, ZIMG_API_VERSION);
    zimg_graph_builder_params_default(&graph_params, ZIMG_API_VERSION);

    src_format.width  = (int)sWidth;
    dst_format.width  = (int)rWidth;
    src_format.height = (int)sHeight;
    dst_format.height = (int)rHeight;

    csp = xp->internalCsp;
    if (x265_cli_csps[csp].planes > 1)
    {
        src_format.subsample_w =
        dst_format.subsample_w = x265_cli_csps[csp].width[1];
        src_format.subsample_h =
        dst_format.subsample_h = x265_cli_csps[csp].height[1];
    }

    src_format.active_region.left = cLeft / 1024.;
    src_format.active_region.top = cTop / 1024.;
    src_format.active_region.width = cRight / 1024.;
    src_format.active_region.height = cBottom / 1024.;

    graph_params.resample_filter_uv =
    graph_params.resample_filter = (zimg_resample_filter_e)resizer;
    graph_params.filter_param_a_uv =
    graph_params.filter_param_a = param1;
    graph_params.filter_param_b_uv =
    graph_params.filter_param_b = param2;
}

void ZimgFilter::processFrame(x265_picture& picture)
{
    if (byPass) return;
    if (bFail) return;

    int err = 0;
    char fail_str[1024];
    int OutputDepth = X265_DEPTH;
    if (!graph) // Init
    {
        int pixelSize = OutputDepth > 8 ? 2 : 1;
        src_format.depth = picture.bitDepth;
        dst_format.depth = OutputDepth;
        src_format.pixel_type = picture.bitDepth > 8 ? ZIMG_PIXEL_WORD : ZIMG_PIXEL_BYTE;
        dst_format.pixel_type = OutputDepth > 8 ? ZIMG_PIXEL_WORD : ZIMG_PIXEL_BYTE;

        switch (picture.colorSpace)
        {
        case X265_CSP_BGR:
        case X265_CSP_BGRA:
        case X265_CSP_RGB:
            src_format.color_family = dst_format.color_family = ZIMG_COLOR_RGB;
            break;
        case X265_CSP_I400:
            src_format.color_family = dst_format.color_family = ZIMG_COLOR_GREY;
            break;
        default:
            src_format.color_family = dst_format.color_family = ZIMG_COLOR_YUV;
            break;
        }
        src_format.pixel_range =
        dst_format.pixel_range = xp->vui.bEnableVideoFullRangeFlag ? ZIMG_RANGE_FULL : ZIMG_RANGE_LIMITED;

        // Create buffer for resize
        for (int i = 0; i < x265_cli_csps[csp].planes; i++)
        {
            int w = rWidth  >> x265_cli_csps[csp].width[i];
            int h = rHeight >> x265_cli_csps[csp].height[i];
            stride[i] = round_up_64(w * pixelSize);
            planes[i] = x265_malloc(h * stride[i]);
        }

        graph = zimg_filter_graph_build(&src_format, &dst_format, &graph_params);
        if (!graph)
        {
            zimg_get_last_error(fail_str, sizeof(fail_str));
            general_log(NULL, "zimg", X265_LOG_ERROR, "Init: %s\n", fail_str);
            bFail = true;
            return;
        }
        // Create temp buffer
        size_t tmp_size;
        err = zimg_filter_graph_get_tmp_size(graph, &tmp_size);
        if (err)
        {
            zimg_get_last_error(fail_str, sizeof(fail_str));
            general_log(NULL, "zimg", X265_LOG_ERROR, "Init: %s\n", fail_str);
            bFail = true;
            return;
        }
        temp = x265_malloc(tmp_size);
        if (!temp)
        {
            general_log(NULL, "zimg", X265_LOG_ERROR, "Init: error allocating memory for temp buffer\n");
            bFail = true;
            return;
        }
    }

    zimg_image_buffer_const src_buf = { ZIMG_API_VERSION };
    zimg_image_buffer dst_buf = { ZIMG_API_VERSION };

    for (int i = 0; i < x265_cli_csps[csp].planes; i++)
    {
        src_buf.plane[i].data = picture.planes[i];
        src_buf.plane[i].stride = picture.stride[i];
        src_buf.plane[i].mask = ZIMG_BUFFER_MAX;
        dst_buf.plane[i].data = planes[i];
        dst_buf.plane[i].stride = stride[i];
        dst_buf.plane[i].mask = ZIMG_BUFFER_MAX;
    }

    err = zimg_filter_graph_process(graph, &src_buf, &dst_buf, temp, 0, 0, 0, 0);
    if (err)
    {
        zimg_get_last_error(fail_str, sizeof(fail_str));
        general_log(NULL, "zimg", X265_LOG_ERROR, "Resize: %s\n", fail_str);
        bFail = true;
        return;
    }

    memcpy(picture.stride, stride, sizeof(stride));
    memcpy(picture.planes, planes, sizeof(planes));
    picture.bitDepth = OutputDepth;
}

void ZimgFilter::release()
{
    if (temp)
    {
        x265_free(temp);
        temp = NULL;
    }

    if (graph)
    {
        zimg_filter_graph_free(graph);
        graph = NULL;
    }
    if (planes[0])
    {
        for (int i = 2; i >= 0; i--)
        {
            x265_free(planes[i]);
            planes[i] = NULL;
        }
    }
}

#endif
