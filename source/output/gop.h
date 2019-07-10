/*****************************************************************************
 * MIT License
 * 
 * Copyright (c) 2018-2019 Xinyue Lu
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

#include "output.h"
#include "common.h"
#include <string>
// #include <fstream>
// #include <iostream>
#include <sstream>
#include <iomanip>

enum
{
    GOP_ISOM_MATRIX_INDEX_UNSPECIFIED = 2
};

using namespace X265_NS;
using namespace std;

class GOPOutput : public OutputFile
{
protected:
    bool b_fail;
    int openFile(const char* fname);
    FILE* open_file_for_write(const string fname, bool retry);
    void smart_fwrite(const void* data, size_t size, FILE* file);
    void clean_up();

    FILE* gop_file;
    FILE* data_file;
    size_t data_pos;
    string filename_prefix;
    string dir_prefix;
    InputFileInfo info;
    int i_numframe;

public:
    GOPOutput(const char* fname, InputFileInfo& inputInfo)
    {
        info = inputInfo;
        b_fail = false;
        gop_file = NULL;
        data_file = NULL;
        openFile(fname);
    }
    bool isFail() const
    {
        return b_fail;
    }

    bool needPTS() const { return true; }

    const char *getName() const { return "gop"; }
    void setParam(x265_param* param);
    int writeHeaders(const x265_nal* nal, uint32_t nalcount);
    int writeFrame(const x265_nal* nal, uint32_t nalcount, x265_picture& pic);
    void closeFile(int64_t largest_pts, int64_t second_largest_pts);
    void release()
    {
        delete this;
    }
};

#endif