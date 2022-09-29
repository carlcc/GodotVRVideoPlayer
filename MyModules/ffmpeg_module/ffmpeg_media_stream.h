#pragma once

#include "structs.h"
#include <atomic>
#include <condition_variable>
#include <core/object/ref_counted.h>
#include <core/os/semaphore.h>
#include <memory>
#include <mutex>
#include <list>
#include <scene/resources/texture.h>
#include <servers/audio/audio_rb_resampler.h>

class FfmpegMediaStream : public RefCounted {
    GDCLASS(FfmpegMediaStream, RefCounted);

    enum class State : uint8_t {
        kStopped,
        kPlaying,
        kPaused,
    };

    struct FrameInfo {
        double frameTime;
        Ref<Image> image;
    };
    static void static_mix(void* data);

public:
    static void _bind_methods();

    FfmpegMediaStream();
    ~FfmpegMediaStream() override;

    FfmpegMediaStream(const FfmpegMediaStream&) = delete;
    FfmpegMediaStream(FfmpegMediaStream&&)      = delete;
    // assigned has been deleted in GD_CLASS macros
    // FfmpegMediaStream& operator=(const FfmpegMediaStream&) = delete;
    FfmpegMediaStream& operator=(FfmpegMediaStream&&) = delete;

    bool init(const String& filePath);

    void play();

    void stop();

    void pause();

    void update(double delta);

    double get_length() const;

    double get_position() const;

    double seek(double position);

    bool is_stopped() const { return state_ == State::kStopped; }
    bool is_playing() const { return state_ == State::kPlaying; }
    bool is_paused() const { return state_ == State::kPaused; }

    Ref<Texture2D> get_texture() const { return texture_; }

private:
    bool mix(AudioFrame* p_buffer, int p_frames);

    void _mix_audio();

    void decode_thread_routine();

private:
    String filePath_ {};

    // ffmpeg objects
    std::unique_ptr<AvIoContextWrapper> avioContext_;
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> avFormatContext_;
    std::unique_ptr<AVCodecContext, AvCodecContextDeleter> videoCodecContext_;
    std::unique_ptr<AVCodecContext, AvCodecContextDeleter> audioCodecContext_;
    std::unique_ptr<AVPacket, AvPacketDeleter> avPacket_;
    std::unique_ptr<AVFrame, AvFrameDeleter> avFrame_ { nullptr };

    Vector<int> videoStreamIndices_ {};
    Vector<int> audioStreamIndices_ {};
    Vector<int> subtitleStreamIndices_ {};
    int videoStreamIndex_ { -1 };
    int audioStreamIndex_ { -1 };

    Vector<AudioFrame> mixBuffer_ {};
    AudioRBResampler audioResampler_ {};
    int audioBufferingMs_ { 1000 };
    int waitResamplerLimit_ { 2 };
    int waitResampler_ { 0 };

    // state
    std::atomic<State> state_ { State::kStopped };
    double time_ { 0 };

    //
    static const constexpr size_t kMaxDecodedFrames_ = 2;
    static_assert(kMaxDecodedFrames_ > 0);

    std::mutex decodedImagesMutex_;
    std::condition_variable hasDecodedImageCv_;
    std::list<FrameInfo> decodedImages_;
    std::thread decodingThread_ {};
    double seekTo_ { -1.0 };

    Ref<ImageTexture> texture_ {};
    uint32_t textureWidth_ { 0 };
    uint32_t textureHeight_ { 0 };
};
