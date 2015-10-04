/*****************************************************************************
 * Copyright (C) 2013-2015 x265 project
 *
 * Authors: Mike Matsnev <mike@haali.su>
 *          Xinyue Lu <i@7086.in>
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

#ifndef X265_HEVC_MKV_H
#define X265_HEVC_MKV_H

#include "output.h"
#include "common.h"
#include "encoder.h"
#include "matroska_ebml.h"

namespace x265 {
typedef struct
{
    mk_writer *w;

    int width, height, d_width, d_height;

    int display_size_units;
    int stereo_mode;

    int64_t frame_duration;

    char b_writing_frame;
    uint32_t i_timebase_num;
    uint32_t i_timebase_den;
} mkv_hnd_t;

class MKVOutput : public OutputFile
{
protected:

    bool b_fail;
    int openFile(const char *fname);
    mkv_hnd_t mkvStorage;
    mkv_hnd_t *p_mkv;
    int i_numframe;
    bool bEnableWavefront;
    InputFileInfo info;
    ProfileTierLevel m_ptl;
    SPS m_sps;

public:

    MKVOutput(const char *fname, InputFileInfo& inputInfo)
    {
        b_fail = false;
        info = inputInfo;
        if (openFile(fname) != 0)
            b_fail = true;
    }

    bool isFail() const
    {
        return b_fail;
    }

    bool needPTS() const { return true; }

    const char *getName() const { return "mkv"; }
    void setParam(x265_param*);
    void setPS(x265_encoder*);
    int writeHeaders(const x265_nal*, uint32_t);
    int writeFrame(const x265_nal*, uint32_t, x265_picture& pic);
    void closeFile(int64_t largest_pts, int64_t second_largest_pts);
    void release()
    {
        delete this;
    }
};
}

#endif // ifndef X265_HEVC_MKV_H
