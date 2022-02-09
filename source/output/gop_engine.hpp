/*****************************************************************************
 * MIT License
 * 
 * Copyright (c) 2022 Xinyue Lu
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

#pragma once
#include <string>
#include <sstream>
#include <iomanip>

#ifdef _MSC_VER
    #include <windows.h>
    #define sleep(x) Sleep((x) * 1000)
#endif
#define TIME_WAIT 30

using namespace std;

enum class GOP_ISOM_MATRIX_INDEX {
  UNSPECIFIED = 2
};

class GOPEngine {
  private:
  // Input
  const char* inputFilename;
  const InputFileInfo inputInfo;

  // Input Args
  string gopFilename;
  string dirPrefix;
  string filenamePrefix;
  int frameOffset = 0;

  // Internal State
  int currentFrame = 0;
  FILE* gopFP = nullptr;
  FILE* dataFP = nullptr;

  public:
  bool fail = false;

  GOPEngine(const char* fname, InputFileInfo& info)
    : inputFilename(fname)
    , inputInfo(info) {
    ParseInputFilename();
    ParsePrefix();
    gopFP = OpenFileForWrite(dirPrefix + gopFilename, false);
  }

  void SetParam(x265_param *p_param)
  {
    p_param->bAnnexB = false;
    p_param->bRepeatHeaders = false;

    fprintf(gopFP, "#options %s.options\n", filenamePrefix.c_str());
    ProduceOptFile(p_param);
  }

  int WriteHeaders(const x265_nal* p_nal, uint32_t nalcount)
  {
    currentFrame = 0;
    int totalWrites = 0;
    if(nalcount < 3)
      LogError("Too few headers, expect 3+, actual %d", nalcount);

    FILE* hdr_file = OpenFileForWrite(dirPrefix + filenamePrefix + ".headers", false);
    if(!hdr_file) return -1;

    fprintf(gopFP, "#headers %s.headers\n", filenamePrefix.c_str());

    for(uint32_t i = 0; i < nalcount; i++) {
      SmartWrite(p_nal[i].payload, p_nal[i].sizeBytes, hdr_file);
      totalWrites += p_nal[i].sizeBytes;
    }

    fclose(hdr_file);
    return totalWrites;
  }

  int WriteFrame(const x265_nal* p_nalu, uint32_t nalcount, x265_picture& pic)
  {
    const bool is_keyframe = pic.sliceType == X265_TYPE_IDR;
    int totalWrites = 0;

    if (is_keyframe) {
      if (dataFP) {
        fclose(dataFP);
        dataFP = nullptr;
      }
      stringstream ss;
      ss << filenamePrefix << string("-") << std::setfill('0') << setw(6) << (currentFrame + frameOffset) << string(".hevc-gop-data");
      string data_filename = ss.str();
      dataFP = OpenFileForWrite(dirPrefix + data_filename, currentFrame > 0);
      if (!dataFP) return -1;
      fprintf(gopFP, "%s\n", data_filename.c_str());
      fflush(gopFP);
    }
    constexpr int8_t ts_len = 2 * sizeof(int64_t);
    constexpr int8_t ts_lenx[4] = {0, 0, 0, ts_len};
    SmartWrite(&ts_lenx, 4, dataFP);
    SmartWrite(&pic.pts, sizeof(int64_t), dataFP);
    SmartWrite(&pic.dts, sizeof(int64_t), dataFP);

    for(uint8_t i = 0; i < nalcount; i++) {
      SmartWrite(p_nalu[i].payload, p_nalu[i].sizeBytes, dataFP);
      totalWrites += p_nalu[i].sizeBytes;
    }

    currentFrame++;

    return totalWrites;
  }

  void Release() {
    if(dataFP) fclose(dataFP);
    if(gopFP) {
      fprintf(gopFP, "# %d frames written, last frame %d\n", currentFrame, currentFrame + frameOffset);
      fclose(gopFP);
    }
  }

  private:
  void ParseInputFilename() {
    string input(inputFilename);
    // split "?"
    auto sz = input.find_first_of('?');
    if (sz == string::npos) {
      gopFilename = input;
    }
    else {
      gopFilename = input.substr(0, sz);
      string args = input.substr(sz+1);
      ParseInputArgs(args);
    }
  }

  void ParseInputArgs(string args) {
    stringstream ss(args);
    string arg;
    string key;
    string value;
    while(getline(ss, arg, '&')) {
      auto sz = arg.find_first_of('=');
      if (sz == string::npos) {
        key = sz;
        value = "1";
      }
      else {
        key = arg.substr(0, sz);
        value = arg.substr(sz+1);
      }
      if (key == "start")
        frameOffset = stoi(value);
    }
  }

  void ParsePrefix() {
    size_t pos;
    if((pos = gopFilename.find_last_of("/\\")) != string::npos) {
      dirPrefix = gopFilename.substr(0, pos+1);
      gopFilename = gopFilename.substr(pos+1);
    }

    if((pos = gopFilename.find_last_of('.')) != string::npos)
      filenamePrefix = gopFilename.substr(0, pos);
    else
      filenamePrefix = gopFilename;
  }

  void ProduceOptFile(x265_param *p_param) {
    FILE* optFP = OpenFileForWrite(dirPrefix + filenamePrefix + ".options", false);
    if(!optFP) return;

    fprintf(optFP, "b-frames %d\n",           p_param->bframes);
    fprintf(optFP, "b-pyramid %d\n",          p_param->bBPyramid);
    fprintf(optFP, "input-timebase-num %d\n", inputInfo.timebaseNum);
    fprintf(optFP, "input-timebase-den %d\n", inputInfo.timebaseDenom);
    fprintf(optFP, "output-fps-num %u\n",     p_param->fpsNum);
    fprintf(optFP, "output-fps-den %u\n",     p_param->fpsDenom);
    fprintf(optFP, "source-width %d\n",       p_param->sourceWidth);
    fprintf(optFP, "source-height %d\n",      p_param->sourceHeight);
    fprintf(optFP, "sar-width %d\n",          p_param->vui.sarWidth);
    fprintf(optFP, "sar-height %d\n",         p_param->vui.sarHeight);
    fprintf(optFP, "primaries-index %d\n",    p_param->vui.colorPrimaries);
    fprintf(optFP, "transfer-index %d\n",     p_param->vui.transferCharacteristics);
    fprintf(optFP, "matrix-index %d\n",       p_param->vui.matrixCoeffs >= 0 ? p_param->vui.matrixCoeffs : static_cast<int>(GOP_ISOM_MATRIX_INDEX::UNSPECIFIED));
    fprintf(optFP, "full-range %d\n",         p_param->vui.bEnableVideoFullRangeFlag >= 0 ? p_param->vui.bEnableVideoFullRangeFlag : 0);

    fclose(optFP);
  }

  FILE* OpenFileForWrite(const string fname, bool retry)
  {
    while(true)
    {
      FILE* fp = x265_fopen(fname.c_str(), "wb");
      if (fp != NULL)
          return fp;
      if (!retry)
          break;
      // Retrying
      LogWarning("unable to open file %s for writing, error %d %s, retrying in %d seconds.\n", fname.c_str(), errno, strerror(errno), TIME_WAIT);
      sleep(TIME_WAIT);
    }
    // Failed
    fail = true;
    LogError("unable to open file %s for writing, error %d %s.\n", fname.c_str(), errno, strerror(errno));
    return nullptr;
  }

  void SmartWrite(const void* data, size_t size, FILE* file)
  {
    size_t data_pos = ftell(gopFP);
    while(true)
    {
        size_t written = fwrite(data, 1, size, file);
        if(written == size)
            break;
        // ENOSPC
        LogWarning("unable to write, error %d %s, retrying in %d seconds.\n", errno, strerror(errno), TIME_WAIT);
        fseek(file, data_pos, SEEK_SET);
        sleep(TIME_WAIT);
    }
  }

  template <typename ...Params>
  void LogWarning(const char* fmt, Params&&... params) {
    general_log(NULL, "gop+", X265_LOG_WARNING, fmt, forward<Params>(params)...);
  }

  template <typename ...Params>
  void LogError(const char* fmt, Params&&... params) {
    general_log(NULL, "gop+", X265_LOG_ERROR, fmt, forward<Params>(params)...);
  }

};
