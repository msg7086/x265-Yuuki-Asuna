#ifndef X265_HEVC_MP4_H
#define X265_HEVC_MP4_H

#include "output.h"
#include "common.h"
#include <fstream>
#include <iostream>
#include <lsmash.h>

namespace x265 {
class MP4Output : public OutputFile
{
protected:
    bool b_fail;
    int openFile(const char *fname);
    void remove_mp4_hnd();
    void sign();
    lsmash_root_t *p_root;
    lsmash_video_summary_t *summary;
    uint32_t i_movie_timescale;
    uint32_t i_video_timescale;
    uint32_t i_track;
    uint32_t i_sample_entry;
    uint64_t i_time_inc;
    int64_t i_start_offset;
    uint64_t i_first_cts;
    uint64_t i_prev_dts;
    uint32_t i_sei_size;
    uint8_t *p_sei_buffer;
    int i_numframe;
    int64_t i_init_delta;
    int i_delay_frames;
    lsmash_file_parameters_t file_param;
    uint32_t old_timescale;
    uint32_t old_time_inc;
    uint32_t fpsNum;
    uint32_t fpsDenom;
    uint32_t fpsScale;
    void FixTimeScale(uint64_t &);
    int64_t GetTimeScaled(int64_t);
    InputFileInfo info;
    x265_param *x265Param;

public:
    MP4Output(const char *fname, InputFileInfo& inputInfo)
    {
        info = inputInfo;
        b_fail = false;
        if(openFile(fname) != 0)
            b_fail = true;
    }
    bool isFail() const
    {
        return b_fail;
    }

    bool needPTS() const { return true; }

    const char *getName() const { return "mp4"; }
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