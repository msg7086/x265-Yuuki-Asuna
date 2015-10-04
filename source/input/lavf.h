#ifndef X265_LAVF_H
#define X265_LAVF_H

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include "input.h"
#include "threading.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
#include <libavutil/dict.h>
}

#define QUEUE_SIZE 5

namespace x265 {
// x265 private namespace
typedef struct
{
    AVFormatContext *lavf;
    AVFrame *frame;
    int stream_id;
    int next_frame;
    int vfr_input;
    x265_picture *first_pic;
} lavf_hnd_t;

class LavfInput : public InputFile
{
protected:
    bool b_fail;
    bool b_eof;
    lavf_hnd_t handle;
    lavf_hnd_t* h;
    InputFileInfo _info;
    void openfile(InputFileInfo& info);
public:
    LavfInput(InputFileInfo& info)
    {
        b_fail = false;
        b_eof = false;
        h = &handle;
        memset(h, 0, sizeof(handle));
        openfile(info);
        _info = info;
    }
    ~LavfInput() {}
    void release();
    bool isEof() const
    {
        return handle.next_frame >= _info.frameCount;
    }
    bool isFail()
    {
        return b_fail;
    }
    void startReader() {}
    bool readPicture(x265_picture&);
    bool readPicture(x265_picture&, InputFileInfo*);
    const char *getName() const
    {
        return "lavf";
    }

    int getWidth() const                          { return _info.width; }

    int getHeight() const                         { return _info.height; }
};
}

#endif // ifndef X265_LAVF_H
