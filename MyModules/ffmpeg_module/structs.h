#pragma once
#include <cassert>
#include <core/io/file_access.h>
#include <core/string/ustring.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* c)
    {
        if (c) {
            avformat_free_context(c);
        }
    }
};
struct AvCodecContextDeleter {
    void operator()(AVCodecContext* c)
    {
        if (c) {
            avcodec_free_context(&c);
        }
    }
};

struct AvPacketDeleter {
    void operator()(AVPacket* p)
    {
        if (p) {
            av_packet_unref(p);
        }
    }
};

struct AvFrameDeleter {
    void operator()(AVFrame* f)
    {
        if (f) {
            av_frame_unref(f);
        }
    }
};

struct AvBufferRefDeleter {
    void operator()(AVBufferRef* r)
    {
        av_buffer_unref(&r);
    }
};

struct AvIoContextWrapper {
    explicit AvIoContextWrapper(const String& filePath)
    {
        enum { kBufferSize = 16384 };
        auto* buffer = (unsigned char*)av_malloc(kBufferSize); // we don't need to free it
        context      = avio_alloc_context(
                 buffer, kBufferSize, 0, this,
                 [](void* opaque, uint8_t* buf, int buf_size) -> int {
                return reinterpret_cast<AvIoContextWrapper*>(opaque)->read_func(buf, buf_size);
            },
                 [](void* opaque, uint8_t* buf, int buf_size) -> int {
                return reinterpret_cast<AvIoContextWrapper*>(opaque)->write_func(buf, buf_size);
            },
                 [](void* opaque, int64_t offset, int whence) -> int64_t {
                return reinterpret_cast<AvIoContextWrapper*>(opaque)->seek_func(offset, whence);
            });
        file = FileAccess::open(filePath, FileAccess::READ);
    }
    ~AvIoContextWrapper()
    {
        // according to ffmpeg document:
        // the internal buffer could have changed, and  we should free context->buffer
        if (context != nullptr) {
            av_free(context->buffer);
        }
        avio_context_free(&context);
    }

    int read_func(uint8_t* buf, int buf_size)
    {
        return (int)file->get_buffer(buf, buf_size);
    }

    int write_func(uint8_t* buf, int buf_size)
    {
        assert(false && "NYI");
        abort();
    }

    int64_t seek_func(int64_t offset, int whence)
    {
        if (whence & AVSEEK_SIZE) {
            return (int64_t)file->get_length();
        }

        if (whence == AVSEEK_FORCE) {
            abort(); // seems ffmpeg will not pass this flag to me
        }

        switch (whence) {
        case SEEK_SET:
            file->seek(offset);
            break;
        case SEEK_CUR:
            file->seek(file->get_position() + offset);
            break;
        case SEEK_END:
            file->seek_end(offset);
            break;
        default:
            abort();
        }

        return (int64_t)file->get_position();
    }

    AVIOContext* context { nullptr };
    Ref<FileAccess> file { nullptr };
};
