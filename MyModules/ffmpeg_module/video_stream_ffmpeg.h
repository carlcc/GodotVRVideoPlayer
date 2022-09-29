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
#include "structs.h"

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
