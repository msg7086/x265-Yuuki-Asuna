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

#ifdef ENABLE_MKV

#include "mkv.h"

using namespace std;

#define ERR(...) general_log(NULL, "mkv", X265_LOG_ERROR, __VA_ARGS__)

namespace x265 {
/*******************/

int MKVOutput::openFile(const char *psz_filename)
{
    p_mkv = &mkvStorage;
    p_mkv->w = mk_create_writer(psz_filename);
    if (!p_mkv->w)
    {
        ERR("Error from mk_create_writer:!\n");
        b_fail = true;
        return -1;
    }
    return 0;
}

void MKVOutput::closeFile(int64_t largest_pts, int64_t second_largest_pts)
{
    int64_t i_last_delta;

    i_last_delta = p_mkv->i_timebase_den ? (int64_t)(((largest_pts - second_largest_pts) * p_mkv->i_timebase_num / p_mkv->i_timebase_den) + 0.5) : 0;
    mk_close(p_mkv->w, i_last_delta);
}

void MKVOutput::setParam(x265_param* p_param)
{
    int64_t dw, dh;

    p_param->bAnnexB = false;
    p_param->bRepeatHeaders = false;

    p_mkv->frame_duration = (int64_t)p_param->fpsDenom * (int64_t)1e9 / p_param->fpsNum;

    dw = p_mkv->width = p_param->sourceWidth;
    dh = p_mkv->height = p_param->sourceHeight;
    p_mkv->display_size_units = DS_PIXELS;
    p_mkv->stereo_mode = -1;

    if (p_param->vui.sarWidth && p_param->vui.sarHeight
        && p_param->vui.sarWidth != p_param->vui.sarHeight)
    {
        double sar = (double)p_param->vui.sarWidth / p_param->vui.sarHeight;
        if (sar > 1.0)
            dw *= sar;
        else
            dh /= sar;
    }
    p_mkv->d_width = static_cast<int>(dw);
    p_mkv->d_height = static_cast<int>(dh);

    p_mkv->i_timebase_num = info.timebaseNum;
    p_mkv->i_timebase_den = info.timebaseDenom;

    bEnableWavefront = p_param->bEnableWavefront;

    i_numframe = 0;
    p_mkv->b_writing_frame = 0;
}

void MKVOutput::setPS(x265_encoder* encoder)
{
    Encoder* enc = static_cast<Encoder*>(encoder);
    m_ptl = enc->m_vps.ptl;
    m_sps = enc->m_sps;
}

int MKVOutput::writeHeaders(const x265_nal* p_nal, uint32_t nalcount)
{
    if (nalcount < 3)
    {
        ERR("header should contain 3+ nals\n");
        b_fail = true;
        return -1;
    }
    int vps_size = p_nal[0].sizeBytes - 4;
    int sps_size = p_nal[1].sizeBytes - 4;
    int pps_size = p_nal[2].sizeBytes - 4;
    int sei_size = nalcount >= 4 ? p_nal[3].sizeBytes - 4 : 0;

    uint8_t *vps = p_nal[0].payload + 4;
    uint8_t *sps = p_nal[1].payload + 4;
    uint8_t *pps = p_nal[2].payload + 4;
    uint8_t *sei = nalcount >= 4 ? p_nal[3].payload + 4 : NULL;

    int ret;
    int hevcC_len;

    if (!p_mkv->width || !p_mkv->height ||
        !p_mkv->d_width || !p_mkv->d_height)
        return -1;

    hevcC_len = 23 + 5 + vps_size + 5 + sps_size + 5 + pps_size + (nalcount >= 4 ? 5 + sei_size : 0);
    uint8_t hevcC[hevcC_len];
    uint8_t *phc;
    phc = hevcC;

    // Value                               Bits  Description
    // -----                               ----  -----------
    // configuration_version               8     1
    *(phc++) = 1;
    // general_profile_space               2     Specifies the context for the interpretation of general_profile_idc and  general_profile_compatibility_flag
    // general_tier_flag                   1     Specifies the context for the interpretation of general_level_idc
    // general_profile_idc                 5     Defines the profile of the bitstream
    *(phc++) = (m_ptl.tierFlag & 1) << 5
        | (m_ptl.profileIdc & 0x1f);
    // general_profile_compatibility_flag  32    Defines profile compatibility, see [2] for interpretation
    for (int j = 0; j < 4; j++)
    {
        uint8_t flag = 0;
        for (int i = 0; i < 8; i++)
        {
            flag = (flag << 1) | m_ptl.profileCompatibilityFlag[j * 8 + i];
        }

        *(phc++) = flag;
    }

    // general_progressive_source_flag     1     Source is progressive, see [2] for interpretation.
    // general_interlace_source_flag       1     Source is interlaced, see [2] for interpretation.
    // general_nonpacked_constraint_flag   1     If 1 then no frame packing arrangement SEI messages, see [2] for more information
    // general_frame_only_constraint_flag  1     If 1 then no fields, see [2] for interpretation
    *(phc++) = (m_ptl.progressiveSourceFlag   & 1) << 7
        | (m_ptl.interlacedSourceFlag    & 1) << 6
        | (m_ptl.nonPackedConstraintFlag & 1) << 5
        | (m_ptl.frameOnlyConstraintFlag & 1) << 4;
    // reserved                            44    Reserved field, value TBD 0
    *(phc++) = 0;
    *(phc++) = 0;
    *(phc++) = 0;
    *(phc++) = 0;
    *(phc++) = 0;
    // general_level_idc                   8     Defines the level of the bitstream
    *(phc++) = m_ptl.levelIdc;
    // reserved                            4     Reserved Field, value '1111'b
    *(phc++) = 0xf0;
    // min_spatial_segmentation_idc        12    Maximum possible size of distinct coded spatial segmentation regions in the pictures of the CVS
    *(phc++) = 0;
    // reserved                            6     Reserved Field, value '111111'b
    // parallelism_type                    2     0=unknown, 1=slices, 2=tiles, 3=WPP
    *(phc++) = (bEnableWavefront ? 3 : 0) | 0xfc; // According to mkvtoolnix, FIXME
    // reserved                            6     Reserved field, value '111111'b
    // chroma_format_idc                   2     See table 6-1, HEVC
    *(phc++) = (m_sps.chromaFormatIdc & 0x3) | 0xfc;
    // reserved                            5     Reserved Field, value '11111'b
    // bit_depth_luma_minus8               3     Bit depth luma minus 8
    *(phc++) = (X265_DEPTH - 8) | 0xf8;
    // reserved                            5     Reserved Field, value '11111'b
    // bit_depth_chroma_minus8             3     Bit depth chroma minus 8
    *(phc++) = (X265_DEPTH - 8) | 0xf8;
    // reserved                            16    Reserved Field, value 0
    *(phc++) = 0;
    *(phc++) = 0;
    // reserved                            2     Reserved Field, value 0
    // max_sub_layers                      3     maximum number of temporal sub-layers
    // temporal_id_nesting_flag            1     Specifies whether inter prediction is additionally restricted. see [2] for interpretation.
    // size_nalu_minus_one                 2     Size of field NALU Length – 1
    *(phc++) = ((m_sps.maxTempSubLayers - 1) & 7) << 3
        | (m_sps.maxTempSubLayers == 1 ? 1 : 0) << 2
        | 3;
    // num_parameter_sets                  8     Number of parameter sets
    *(phc++) = nalcount;

    // -- VPS --
    // 0b01UUUUUU
    *(phc++) = 0x40 | NAL_UNIT_VPS; // NAL_* & 0x3f;
    // 2 byte count
    *(phc++) = 0;
    *(phc++) = 1;
    // 2 byte size
    *(phc++) = vps_size >> 8;
    *(phc++) = vps_size & 0xff;
    // data
    memcpy(phc, vps, vps_size);
    phc += vps_size;

    // -- SPS --
    // 0b01UUUUUU
    *(phc++) = 0x40 | NAL_UNIT_SPS; // NAL_* & 0x3f;
    // 2 byte count
    *(phc++) = 0;
    *(phc++) = 1;
    // 2 byte size
    *(phc++) = sps_size >> 8;
    *(phc++) = sps_size & 0xff;
    // data
    memcpy(phc, sps, sps_size);
    phc += sps_size;

    // -- PPS --
    // 0b01UUUUUU
    *(phc++) = 0x40 | NAL_UNIT_PPS; // NAL_* & 0x3f;
    // 2 byte count
    *(phc++) = 0;
    *(phc++) = 1;
    // 2 byte size
    *(phc++) = pps_size >> 8;
    *(phc++) = pps_size & 0xff;
    // data
    memcpy(phc, pps, pps_size);
    phc += pps_size;

    if (nalcount >= 4)
    {
        // -- User Data SEI --
        // 0b01UUUUUU
        *(phc++) = 0x40 | NAL_UNIT_PREFIX_SEI; // NAL_* & 0x3f;
        // 2 byte count
        *(phc++) = 0;
        *(phc++) = 1;
        // 2 byte size
        *(phc++) = sei_size >> 8;
        *(phc++) = sei_size & 0xff;
        // data
        memcpy(phc, sei, sei_size);
        phc += sei_size;
    }

    ret = mk_write_header(p_mkv->w, "x265", "V_MPEGH/ISO/HEVC",
                          hevcC, hevcC_len, p_mkv->frame_duration, 50000,
                          p_mkv->width, p_mkv->height,
                          p_mkv->d_width, p_mkv->d_height, p_mkv->display_size_units, p_mkv->stereo_mode);
    if (ret < 0)
    {
        ERR("Error from mk_write_header: %d !\n", ret);
        return ret;
    }

    if (nalcount < 4)
        return hevcC_len;

    // SEI

    if (!p_mkv->b_writing_frame)
    {
        if (mk_start_frame(p_mkv->w) < 0)
        {
            ERR("Error from mk_start_frame!\n");
            return -1;
        }
        p_mkv->b_writing_frame = 1;
    }

    if (mk_add_frame_data(p_mkv->w, p_nal[3].payload, p_nal[3].sizeBytes) < 0)
    {
        ERR("Error from mk_add_frame_data!\n");
        return -1;
    }

    return hevcC_len + p_nal[3].sizeBytes;
}

int MKVOutput::writeFrame(const x265_nal* p_nalu, uint32_t nalcount, x265_picture& pic)
{
    const bool b_keyframe = pic.sliceType == X265_TYPE_IDR;
    const bool b_bframe = pic.sliceType == X265_TYPE_B;

    if (!p_mkv->b_writing_frame)
    {
        if (mk_start_frame(p_mkv->w) < 0)
        {
            ERR("Error from mk_start_frame!\n");
            return -1;
        }
        p_mkv->b_writing_frame = 1;
    }
    int totalBytes = 0;

    for (uint32_t i = 0; i < nalcount; i++)
    {
        if (mk_add_frame_data(p_mkv->w, p_nalu[i].payload, p_nalu[i].sizeBytes) < 0)
        {
            ERR("Error from mk_add_frame_data!\n");
            return -1;
        }
        totalBytes += p_nalu[i].sizeBytes;
    }

    int64_t i_stamp = (int64_t)((pic.pts * 1e9 * p_mkv->i_timebase_num / p_mkv->i_timebase_den) + 0.5);

    p_mkv->b_writing_frame = 0;

    if (mk_set_frame_flags(p_mkv->w, i_stamp, b_keyframe, b_bframe) < 0)
    {
        ERR("Error from mk_set_frame_flags!\n");
        return -1;
    }

    return totalBytes;
}
}

#endif // ifdef ENABLE_MKV
