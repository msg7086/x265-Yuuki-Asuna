#include "gop.h"

#define NALU_LENGTH_SIZE 4

/*******************/

#define GOP_LOG_ERROR( ... )                general_log( NULL, "gop", X265_LOG_ERROR, __VA_ARGS__ )

#define GOP_FAIL_IF_ERR_R( cond, ... )\
if( cond )\
{\
    GOP_LOG_ERROR( __VA_ARGS__ );\
    b_fail = true;\
    clean_up();\
    return;\
}

#define GOP_FAIL_IF_ERR( cond, ... )\
if( cond )\
{\
    GOP_LOG_ERROR( __VA_ARGS__ );\
    b_fail = true;\
    clean_up();\
    return -1;\
}

/*******************/

using namespace std;

namespace x265 {

void GOPOutput::clean_up()
{
    if(data_file) fclose(data_file);
    if(gop_file)  fclose(gop_file);
}

int GOPOutput::openFile(const char *gop_filename)
{
    gop_file = fopen(gop_filename, "wb");
    GOP_FAIL_IF_ERR(!gop_file, "cannot open output file `%s'.\n", gop_filename);
    
    const char* slash = strrchr(gop_filename, '/');
    if(slash == NULL) slash = strrchr(gop_filename, '\\');
    if(slash == NULL) {
        dir_prefix = "";
        slash = gop_filename;
    }
    else {
        int dir_len =  slash - gop_filename + 2;
        char* tmp_dir_prefix = new char[dir_len];
        strncpy(tmp_dir_prefix, gop_filename, dir_len - 1);
        tmp_dir_prefix[dir_len - 1] = 0;
        dir_prefix = tmp_dir_prefix;
        slash++;
    }
    int len = strlen(slash) - 3;
    filename_prefix = new char[len];
    strncpy(filename_prefix, slash, len - 1);
    filename_prefix[len - 1] = 0;

    return 0;
}

void GOPOutput::setParam(x265_param *p_param)
{
    p_param->bAnnexB = false;
    p_param->bRepeatHeaders = false;
    i_numframe = 0;

    int len = strlen(filename_prefix) + 20;
    char opt_filename[len];
    sprintf(opt_filename, "%s%s.options", dir_prefix, filename_prefix);
    FILE* opt_file = fopen(opt_filename, "wb");
    GOP_FAIL_IF_ERR_R(!opt_file, "cannot open options file `%s'.\n", opt_filename);

    fprintf(gop_file, "#options %s.options\n", filename_prefix);

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
    fprintf(opt_file, "matrix-index %d\n",       p_param->vui.matrixCoeffs >= 0 ? p_param->vui.matrixCoeffs : ISOM_MATRIX_INDEX_UNSPECIFIED);
    fprintf(opt_file, "full-range %d\n",         p_param->vui.bEnableVideoFullRangeFlag >= 0 ? p_param->vui.bEnableVideoFullRangeFlag : 0);

    fclose(opt_file);
}

int GOPOutput::writeHeaders(const x265_nal* p_nal, uint32_t nalcount)
{
    GOP_FAIL_IF_ERR(nalcount < 3, "header should contain 3+ nals");

    int len = strlen(filename_prefix) + 20;
    char hdr_filename[len];
    sprintf(hdr_filename, "%s%s.headers", dir_prefix, filename_prefix);
    FILE* hdr_file = fopen(hdr_filename, "wb");
    GOP_FAIL_IF_ERR(!hdr_file, "cannot open headers file `%s'.\n", hdr_filename);

    fprintf(gop_file, "#headers %s.headers\n", filename_prefix);

    for(unsigned int i = 0; i < nalcount; i++)
        fwrite(p_nal[i].payload, sizeof(uint8_t), p_nal[i].sizeBytes, hdr_file);

    fclose(hdr_file);
    return p_nal[0].sizeBytes + p_nal[1].sizeBytes + p_nal[2].sizeBytes;
}

int GOPOutput::writeFrame(const x265_nal* p_nalu, uint32_t nalcount, x265_picture& pic)
{
    const bool b_keyframe = pic.sliceType == X265_TYPE_IDR;
    int i_size = 0;

    if (b_keyframe) {
        if (data_file)
            fclose(data_file);
        int len = strlen(filename_prefix) + 20;
        char data_filename[len];
        sprintf(data_filename, "%s%s.data-%d", dir_prefix, filename_prefix, i_numframe);
        data_file = fopen(data_filename, "wb");
        GOP_FAIL_IF_ERR(!data_file, "cannot open data file `%s'.\n", data_filename);
        fprintf(gop_file, "%s.data-%d\n", filename_prefix, i_numframe);
        fflush(gop_file);
    }
    int8_t ts_len = 2 * sizeof(int64_t);
    int8_t ts_lenx[4] = {0, 0, 0, ts_len};
    fwrite(&ts_lenx, sizeof(int8_t), 4, data_file);
    fwrite(&pic.pts, sizeof(int64_t), 1, data_file);
    fwrite(&pic.dts, sizeof(int64_t), 1, data_file);

    for(uint8_t i = 0; i < nalcount; i++)
        i_size += p_nalu[i].sizeBytes;

    for(uint8_t i = 0; i < nalcount; i++)
        fwrite(p_nalu[i].payload, sizeof(uint8_t), p_nalu[i].sizeBytes, data_file);

    i_numframe++;

    return i_size;
}

void GOPOutput::closeFile(int64_t, int64_t)
{
    clean_up();
}

}
