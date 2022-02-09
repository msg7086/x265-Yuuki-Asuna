/*****************************************************************************
 * MIT License
 * 
 * Copyright (c) 2018-2022 Xinyue Lu
 *
 * Authors: Xinyue Lu <i@7086.in>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************
 * The MIT License applies to this file only.
 *****************************************************************************/

#ifndef X265_HEVC_GOP_H
#define X265_HEVC_GOP_H

#include <memory>
#include "output.h"
#include "common.h"

using namespace X265_NS;
using namespace std;

#include "gop_engine.hpp"

class GOPOutput : public OutputFile
{
protected:
    std::unique_ptr<GOPEngine> gop_engine;

public:
    GOPOutput(const char* fname, InputFileInfo& inputInfo) : gop_engine(std::make_unique<GOPEngine>(fname, inputInfo)) { }

    const char *getName() const { return "gop+"; }

    bool isFail() const { return gop_engine->fail; }

    bool needPTS() const { return true; }

    void setParam(x265_param* param) { gop_engine->SetParam(param); }

    int writeHeaders(const x265_nal* nal, uint32_t nalcount) { return gop_engine->WriteHeaders(nal, nalcount); }

    int writeFrame(const x265_nal* nal, uint32_t nalcount, x265_picture& pic) { return gop_engine->WriteFrame(nal, nalcount, pic); }

    void closeFile(int64_t, int64_t) { gop_engine->Release(); }

    void release() { delete this; }
};

#endif
