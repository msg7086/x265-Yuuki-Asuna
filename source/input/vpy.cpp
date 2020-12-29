/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Vladimir Kontserenko <djatom@beatrice-raws.org>
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

#include "vpy.h"
#include "common.h"

static void frameDoneCallback(void* userData, const VSFrameRef* f, const int n, VSNodeRef* node, const char*)
{
    VSFDCallbackData* vpyCallbackData = static_cast<VSFDCallbackData*>(userData);

    ++vpyCallbackData->completedFrames;

    if(f)
    {
        vpyCallbackData->reorderMap[n] = f;

        size_t retries = 0;
        while((vpyCallbackData->completedFrames - vpyCallbackData->outputFrames) > vpyCallbackData->parallelRequests) // wait until x265 asks more frames
        {
            Sleep(15);
            if(retries > vpyCallbackData->parallelRequests * 1.5) // we don't want to wait for eternity 
                break;
            retries++;
        }

        if(vpyCallbackData->requestedFrames < vpyCallbackData->totalFrames)
        {
            //x265::general_log(nullptr, "vpy", X265_LOG_FULL, "Callback: retries: %d, current frame: %d, requested: %d, completed: %d, output: %d  \n", retries, n, vpyCallbackData->requestedFrames.load(), vpyCallbackData->completedFrames.load(), vpyCallbackData->outputFrames.load());
            vpyCallbackData->vsapi->getFrameAsync(vpyCallbackData->requestedFrames, node, frameDoneCallback, vpyCallbackData);
            ++vpyCallbackData->requestedFrames;
        }
    }
}

using namespace X265_NS;

VPYInput::VPYInput(InputFileInfo& info) : nextFrame(0), vpyFailed(false)
{
    vss_library = vs_open();
    if(!vss_library)
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "failed to load VapourSynth\n");
        vpyFailed = true;
    }

    vpyCallbackData.outputFrames = 0;
    vpyCallbackData.requestedFrames = 0;
    vpyCallbackData.completedFrames = 0;
    vpyCallbackData.totalFrames = -1;
    vpyCallbackData.startFrame = 0;

    #if defined(__GNUC__) && __GNUC__ >= 8
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
    #endif

    vss_func.init = reinterpret_cast<func_init>(vs_address(vss_library, X86_64 ? "vsscript_init" : "_vsscript_init@0"));
    if(!vss_func.init)
        vpyFailed = true;
    vss_func.finalize = reinterpret_cast<func_finalize>(vs_address(vss_library, X86_64 ? "vsscript_finalize" : "_vsscript_finalize@0"));
    if(!vss_func.finalize)
        vpyFailed = true;
    vss_func.evaluateFile = reinterpret_cast<func_evaluateFile>(vs_address(vss_library, X86_64 ? "vsscript_evaluateFile" : "_vsscript_evaluateFile@12"));
    if(!vss_func.evaluateFile)
        vpyFailed = true;
    vss_func.freeScript = reinterpret_cast<func_freeScript>(vs_address(vss_library, X86_64 ? "vsscript_freeScript" : "_vsscript_freeScript@4")    );
    if(!vss_func.freeScript)
        vpyFailed = true;
    vss_func.getError = reinterpret_cast<func_getError>(vs_address(vss_library, X86_64 ? "vsscript_getError" : "_vsscript_getError@4"));
    if(!vss_func.getError)
        vpyFailed = true;
    vss_func.getOutput = reinterpret_cast<func_getOutput>(vs_address(vss_library, X86_64 ? "vsscript_getOutput" : "_vsscript_getOutput@8"));
    if(!vss_func.getOutput)
        vpyFailed = true;
    vss_func.getCore = reinterpret_cast<func_getCore>(vs_address(vss_library, X86_64 ? "vsscript_getCore" : "_vsscript_getCore@4"));
    if(!vss_func.getCore)
        vpyFailed = true;
    vss_func.getVSApi2 = reinterpret_cast<func_getVSApi2>(vs_address(vss_library, X86_64 ? "vsscript_getVSApi2" : "_vsscript_getVSApi2@4"));
    if(!vss_func.getVSApi2)
        vpyFailed = true;

    #if defined(__GNUC__) && __GNUC__ >= 8
    #pragma GCC diagnostic pop
    #endif

    if(!vss_func.init())
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "failed to initialize VapourSynth environment\n");
        vpyFailed = true;
        return;
    }

    vpyCallbackData.vsapi = vsapi = vss_func.getVSApi2(VAPOURSYNTH_API_VERSION);
    if(vss_func.evaluateFile(&script, info.filename, efSetWorkingDir))
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "Can't evaluate script: %s\n", vss_func.getError(script));
        vpyFailed = true;
        vss_func.freeScript(script);
        vss_func.finalize();
        return;
    }

    node = vss_func.getOutput(script, 0);
    if(!node)
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "`%s' has no video data\n", info.filename);
        vpyFailed = true;
        return;
    }

    const VSCoreInfo* core_info = vsapi->getCoreInfo(vss_func.getCore(script));

    const VSVideoInfo* vi = vsapi->getVideoInfo(node);
    if(!isConstantFormat(vi))
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "only constant video formats are supported\n");
        vpyFailed = true;
    }

    info.width = vi->width;
    info.height = vi->height;

    vpyCallbackData.parallelRequests = core_info->numThreads;

    char errbuf[256];
    frame0 = vsapi->getFrame(0, node, errbuf, sizeof(errbuf));
    if(!frame0)
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "%s occurred while getting frame 0\n", errbuf);
        vpyFailed = true;
        return;
    }

    vpyCallbackData.reorderMap[0] = frame0;
    ++vpyCallbackData.completedFrames;

    const VSMap* frameProps0 = vsapi->getFramePropsRO(frame0);

    info.sarWidth = vsapi->propNumElements(frameProps0, "_SARNum") > 0 ? vsapi->propGetInt(frameProps0, "_SARNum", 0, nullptr) : 0;
    info.sarHeight =vsapi->propNumElements(frameProps0, "_SARDen") > 0 ? vsapi->propGetInt(frameProps0, "_SARDen", 0, nullptr) : 0;
    if(vi->fpsNum == 0 && vi->fpsDen == 0) // VFR detection
    {
        int errDurNum, errDurDen;
        int64_t rateDen = vsapi->propGetInt(frameProps0, "_DurationNum", 0, &errDurNum);
        int64_t rateNum = vsapi->propGetInt(frameProps0, "_DurationDen", 0, &errDurDen);

        if(errDurNum || errDurDen)
        {
            general_log(nullptr, "vpy", X265_LOG_ERROR, "VFR: missing FPS values at frame 0");
            vpyFailed = true;
            return;
        }

        if(!rateNum)
        {
            general_log(nullptr, "vpy", X265_LOG_ERROR, "VFR: FPS numerator is zero at frame 0");
            vpyFailed = true;
            return;
        }

        /* Force CFR until we have support for VFR by x265 */
        info.fpsNum   = rateNum;
        info.fpsDenom = rateDen;
        general_log(nullptr, "vpy", X265_LOG_INFO, "VideoNode is VFR, but x265 doesn't support that at the moment. Forcing CFR\n");
    }
    else
    {
        info.fpsNum   = vi->fpsNum;
        info.fpsDenom = vi->fpsDen;
    }

    info.frameCount = vpyCallbackData.totalFrames = vi->numFrames;
    info.depth = vi->format->bitsPerSample;

    if(vi->format->bitsPerSample >= 8 && vi->format->bitsPerSample <= 16)
    {
        if(vi->format->colorFamily == cmYUV)
        {
            if(vi->format->subSamplingW == 0 && vi->format->subSamplingH == 0) {
                info.csp = X265_CSP_I444;
                general_log(nullptr, "vpy", X265_LOG_INFO, "Video colorspace: YUV444 (YV24)\n");
            }
            else if(vi->format->subSamplingW == 1 && vi->format->subSamplingH == 0) {
                info.csp = X265_CSP_I422;
                general_log(nullptr, "vpy", X265_LOG_INFO, "Video colorspace: YUV422 (YV16)\n");
            }
            else if(vi->format->subSamplingW == 1 && vi->format->subSamplingH == 1) {
                info.csp = X265_CSP_I420;
                general_log(nullptr, "vpy", X265_LOG_INFO, "Video colorspace: YUV420 (YV12)\n");
            }
        }
        else if(vi->format->colorFamily == cmGray) {
            info.csp = X265_CSP_I400;
            general_log(nullptr, "vpy", X265_LOG_INFO, "Video colorspace: YUV400 (Y8)\n");
        }
    }
    else
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "not supported pixel type: %s\n", vi->format->name);
        vpyFailed = true;
        return;
    }
    general_log(nullptr, "vpy", X265_LOG_INFO, "Video depth: %d\n", info.depth);
    general_log(nullptr, "vpy", X265_LOG_INFO, "Video resolution: %dx%d\n", info.width, info.height);
    general_log(nullptr, "vpy", X265_LOG_INFO, "Video framerate: %d/%d\n", info.fpsNum, info.fpsDenom);
    general_log(nullptr, "vpy", X265_LOG_INFO, "Video framecount: %d\n", info.frameCount);
    _info = info;
}

VPYInput::~VPYInput()
{
    if(frame0)
        vsapi->freeFrame(frame0);

    if(node)
        vsapi->freeNode(node);

    vss_func.freeScript(script);
    vss_func.finalize();

    if(vss_library)
        vs_close(vss_library);
}

void VPYInput::startReader()
{
    general_log(nullptr, "vpy", X265_LOG_INFO, "using %d parallel requests\n", vpyCallbackData.parallelRequests);

    const int requestStart = vpyCallbackData.completedFrames;
    const int intitalRequestSize = std::min<int>(vpyCallbackData.parallelRequests, _info.frameCount - requestStart);
    vpyCallbackData.requestedFrames = requestStart + intitalRequestSize;

    for (int n = requestStart; n < requestStart + intitalRequestSize; n++)
        vsapi->getFrameAsync(n, node, frameDoneCallback, &vpyCallbackData);
}

bool VPYInput::readPicture(x265_picture& pic)
{
    const VSFrameRef* currentFrame = nullptr;

    if(nextFrame >= _info.frameCount)
        return false;

    while (!!!vpyCallbackData.reorderMap[nextFrame])
    {
        Sleep(10); // wait for completition a bit
    }

    currentFrame = vpyCallbackData.reorderMap[nextFrame];
    vpyCallbackData.reorderMap.erase(nextFrame);
    ++vpyCallbackData.outputFrames;

    if(!currentFrame)
    {
        general_log(nullptr, "vpy", X265_LOG_ERROR, "error occurred while reading frame %d\n", nextFrame);
        vpyFailed = true;
    }

    pic.width = _info.width;
    pic.height = _info.height;
    pic.colorSpace = _info.csp;
    pic.bitDepth = _info.depth;

    if (frame_size == 0 || frame_buffer == nullptr) {
        for (int i = 0; i < x265_cli_csps[_info.csp].planes; i++)
            frame_size += vsapi->getFrameHeight(currentFrame, i) * vsapi->getStride(currentFrame, i);
        frame_buffer = reinterpret_cast<uint8_t*>(x265_malloc(frame_size));
    }

    pic.framesize = frame_size;

    uint8_t* ptr = frame_buffer;
    for(int i = 0; i < x265_cli_csps[_info.csp].planes; i++)
    {
        pic.stride[i] = vsapi->getStride(currentFrame, i);
        pic.planes[i] = ptr;
        auto len = vsapi->getFrameHeight(currentFrame, i) * pic.stride[i];

        memcpy(pic.planes[i], const_cast<unsigned char*>(vsapi->getReadPtr(currentFrame, i)), len);
        ptr += len;
    }

    vsapi->freeFrame(currentFrame);

    nextFrame++; // for Eof method

    return true;
}

