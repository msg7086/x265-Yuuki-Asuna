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

#ifndef X265_ZIMG_H
#define X265_ZIMG_H

#include "x265.h"
#include "filters.h"
#include "zimg.h"
#include <cmath>
#include <cstring>

namespace X265_NS {

class ZimgFilter : public Filter
{
protected:
    /* Crop, AVISynth Syntax
     * Unit is 1/1024 of a pixel
     * If cRight and cBottom > 0, they indicate width and height
     * If < 0, they indicate right and bottom
     * To crop a 4 pixels edge from 640x480
     * One can specify 4,4,-4,-4 or 4,4,632,472
     */
    int cLeft, cRight, cTop, cBottom;
    /* WxH of Input */
    uint32_t sWidth, sHeight;
    /* WxH of Output */
    uint32_t rWidth, rHeight;
    int resizer;
    double param1, param2;
    x265_param* xp;
    bool bFail;
    bool byPass;
    int csp;
    zimg_resize_context* resizeCtx[3];
    zimg_depth_context* depthCtx;
    int stride[3];
    void* planes[3];
    void* temp;
    int upconvStride[3];
    void* upconvBuffer[3];
    int resizeStride[3];
    void* resizeBuffer[3];
    void U16(x265_picture&);
    void R16(x265_picture&);
    void Oxx(x265_picture&);
public:
    ZimgFilter(char*);
    ~ZimgFilter() {}
    void setParam(x265_param*);
    bool isFail() const { return bFail; }
    void release();
    void processFrame(x265_picture&);
};
}
//class Resize
//void resizeImage(x265_picture&, int picWidth, int picHeight, char* params);
#endif //X265_ZIMG_H
