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

#ifndef X265_VPY_H
#define X265_VPY_H

#include <unordered_map>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

#include <vapoursynth/VSScript.h>
#include <vapoursynth/VSHelper.h>

#include "input.h"

#ifdef __unix__
#include <dlfcn.h>
#ifdef __MACH__
#define vs_open() dlopen("libvapoursynth-script.dylib", RTLD_LAZY | RTLD_NOW)
#else
#define vs_open() dlopen("libvapoursynth-script.so", RTLD_LAZY | RTLD_NOW)
#endif
#define vs_close dlclose
#define vs_address dlsym
#else
#define vs_open() LoadLibraryW(L"vsscript")
#define vs_close FreeLibrary
#define vs_address GetProcAddress
#endif

struct VSFDCallbackData {
    const VSAPI* vsapi = nullptr;
    std::unordered_map<int, const VSFrameRef*> reorderMap;
    int parallelRequests;
    std::atomic<int> outputFrames;
    std::atomic<int> requestedFrames;
    std::atomic<int> completedFrames;
    int totalFrames;
    int startFrame;
};

namespace X265_NS {
// x265 private namespace

using func_init = int (VS_CC *)();
using func_finalize = int (VS_CC *)();
using func_evaluateFile = int (VS_CC *)(VSScript** handle, const char* scriptFilename, int flags);
using func_freeScript = void (VS_CC *)(VSScript* handle);
using func_getError = const char* (VS_CC *)(VSScript* handle);
using func_getOutput = VSNodeRef* (VS_CC *)(VSScript* handle, int index);
using func_getCore = VSCore* (VS_CC *)(VSScript* handle);
using func_getVSApi2 = const VSAPI* (VS_CC *)(int version);

struct VSSFunc {
    func_init init;
    func_finalize finalize;
    func_evaluateFile evaluateFile;
    func_freeScript freeScript;
    func_getError getError;
    func_getOutput getOutput;
    func_getCore getCore;
    func_getVSApi2 getVSApi2;
};

class VPYInput : public InputFile
{
protected:

    int depth;

    int width;

    int height;

    int colorSpace;

    int frameCount;

    int nextFrame;

    bool vpyFailed;

#if defined(_WIN32_WINNT)
    HMODULE vss_library;
#else
    void* vss_library;
#endif
    VSSFunc vss_func;

    const VSAPI* vsapi = nullptr;

    VSScript* script = nullptr;

    VSNodeRef* node = nullptr;

    const VSFrameRef* frame0 = nullptr;

    VSFDCallbackData vpyCallbackData;

public:

    VPYInput(InputFileInfo& info);

    virtual ~VPYInput();

    void release() {};

    bool isEof() const { return nextFrame >= frameCount; }

    bool isFail() { return vpyFailed; }

    void startReader();

    bool readPicture(x265_picture&);

    const char* getName() const { return "vpy"; }

    int getWidth() const { return width; }

    int getHeight() const { return height; }
};
}

#endif // ifndef X265_VPY_H
