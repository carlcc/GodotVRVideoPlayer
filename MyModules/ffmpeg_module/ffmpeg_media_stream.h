#pragma once

#include "structs.h"
#include <atomic>
#include <condition_variable>
#include <core/object/ref_counted.h>
#include <core/os/semaphore.h>
#include <list>
#include <memory>
#include <mutex>
#include <scene/resources/texture.h>
#include <servers/audio/audio_rb_resampler.h>

class FfmpegMediaStream : public RefCounted {
    GDCLASS(FfmpegMediaStream, RefCounted);

    enum class State : uint8_t {
        kStopped,
        kPlaying,
        kPaused,
    };

    static void static_mix(void* data);

public:
    enum PixelFormat : int {
        kPixelFormatNone = -1,
        kPixelFormatYuv420P,
        kPixelFormatNv12,
    };

    struct FrameInfo {
        PixelFormat format { PixelFormat::kPixelFormatNone };
        double frameTime { 0.0 };
        Ref<Image> images[4] { nullptr };
    };

    static void _bind_methods();

    FfmpegMediaStream();
    ~FfmpegMediaStream() override;

    FfmpegMediaStream(const FfmpegMediaStream&) = delete;
    FfmpegMediaStream(FfmpegMediaStream&&)      = delete;
    // assigned has been deleted in GD_CLASS macros
    // FfmpegMediaStream& operator=(const FfmpegMediaStream&) = delete;
    FfmpegMediaStream& operator=(FfmpegMediaStream&&) = delete;

    bool set_file(const String& filePath);

    Vector<String> available_video_hardware_accelerators();

    bool create_decoders(const String& videoHwAccel = "none");

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

    Ref<ImageTexture> get_texture(uint32_t index) const { return textures_[index]; }
    uint32_t get_textures_count() const { return textures_.size(); }

private:
    bool mix(AudioFrame* p_buffer, int p_frames);

    void _mix_audio();

    void decode_thread_routine();

    int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type);

    bool try_apply_hw_accelerator(AVCodecContext* codecContext, const AVCodec* codec, const String& hw);

private:
    String filePath_ {};

    // ffmpeg objects
    std::unique_ptr<AvIoContextWrapper> avioContext_;
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> avFormatContext_;
    std::unique_ptr<AVCodecContext, AvCodecContextDeleter> videoCodecContext_;
    std::unique_ptr<AVCodecContext, AvCodecContextDeleter> audioCodecContext_;
    std::unique_ptr<AVPacket, AvPacketDeleter> avPacket_;
    std::unique_ptr<AVFrame, AvFrameDeleter> avFrame_ { nullptr };
    std::unique_ptr<AVBufferRef, AvBufferRefDeleter> hwBuffer_ { nullptr };
    const AVInputFormat* inputFormat_ { nullptr };

    Vector<int> videoStreamIndices_ {};
    Vector<int> audioStreamIndices_ {};
    Vector<int> subtitleStreamIndices_ {};
    int videoStreamIndex_ { AVERROR_DECODER_NOT_FOUND };
    int audioStreamIndex_ { AVERROR_DECODER_NOT_FOUND };

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

    Vector<Ref<ImageTexture>> textures_ {};
    PixelFormat currentPixelFormat_ { PixelFormat::kPixelFormatNone };

    uint32_t textureWidth_ { 0 };
    uint32_t textureHeight_ { 0 };
};

VARIANT_ENUM_CAST(FfmpegMediaStream::PixelFormat);