#ifndef X265_AVS_H
#define X265_AVS_H

#include "input.h"
#include <avisynth/avisynth_c.h>

#if _WIN32
#include <windows.h>
    typedef HMODULE lib_t;
    typedef FARPROC func_t;
    typedef LPCWSTR string_t;
#else
#include <dlfcn.h>
    typedef void* lib_t;
    typedef void* func_t;
    typedef const char* string_t;
    #define __stdcall
#endif

#define AVS_INTERFACE_26 6
#define BUFFER_SIZE 4096

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
    char real_filename[BUFFER_SIZE] {0};
    InputFileInfo _info;
    void load_avs();
    void info_avs();
    void openfile(InputFileInfo& info);
    #if _WIN32
        string_t libname = L"avisynth";
        wchar_t libname_buffer[BUFFER_SIZE];
        void avs_open() { h->library = LoadLibraryW(libname); }
        void avs_close() { FreeLibrary(h->library); h->library = nullptr; }
        func_t avs_address(LPCSTR func) { return GetProcAddress(h->library, func); }
    #else
        #ifdef __MACH__
            string_t libname = "libavisynth.dylib";
        #else
            string_t libname = "libavisynth.so";
        #endif
        char libname_buffer[BUFFER_SIZE];
        void avs_open() { h->library = dlopen(libname, RTLD_NOW); }
        void avs_close() { dlclose(h->library); h->library = nullptr; }
        func_t avs_address(const char * func) { return dlsym(h->library, func); }
    #endif

public:
    AVSInput(InputFileInfo& info)
    {
        h = &handle;
        memset(h, 0, sizeof(handle));

        const char * filename_pos = strstr(info.filename, "]://");
        if(info.filename[0] == '[' && filename_pos) {
            char real_libname[BUFFER_SIZE] {0};
            strncpy(real_libname, info.filename + 1, BUFFER_SIZE - 1);
            strncpy(real_filename, filename_pos + 4, BUFFER_SIZE - 1);
            real_libname[filename_pos - info.filename - 1] = 0;
            #if _WIN32
                if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, real_libname, -1, libname_buffer, sizeof(libname_buffer)/sizeof(wchar_t))) {
                    libname = libname_buffer;
                }
                else {
                    general_log(nullptr, "avs+", X265_LOG_ERROR, "Unable to parse AviSynth+ library path\n");
                    b_fail = true;
                    return;
                }
            #else
                strncpy(libname_buffer, real_libname, BUFFER_SIZE);
                libname = libname_buffer;
            #endif
            general_log(nullptr, "avs+", X265_LOG_INFO, "Using external AviSynth+ library from %s\n", real_libname);
        }
        else {
            strncpy(real_filename, info.filename, BUFFER_SIZE - 1);
        }
        load_avs();
        if (!h->library)
        {
            b_fail = true;
            return;
        }
        info_avs();
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
