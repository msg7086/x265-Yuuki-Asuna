/*****************************************************************************
 * avs.c: avisynth input
 *****************************************************************************
 * Copyright (C) 2020 Xinyue Lu
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
 *****************************************************************************/

#include "avs.h"

#define FAIL_IF_ERROR( cond, ... )\
if( cond )\
{\
    general_log( NULL, "avs+", X265_LOG_ERROR, __VA_ARGS__ );\
    b_fail = true;\
    return;\
}

using namespace X265_NS;

void AVSInput::load_avs()
{
    avs_open();
    if (!h->library)
        return;
    LOAD_AVS_FUNC(avs_clip_get_error);
    LOAD_AVS_FUNC(avs_create_script_environment);
    LOAD_AVS_FUNC(avs_delete_script_environment);
    LOAD_AVS_FUNC(avs_get_frame);
    LOAD_AVS_FUNC(avs_get_version);
    LOAD_AVS_FUNC(avs_get_video_info);
    LOAD_AVS_FUNC(avs_function_exists);
    LOAD_AVS_FUNC(avs_invoke);
    LOAD_AVS_FUNC(avs_release_clip);
    LOAD_AVS_FUNC(avs_release_value);
    LOAD_AVS_FUNC(avs_release_video_frame);
    LOAD_AVS_FUNC(avs_take_clip);

    LOAD_AVS_FUNC(avs_is_y8);
    LOAD_AVS_FUNC(avs_is_420);
    LOAD_AVS_FUNC(avs_is_422);
    LOAD_AVS_FUNC(avs_is_444);
    LOAD_AVS_FUNC(avs_bits_per_component);
    h->env = h->func.avs_create_script_environment(AVS_INTERFACE_26);
    return;
fail:
    avs_close();
}

void AVSInput::info_avs()
{
    if (!h->func.avs_function_exists(h->env, "VersionString"))
        return;
    AVS_Value ver = h->func.avs_invoke(h->env, "VersionString", avs_new_value_array(NULL, 0), NULL);
    if(avs_is_error(ver))
        return;
    if(!avs_is_string(ver))
        return;
    const char *version = avs_as_string(ver);
    h->func.avs_release_value(ver);
    general_log(NULL, "avs+", X265_LOG_INFO, "%s\n", version);
}

void AVSInput::openfile(InputFileInfo& info)
{
#ifdef _WIN32
    wchar_t filename_wc[BUFFER_SIZE * 4];
    MultiByteToWideChar(CP_UTF8, 0, real_filename, -1, filename_wc, BUFFER_SIZE);
    WideCharToMultiByte(CP_THREAD_ACP, 0, filename_wc, -1, real_filename, BUFFER_SIZE, NULL, NULL);
#endif
    AVS_Value res = h->func.avs_invoke(h->env, "Import", avs_new_value_string(real_filename), NULL);
    FAIL_IF_ERROR(avs_is_error(res), "Error loading file: %s\n", avs_as_string(res));
    FAIL_IF_ERROR(!avs_is_clip(res), "File didn't return a video clip\n");
    h->clip = h->func.avs_take_clip(res, h->env);
    const AVS_VideoInfo* vi = h->func.avs_get_video_info(h->clip);
    info.width = vi->width;
    info.height = vi->height;
    info.fpsNum = vi->fps_numerator;
    info.fpsDenom = vi->fps_denominator;
    info.frameCount = vi->num_frames;
    info.depth = h->func.avs_bits_per_component(vi);
    h->plane_count = 3;
    if(h->func.avs_is_y8(vi))
    {
        h->plane_count = 1;
        info.csp = X265_CSP_I400;
        general_log(NULL, "avs+", X265_LOG_INFO, "Video colorspace: YUV400 (Y8)\n");
    }
    else if(h->func.avs_is_420(vi))
    {
        info.csp = X265_CSP_I420;
        general_log(NULL, "avs+", X265_LOG_INFO, "Video colorspace: YUV420 (YV12)\n");
    }
    else if(h->func.avs_is_422(vi))
    {
        info.csp = X265_CSP_I422;
        general_log(NULL, "avs+", X265_LOG_INFO, "Video colorspace: YUV422 (YV16)\n");
    }
    else if(h->func.avs_is_444(vi))
    {
        info.csp = X265_CSP_I444;
        general_log(NULL, "avs+", X265_LOG_INFO, "Video colorspace: YUV444 (YV24)\n");
    }
    else
    {
        FAIL_IF_ERROR(1, "Video colorspace is not supported\n");
    }
    general_log(NULL, "avs+", X265_LOG_INFO, "Video depth: %d\n", info.depth);
    general_log(NULL, "avs+", X265_LOG_INFO, "Video resolution: %dx%d\n", info.width, info.height);
    general_log(NULL, "avs+", X265_LOG_INFO, "Video framerate: %d/%d\n", info.fpsNum, info.fpsDenom);
    general_log(NULL, "avs+", X265_LOG_INFO, "Video framecount: %d\n", info.frameCount);
    if (info.skipFrames)
        h->next_frame = info.skipFrames;
}

bool AVSInput::readPicture(x265_picture& pic)
{
    AVS_VideoFrame *frm = h->func.avs_get_frame(h->clip, h->next_frame);
    const char *err = h->func.avs_clip_get_error(h->clip);
    if (err)
    {
        general_log(NULL, "avs+", X265_LOG_ERROR, "%s occurred while reading frame %d\n", err, h->next_frame);
        b_fail = true;
        return false;
    }
    pic.width = _info.width;
    pic.height = _info.height;

    if (frame_size == 0 || frame_buffer == nullptr) {
        frame_size = frm->height * frm->pitch;
        if (h->plane_count > 1)
            frame_size += frm->heightUV * frm->pitchUV * 2;
        frame_buffer = reinterpret_cast<uint8_t*>(x265_malloc(frame_size));
    }
    pic.framesize = frame_size;

    uint8_t* ptr = frame_buffer;
    pic.planes[0] = ptr;
    pic.stride[0] = frm->pitch;
    memcpy(pic.planes[0], frm->vfb->data + frm->offset, frm->pitch * frm->height);
    if (h->plane_count > 1)
    {
        ptr += frm->pitch * frm->height;
        pic.planes[1] = ptr;
        pic.stride[1] = frm->pitchUV;
        memcpy(pic.planes[1], frm->vfb->data + frm->offsetU, frm->pitchUV * frm->heightUV);

        ptr += frm->pitchUV * frm->heightUV;
        pic.planes[2] = ptr;
        pic.stride[2] = frm->pitchUV;
        memcpy(pic.planes[2], frm->vfb->data + frm->offsetV, frm->pitchUV * frm->heightUV);
    }
    pic.colorSpace = _info.csp;
    pic.bitDepth = _info.depth;

    h->func.avs_release_video_frame(frm);

    h->next_frame++;
    return true;
}

void AVSInput::release()
{
    if (h->clip)
        h->func.avs_release_clip(h->clip);
    if (h->env)
        h->func.avs_delete_script_environment(h->env);
    if (h->library)
        avs_close();
}
