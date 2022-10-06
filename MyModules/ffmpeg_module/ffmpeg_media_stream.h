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

class FfmpegCodecHwConfig : public RefCounted {
    GDCLASS(FfmpegCodecHwConfig, RefCounted);

public:
    static void _bind_methods();
    FfmpegCodecHwConfig() = default;
    explicit FfmpegCodecHwConfig(const AVCodecHWConfig* cfg)
        : config_ { cfg }
    {
    }

    String get_name() const;

    const AVCodecHWConfig* avcodec_hw_config() const { return config_; }

private:
    const AVCodecHWConfig* config_ { nullptr };
};

class FfmpegCodec : public RefCounted {
    GDCLASS(FfmpegCodec, RefCounted);

public:
    static void _bind_methods();
    FfmpegCodec() = default;
    explicit FfmpegCodec(const AVCodec* codec)
        : codec_ { codec }
    {
    }

    TypedArray<FfmpegCodecHwConfig> available_hw_configs() const;
    String get_name() const { return codec_->name; }

    const AVCodec* avcodec() const { return codec_; }

private:
    const AVCodec* codec_ { nullptr };
};

class FfmpegMediaStream : public RefCounted {
    GDCLASS(FfmpegMediaStream, RefCounted);

    static void static_mix(void* data);

public:
    enum PixelFormat : int {
        kPixelFormatNone = -1,
        kPixelFormatYuv420P,
        kPixelFormatNv12,
    };
    enum State : int {
        kStateStopped,
        kStatePlaying,
        kStatePaused,
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

    TypedArray<FfmpegCodec> available_video_decoders() const;

    bool create_decoders(const FfmpegCodec* videoCodec, const FfmpegCodecHwConfig* videoHwCfg);

    void play();

    void stop();

    void pause();

    State get_state() const { return state_; }

    void update(double delta);

    double get_length() const;

    double get_position() const;

    double seek(double position);

    bool is_stopped() const { return state_ == State::kStateStopped; }
    bool is_playing() const { return state_ == State::kStatePlaying; }
    bool is_paused() const { return state_ == State::kStatePaused; }

    int get_video_stream_count() const { return videoStreamIndices_.size(); }
    int get_audio_stream_count() const { return audioStreamIndices_.size(); }
    int get_subtitle_stream_count() const { return subtitleStreamIndices_.size(); }
    String get_encapsulation_format() const { return inputFormat_ == nullptr ? "[Unknown]" : inputFormat_->name; }
    String get_video_encoding_format() const { return videoCodecContext_ == nullptr ? "[Unknown]" : avcodec_get_name(videoCodecContext_->codec->id); }
    String get_audio_encoding_format() const { return audioCodecContext_ == nullptr ? "[Unknown]" : avcodec_get_name(audioCodecContext_->codec->id); }
    String get_video_codec_name() const { return videoCodecContext_ == nullptr || videoCodecContext_->codec == nullptr ? "[Unknown]" : videoCodecContext_->codec->name; }
    String get_audio_codec_name() const { return audioCodecContext_ == nullptr || audioCodecContext_->codec == nullptr ? "[Unknown]" : audioCodecContext_->codec->name; }

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
    std::atomic<State> state_ { State::kStateStopped };
    double time_ { 0 };
    double lastFrameTime_ { 0 };
    mutable double totalTime_ { 0 };

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
VARIANT_ENUM_CAST(FfmpegMediaStream::State);