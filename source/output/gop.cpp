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

#include "gop.h"

using namespace X265_NS;
using namespace std;

#ifdef _MSC_VER
    #include <windows.h>
    #define sleep(x) Sleep((x) * 1000)
#endif
#define TIME_WAIT 30

FILE* GOPOutput::open_file_for_write(const string fname, bool retry)
{
    while(true)
    {
        FILE* fp = fopen(fname.c_str(), "wb");
        if(fp != NULL)
            return fp;
        if(!retry)
            break;
        // Retrying
        general_log(NULL, getName(), X265_LOG_WARNING,
            "unable to open file %s for writing, error %d %s, retrying in %d seconds.\n", fname.c_str(), errno, strerror(errno), TIME_WAIT);
        sleep(TIME_WAIT);
    }
    // Failed
    b_fail = true;
    general_log(NULL, getName(), X265_LOG_ERROR,
        "unable to open file %s for writing, error %d %s.\n", fname.c_str(), errno, strerror(errno));
    return NULL;
}

void GOPOutput::smart_fwrite(const void* data, size_t size, FILE* file)
{
    size_t written;
    while(true)
    {
        written = fwrite(data, 1, size, file);
        if(written == size)
            break;
        // ENOSPC
        general_log(NULL, getName(), X265_LOG_WARNING,
            "unable to write, error %d %s, retrying in %d seconds.\n", errno, strerror(errno), TIME_WAIT);
        fseek(file, data_pos, SEEK_SET);
        sleep(TIME_WAIT);
    }
    // Worked
    data_pos += written;
}

void GOPOutput::clean_up()
{
    if(data_file) fclose(data_file);
    if(gop_file)  fclose(gop_file);
}

int GOPOutput::openFile(const char* gop_filename)
{
    gop_file = open_file_for_write(gop_filename, false);
    if(!gop_file) return -1;

    string gop_fn(gop_filename);
    size_t pos;
    if((pos = gop_fn.rfind('/')) != string::npos || (pos = gop_fn.rfind('\\')) != string::npos)
    {
        dir_prefix = gop_fn.substr(0, pos+1);
        gop_fn = gop_fn.substr(pos+1);
    }

    if((pos = gop_fn.rfind('.')) != string::npos)
        filename_prefix = gop_fn.substr(0, pos);
    else
        filename_prefix = gop_fn;

    return 0;
}

void GOPOutput::setParam(x265_param *p_param)
{
    p_param->bAnnexB = false;
    p_param->bRepeatHeaders = false;
    i_numframe = 0;

    FILE* opt_file = open_file_for_write(dir_prefix + filename_prefix + ".options", false);
    if(!opt_file) return;

    fprintf(gop_file, "#options %s.options\n", filename_prefix.c_str());

    fprintf(opt_file, "b-frames %d\n",           p_param->bframes);
    fprintf(opt_file, "b-pyramid %d\n",          p_param->bBPyramid);
    fprintf(opt_file, "input-timebase-num %d\n", info.timebaseNum);
    fprintf(opt_file, "input-timebase-den %d\n", info.timebaseDenom);
    fprintf(opt_file, "output-fps-num %u\n",     p_param->fpsNum);
    fprintf(opt_file, "output-fps-den %u\n",     p_param->fpsDenom);
    fprintf(opt_file, "source-width %d\n",       p_param->sourceWidth);
    fprintf(opt_file, "source-height %d\n",      p_param->sourceHeight);
    fprintf(opt_file, "sar-width %d\n",          p_param->vui.sarWidth);
    fprintf(opt_file, "sar-height %d\n",         p_param->vui.sarHeight);
    fprintf(opt_file, "primaries-index %d\n",    p_param->vui.colorPrimaries);
    fprintf(opt_file, "transfer-index %d\n",     p_param->vui.transferCharacteristics);
    fprintf(opt_file, "matrix-index %d\n",       p_param->vui.matrixCoeffs >= 0 ? p_param->vui.matrixCoeffs : GOP_ISOM_MATRIX_INDEX_UNSPECIFIED);
    fprintf(opt_file, "full-range %d\n",         p_param->vui.bEnableVideoFullRangeFlag >= 0 ? p_param->vui.bEnableVideoFullRangeFlag : 0);

    fclose(opt_file);
}

int GOPOutput::writeHeaders(const x265_nal* p_nal, uint32_t nalcount)
{
    assert(nalcount >= 3); // header should contain 3+ nals

    FILE* hdr_file = open_file_for_write(dir_prefix + filename_prefix + ".headers", false);
    if(!hdr_file) return -1;

    fprintf(gop_file, "#headers %s.headers\n", filename_prefix.c_str());

    for(unsigned int i = 0; i < nalcount; i++)
        smart_fwrite(p_nal[i].payload, p_nal[i].sizeBytes, hdr_file);

    fclose(hdr_file);
    return p_nal[0].sizeBytes + p_nal[1].sizeBytes + p_nal[2].sizeBytes;
}

int GOPOutput::writeFrame(const x265_nal* p_nalu, uint32_t nalcount, x265_picture& pic)
{
    const bool is_keyframe = pic.sliceType == X265_TYPE_IDR;
    int i_size = 0;

    if (is_keyframe) {
        if (data_file)
            fclose(data_file);
        stringstream ss;
        ss << filename_prefix << string("-") << std::setfill('0') << setw(6) << i_numframe << string(".hevc-gop-data");
        string data_filename = ss.str();
        data_file = open_file_for_write(dir_prefix + data_filename, i_numframe > 0);
        if(!data_file) return -1;
        data_pos = 0;
        fprintf(gop_file, "%s\n", data_filename.c_str());
        fflush(gop_file);
    }
    int8_t ts_len = 2 * sizeof(int64_t);
    int8_t ts_lenx[4] = {0, 0, 0, ts_len};
    smart_fwrite(&ts_lenx, 4, data_file);
    smart_fwrite(&pic.pts, sizeof(int64_t), data_file);
    smart_fwrite(&pic.dts, sizeof(int64_t), data_file);

    for(uint8_t i = 0; i < nalcount; i++)
        i_size += p_nalu[i].sizeBytes;

    for(uint8_t i = 0; i < nalcount; i++)
        smart_fwrite(p_nalu[i].payload, p_nalu[i].sizeBytes, data_file);

    i_numframe++;

    return i_size;
}

void GOPOutput::closeFile(int64_t, int64_t)
{
    clean_up();
}
