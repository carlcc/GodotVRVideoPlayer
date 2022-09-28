#pragma once

#include <cassert>
#include <core/io/file_access.h>
#include <core/io/resource_loader.h>
#include <core/os/semaphore.h>
#include <core/os/thread.h>
#include <core/templates/ring_buffer.h>
#include <core/templates/safe_refcount.h>
#include <core/templates/vector.h>
#include <scene/resources/video_stream.h>
#include <servers/audio_server.h>

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

class VideoStreamPlaybackFfmpeg : public VideoStreamPlayback {
    GDCLASS(VideoStreamPlaybackFfmpeg, VideoStreamPlayback);

    String file_name_;
    std::unique_ptr<AvIoContextWrapper> avioWrapper_ { nullptr };
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> formatContext_ { nullptr };
    std::unique_ptr<AVCodecContext, AvCodecContextDeleter> codecContext_ { nullptr };
    std::unique_ptr<AVPacket, AvPacketDeleter> packet_ { nullptr };
    std::unique_ptr<AVFrame, AvFrameDeleter> frame_ { nullptr };

    Vector<uint8_t> frame_data_ {};
    int frames_pending_ { 0 };

    Vector<int> videoStreamIndices_ {};
    Vector<int> audioStreamIndices_ {};
    Vector<int> subtitleStreamIndices_ {};
    Ref<ImageTexture> texture_ {};

    int videoStreamIndex_ { -1 };
    int audioStreamIndex_ { -1 };
    int subtitleStreamIndex_ { -1 };

    bool playing_ { false };
    bool paused_ { false };
    double time_ { 0.0 };
    double delay_compensation_ { 0.0 };
    double videobuf_time_ { 0.0 };

    //    enum {
    //        MAX_FRAMES = 4,
    //    };
    //
    //    // Image frames[MAX_FRAMES];
    //    Image::Format format = Image::Format::FORMAT_L8;
    //    Vector<uint8_t> frame_data;
    //    int frames_pending = 0;
    //    Ref<FileAccess> file;
    //    String file_name;
    //    int audio_frames_wrote = 0;
    //    Point2i size;

    double get_time() const { return time_ - delay_compensation_; }

    void video_frame_write(AVFrame* frame);

    void clear();

public:
    static void _bind_methods();

    virtual void play() override;
    virtual void stop() override;
    virtual bool is_playing() const override;

    virtual void set_paused(bool p_paused) override;
    virtual bool is_paused() const override;

    virtual void set_loop(bool p_enable) override;
    virtual bool has_loop() const override;

    virtual double get_length() const override;

    virtual String get_stream_name() const;

    virtual int get_loop_count() const;

    virtual double get_playback_position() const override;
    virtual void seek(double p_time) override;

    void set_file(const String& p_file);

    virtual Ref<Texture2D> get_texture() const override;
    virtual void update(double p_delta) override;

    virtual void set_mix_callback(AudioMixCallback p_callback, void* p_userdata) override;
    virtual int get_channels() const override;
    virtual int get_mix_rate() const override;

    virtual void set_audio_track(int p_idx) override;

    VideoStreamPlaybackFfmpeg();
    ~VideoStreamPlaybackFfmpeg();
};

class VideoStreamFfmpeg : public VideoStream {
    GDCLASS(VideoStreamFfmpeg, VideoStream);

    String file;
    int audio_track;

protected:
    static void _bind_methods();

public:
    Ref<VideoStreamPlayback> instantiate_playback() override
    {
        Ref<VideoStreamPlaybackFfmpeg> pb = memnew(VideoStreamPlaybackFfmpeg);
        pb->set_audio_track(audio_track);
        pb->set_file(file);
        return pb;
    }

    void set_file(const String& p_file) { file = p_file; }
    String get_file() { return file; }
    void set_audio_track(int p_track) override { audio_track = p_track; }

    VideoStreamFfmpeg() { audio_track = 0; }
};

class ResourceFormatLoaderFfmpeg : public ResourceFormatLoader {
public:
    virtual Ref<Resource> load(const String& p_path, const String& p_original_path = "", Error* r_error = nullptr, bool p_use_sub_threads = false, float* r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE);
    virtual void get_recognized_extensions(List<String>* p_extensions) const;
    virtual bool handles_type(const String& p_type) const;
    virtual String get_resource_type(const String& p_path) const;
};
