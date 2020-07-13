#ifndef X265_AVS_H
#define X265_AVS_H

#include "input.h"
#include <avisynth/avisynth_c.h>

#if _WIN32
#include <windows.h>
typedef HMODULE lib_t;
typedef FARPROC func_t;
#else
#include <dlfcn.h>
typedef void* lib_t;
typedef void* func_t;
#define __stdcall
#endif

#define QUEUE_SIZE 5
#define AVS_INTERFACE_26 6

#define LOAD_AVS_FUNC(name) \
{\
	h->func.name = reinterpret_cast<decltype(h->func.name)>((void*)avs_address(#name));\
	if (!h->func.name) goto fail;\
}

namespace X265_NS {
// x265 private namespace

typedef struct
{
	AVS_Clip *clip;
	AVS_ScriptEnvironment *env;
	lib_t library;
	/* declare function pointers for the utilized functions to be loaded without __declspec,
	   as the avisynth header does not compensate for this type of usage */
	struct
	{
		const char *(__stdcall *avs_clip_get_error)( AVS_Clip *clip );
		AVS_ScriptEnvironment *(__stdcall *avs_create_script_environment)( int version );
		void (__stdcall *avs_delete_script_environment)( AVS_ScriptEnvironment *env );
		AVS_VideoFrame *(__stdcall *avs_get_frame)( AVS_Clip *clip, int n );
		int (__stdcall *avs_get_version)( AVS_Clip *clip );
		const AVS_VideoInfo *(__stdcall *avs_get_video_info)( AVS_Clip *clip );
		int (__stdcall *avs_function_exists)( AVS_ScriptEnvironment *env, const char *name );
		AVS_Value (__stdcall *avs_invoke)( AVS_ScriptEnvironment *env, const char *name,
			AVS_Value args, const char **arg_names );
		void (__stdcall *avs_release_clip)( AVS_Clip *clip );
		void (__stdcall *avs_release_value)( AVS_Value value );
		void (__stdcall *avs_release_video_frame)( AVS_VideoFrame *frame );
		AVS_Clip *(__stdcall *avs_take_clip)( AVS_Value, AVS_ScriptEnvironment *env );
        int (__stdcall *avs_is_y8)(const AVS_VideoInfo * p);
        int (__stdcall *avs_is_420)(const AVS_VideoInfo * p);
        int (__stdcall *avs_is_422)(const AVS_VideoInfo * p);
        int (__stdcall *avs_is_444)(const AVS_VideoInfo * p);
        int (__stdcall *avs_bits_per_component)(const AVS_VideoInfo * p);
	} func;
    int next_frame;
    int plane_count;
} avs_hnd_t;

class AVSInput : public InputFile
{
protected:
    bool b_fail {false};
    bool b_eof {false};
    avs_hnd_t handle;
    avs_hnd_t* h;
    size_t frame_size {0};
    uint8_t* frame_buffer {nullptr};
    InputFileInfo _info;
    void load_avs();
    void info_avs();
    void openfile(InputFileInfo& info);
    #if _WIN32
        void avs_open() { h->library = LoadLibraryW(L"avisynth"); }
        void avs_close() { FreeLibrary(h->library); h->library = nullptr; }
        func_t avs_address(LPCSTR func) { return GetProcAddress(h->library, func); }
    #else
        void avs_open() { h->library = dlopen("libavisynth.so", RTLD_NOW); }
        void avs_close() { dlclose(h->library); h->library = nullptr; }
        func_t avs_address(const char * func) { return dlsym(h->library, func); }
    #endif

public:
    AVSInput(InputFileInfo& info)
    {
        h = &handle;
        memset(h, 0, sizeof(handle));
        load_avs();
        info_avs();
        if (!h->library)
        {
            b_fail = true;
            return;
        }
        openfile(info);
        _info = info;
    }
    ~AVSInput() {}
    void release();
    bool isEof() const
    {
        return h->next_frame >= _info.frameCount;
    }
    bool isFail()
    {
        return b_fail;
    }
    void startReader() {}
    bool readPicture(x265_picture&);
    const char *getName() const
    {
        return "avs+";
    }

    int getWidth() const                          { return _info.width; }

    int getHeight() const                         { return _info.height; }
};
}

#endif // ifndef X265_AVS_H
