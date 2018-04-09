#ifndef X265_HEVC_GOP_H
#define X265_HEVC_GOP_H

#include "output.h"
#include "common.h"
#include <fstream>
#include <iostream>
#include <lsmash.h>

namespace x265 {
class GOPOutput : public OutputFile
{
protected:
    bool b_fail;
    int openFile(const char *fname);
    void clean_up();

    FILE* gop_file;
    FILE* data_file;
    char* filename_prefix;
    const char* dir_prefix;
    void FixTimeScale(uint64_t &);
    int64_t GetTimeScaled(int64_t);
    InputFileInfo info;
    int i_numframe;

public:
    GOPOutput(const char *fname, InputFileInfo& inputInfo)
    {
        info = inputInfo;
        b_fail = false;
        gop_file = NULL;
        data_file = NULL;
        if(openFile(fname) != 0)
            b_fail = true;
    }
    bool isFail() const
    {
        return b_fail;
    }

    bool needPTS() const { return true; }

    const char *getName() const { return "gop"; }
    void setParam(x265_param *param);
    int writeHeaders(const x265_nal* nal, uint32_t nalcount);
    int writeFrame(const x265_nal* nal, uint32_t nalcount, x265_picture& pic);
    void closeFile(int64_t largest_pts, int64_t second_largest_pts);
    void release()
    {
        delete this;
    }
};
}

#endif