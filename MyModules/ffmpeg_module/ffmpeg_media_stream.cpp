#include "ffmpeg_media_stream.h"
#include <scene/audio/audio_stream_player.h>
#include <string>

extern "C" {
#include "libavutil/avutil.h"
}

#ifdef __ANDROID__
extern "C" {
#include <libavcodec/jni.h>
}
#include <platform/android/thread_jandroid.h>

static void register_java_vm()
{
    JNIEnv* env = get_jni_env();
    JavaVM* vm  = nullptr;
    env->GetJavaVM(&vm);

    av_jni_set_java_vm(vm, nullptr);
}

#endif

static double get_stream_time_seconds(AVStream* stream, int64_t pts)
{
    return pts * stream->time_base.num / (double)stream->time_base.den;
}

static int64_t get_stream_time_pts(AVStream* stream, double seconds)
{
    return int64_t(seconds * stream->time_base.den / (double)stream->time_base.num);
}

static double get_stream_duration(AVStream* stream)
{
    return get_stream_time_seconds(stream, stream->duration);
}

static double get_context_duration(AVFormatContext* ctx)
{
    return ctx->duration / AV_TIME_BASE;
}

inline uint32_t get_textures_count_by_pixel_format(FfmpegMediaStream::PixelFormat fmt)
{
    // clang-format off
    switch (fmt) {
    case FfmpegMediaStream::kPixelFormatYuv420P: return 3;
    case FfmpegMediaStream::kPixelFormatNv12:    return 2;
    case FfmpegMediaStream::kPixelFormatNone:
    default:
        ERR_PRINT("Invalid format");
        abort();
    }
    // clang-format on
}

static String get_hw_type_name(const AVCodecHWConfig* cfg)
{
    if (cfg == nullptr) {
        return "[null]";
    }
    auto* deviceTypeName = av_hwdevice_get_type_name(cfg->device_type);
    if (deviceTypeName == nullptr) {
        deviceTypeName = "[null]";
    }
    String result = deviceTypeName;
    result        = result.to_lower();
    return result;
}

static const String kPixelFormatChangedSignalName { "pixel_format_changed" };
static const String kPlayStateChangedSignalName { "play_state_changed" };

void FfmpegCodecHwConfig::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_name"), &FfmpegCodecHwConfig::get_name);
}

String FfmpegCodecHwConfig::get_name() const
{
    return get_hw_type_name(config_);
}

void FfmpegCodec::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("available_hw_configs"), &FfmpegCodec::available_hw_configs);
    ClassDB::bind_method(D_METHOD("get_name"), &FfmpegCodec::get_name);
}

TypedArray<FfmpegCodecHwConfig> FfmpegCodec::available_hw_configs() const
{
    TypedArray<FfmpegCodecHwConfig> result;
    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec_, i);
        if (config == nullptr) {
            break;
        }
        result.push_back(memnew(FfmpegCodecHwConfig(config)));
    }
    return result;
}

void FfmpegMediaStream::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_texture", "index"), &FfmpegMediaStream::get_texture);
    ClassDB::bind_method(D_METHOD("get_textures_count"), &FfmpegMediaStream::get_textures_count);
    ClassDB::bind_method(D_METHOD("update", "delta"), &FfmpegMediaStream::update);
    ClassDB::bind_method(D_METHOD("play"), &FfmpegMediaStream::play);
    ClassDB::bind_method(D_METHOD("stop"), &FfmpegMediaStream::stop);
    ClassDB::bind_method(D_METHOD("pause"), &FfmpegMediaStream::pause);
    ClassDB::bind_method(D_METHOD("is_playing"), &FfmpegMediaStream::is_playing);
    ClassDB::bind_method(D_METHOD("is_paused"), &FfmpegMediaStream::is_paused);
    ClassDB::bind_method(D_METHOD("is_stopped"), &FfmpegMediaStream::is_stopped);
    ClassDB::bind_method(D_METHOD("get_state"), &FfmpegMediaStream::get_state);
    ClassDB::bind_method(D_METHOD("get_length"), &FfmpegMediaStream::get_length);
    ClassDB::bind_method(D_METHOD("get_position"), &FfmpegMediaStream::get_position);
    ClassDB::bind_method(D_METHOD("get_video_stream_count"), &FfmpegMediaStream::get_video_stream_count);
    ClassDB::bind_method(D_METHOD("get_audio_stream_count"), &FfmpegMediaStream::get_audio_stream_count);
    ClassDB::bind_method(D_METHOD("get_subtitle_stream_count"), &FfmpegMediaStream::get_subtitle_stream_count);
    ClassDB::bind_method(D_METHOD("get_encapsulation_format"), &FfmpegMediaStream::get_encapsulation_format);
    ClassDB::bind_method(D_METHOD("get_video_encoding_format"), &FfmpegMediaStream::get_video_encoding_format);
    ClassDB::bind_method(D_METHOD("get_audio_encoding_format"), &FfmpegMediaStream::get_audio_encoding_format);
    ClassDB::bind_method(D_METHOD("get_video_codec_name"), &FfmpegMediaStream::get_video_codec_name);
    ClassDB::bind_method(D_METHOD("get_audio_codec_name"), &FfmpegMediaStream::get_audio_codec_name);

    ClassDB::bind_method(D_METHOD("seek", "position"), &FfmpegMediaStream::seek);
    ClassDB::bind_method(D_METHOD("set_file", "filePath"), &FfmpegMediaStream::set_file);
    ClassDB::bind_method(D_METHOD("available_video_decoders"), &FfmpegMediaStream::available_video_decoders);
    ClassDB::bind_method(D_METHOD("create_decoders", "hw"), &FfmpegMediaStream::create_decoders);

    ADD_SIGNAL(MethodInfo(kPixelFormatChangedSignalName, PropertyInfo(Variant::INT, "format")));
    ADD_SIGNAL(MethodInfo(kPlayStateChangedSignalName, PropertyInfo(Variant::INT, "state")));
    BIND_ENUM_CONSTANT(kPixelFormatNone);
    BIND_ENUM_CONSTANT(kPixelFormatYuv420P);
    BIND_ENUM_CONSTANT(kPixelFormatNv12);

    BIND_ENUM_CONSTANT(kStateStopped);
    BIND_ENUM_CONSTANT(kStatePlaying);
    BIND_ENUM_CONSTANT(kStatePaused);

#ifdef __ANDROID__
    register_java_vm();
#endif
}

bool FfmpegMediaStream::set_file(const String& filePath)
{
    if (!filePath_.is_empty()) {
        ERR_PRINT("You have set the file path before, try to create a new FfmpegMediaStream object!");
        return false;
    }
    bool useAvio =
#if defined(__ANDROID__)
        filePath.begins_with("res://") || filePath.begins_with("user://");
#else
        true;
#endif
    if (useAvio) {
        avioContext_ = std::make_unique<AvIoContextWrapper>(filePath);
        auto* avio   = avioContext_.get();
        if (avio->file.is_null()) {
            ERR_PRINT("Cannot open file '" + filePath + "'.");
            return false;
        }
    }
    filePath_ = filePath;

    int ret = 0;
    if (useAvio) {
        ret = av_probe_input_buffer(avioContext_.get()->context, &inputFormat_, "", nullptr, 0, 0);
        if (ret < 0) {
            ERR_PRINT("Failed to probe input format!");
            return false;
        }
    }

    // open file
    auto utf8FilePath              = filePath.utf8();
    const char* url                = utf8FilePath.get_data();
    AVFormatContext* formatContext = avformat_alloc_context();
    if (useAvio) {
        formatContext->pb    = avioContext_->context;
        formatContext->flags = AVFMT_FLAG_CUSTOM_IO;
        url                  = "";
    }
    avFormatContext_.reset(formatContext);
    ret = avformat_open_input(&formatContext, url, inputFormat_, nullptr);
    if (ret != 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        ERR_PRINT(String("Failed to call avformat_open_input(): {0}, {1}").format(varray(buf, filePath)));

        return false;
    }
    inputFormat_ = avFormatContext_->iformat;

    for (int i = 0; i < formatContext->nb_streams; ++i) {
        auto* stream = formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndices_.push_back(i);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndices_.push_back(i);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            subtitleStreamIndices_.push_back(i);
        }
    }
    if (!videoStreamIndices_.is_empty()) { // TODO: Multiple stream?
        const AVCodec* codec = nullptr;
        videoStreamIndex_    = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        assert(videoStreamIndex_ >= 0);
    }
    if (!audioStreamIndices_.is_empty()) {
        const AVCodec* codec = nullptr;
        audioStreamIndex_    = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        assert(audioStreamIndex_ >= 0);
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        ERR_PRINT("Failed to find stream info");
        return false;
    }

    if (videoStreamIndices_.is_empty() && audioStreamIndices_.is_empty()) {
        ERR_PRINT("Failed to find a video stream and audio stream.");
        return false;
    }
    avPacket_.reset(av_packet_alloc());
    avFrame_.reset(av_frame_alloc());

    return true;
}

TypedArray<FfmpegCodec> FfmpegMediaStream::available_video_decoders() const
{
    TypedArray<FfmpegCodec> result;

    if (!videoStreamIndices_.is_empty()) {
        auto codecId   = avFormatContext_->streams[videoStreamIndex_]->codecpar->codec_id;
        void* iterator = nullptr;
        while (const AVCodec* codec = av_codec_iterate(&iterator)) {
            if (!av_codec_is_decoder(codec)) {
                continue;
            }
            if (codec->id != codecId) {
                continue;
            }

            result.push_back(memnew(FfmpegCodec(codec)));
        }
    }

    return result;
}

bool FfmpegMediaStream::create_decoders(const FfmpegCodec* videoCodec, const FfmpegCodecHwConfig* videoHwCfg)
{
    if (videoStreamIndex_ >= 0) {
        auto* stream = avFormatContext_->streams[videoStreamIndex_];
        auto codecId = videoCodec->avcodec()->id;
        auto* codec  = videoCodec->avcodec();

        auto codecContext = avcodec_alloc_context3(codec);
        videoCodecContext_.reset(codecContext);
        avcodec_parameters_to_context(codecContext, stream->codecpar);

        bool isHwAccelerated = false;
        if (videoHwCfg != nullptr) {
            isHwAccelerated = 0 == hw_decoder_init(codecContext, videoHwCfg->avcodec_hw_config()->device_type);
            if (!isHwAccelerated) {
                codecContext->thread_count = (int)std::thread::hardware_concurrency();
            }
        }

        if (isHwAccelerated) {
            String hw = get_hw_type_name(videoHwCfg->avcodec_hw_config());
            ERR_PRINT(String("Using hw decoder {0}").format(varray(hw)));
        } else {
            ERR_PRINT(String("No hw decoder for {0} with name {1} found").format(varray(codec->name)));
        }

        auto ret = avcodec_open2(codecContext, codec, nullptr);
        if (0 != ret) {
            ERR_PRINT(String("Open codec {0} failed").format(varray(avcodec_get_name(codecId))));
            return false;
        }
    }
    if (audioStreamIndex_ > 0) {
        auto* stream = avFormatContext_->streams[audioStreamIndex_];
        auto codecId = stream->codecpar->codec_id;
        auto* codec  = avcodec_find_decoder(codecId);

        auto codecContext = avcodec_alloc_context3(codec);
        audioCodecContext_.reset(codecContext);
        avcodec_parameters_to_context(codecContext, stream->codecpar);
        if (0 != avcodec_open2(codecContext, codec, nullptr)) {
            ERR_PRINT(String("Open codec {0} failed").format(avcodec_get_name(codecId)));
            return false;
        }

        auto channelCount = codecContext->ch_layout.nb_channels;
        auto sampleRate   = codecContext->sample_rate;
        audioResampler_.setup(channelCount, sampleRate, AudioServer::get_singleton()->get_mix_rate(), audioBufferingMs_, 0);
    } else {
        audioResampler_.clear();
    }
    return true;
}

FfmpegMediaStream::FfmpegMediaStream() = default;

FfmpegMediaStream::~FfmpegMediaStream()
{
    // First stop playing
    stop();

    // Then destroy ffmpeg related objects
    // TODO:
}

void FfmpegMediaStream::static_mix(void* data)
{
    reinterpret_cast<FfmpegMediaStream*>(data)->_mix_audio();
}

void FfmpegMediaStream::play()
{
    if (state_ == State::kStatePlaying) {
        return;
    }
    State prevState = state_;
    state_          = State::kStatePlaying; // set state now, it will be used in decoding thread
    if (prevState == State::kStateStopped) {
        assert(!decodingThread_.joinable());
        decodingThread_ = std::thread([this]() {
            decode_thread_routine();
        });
        if (audioStreamIndex_ != -1) {
            mixBuffer_.resize(AudioServer::get_singleton()->thread_get_mix_buffer_size()); // resize before register
            AudioServer::get_singleton()->add_mix_callback(&FfmpegMediaStream::static_mix, this);
        }
    }

    emit_signal(kPlayStateChangedSignalName, State::kStatePlaying);
}

void FfmpegMediaStream::stop()
{
    if (state_ == State::kStateStopped) {
        return;
    }
    AudioServer::get_singleton()->remove_mix_callback(&FfmpegMediaStream::static_mix, this);

    {
        std::unique_lock<std::mutex> lck(decodedImagesMutex_);
        state_ = State::kStateStopped;
    }
    hasDecodedImageCv_.notify_one();
    if (decodingThread_.joinable()) {
        decodingThread_.join();
    }
    time_          = 0;
    lastFrameTime_ = 0;
    totalTime_     = 0;
    emit_signal(kPlayStateChangedSignalName, State::kStateStopped);
}

void FfmpegMediaStream::pause()
{
    if (state_ != State::kStatePlaying) {
        ERR_PRINT("The stream is not being played");
        return;
    }

    state_ = State::kStatePaused;
    emit_signal(kPlayStateChangedSignalName, State::kStatePaused);
}

void FfmpegMediaStream::update(double delta)
{
    if (state_ != State::kStatePlaying) {
        return;
    }

    time_ += delta;

    FrameInfo frameInfo {};
    {
        std::unique_lock<std::mutex> lck(decodedImagesMutex_);
        if (!decodedImages_.empty()) {
            auto frameTime = decodedImages_.front().frameTime;
            if (time_ > frameTime) {
                frameInfo = decodedImages_.front();
                decodedImages_.pop_front();

                lck.unlock();
                hasDecodedImageCv_.notify_one();

                // if the decoding thread cannot catch up time, then we slow down the clock
                auto tmp = frameTime + 500.0;
                if (time_ > tmp) {
                    time_     = tmp;
                    frameTime = tmp;
                }
                lastFrameTime_ = frameTime;
            }
        }
    }

    if (frameInfo.format != PixelFormat::kPixelFormatNone) {
        auto textureCount = get_textures_count_by_pixel_format(frameInfo.format);

        if (frameInfo.format != currentPixelFormat_) {
            currentPixelFormat_ = frameInfo.format;
            textures_.clear();
            textures_.resize((int)textureCount);
            for (auto& t : textures_) {
                t = Ref<ImageTexture>(memnew(ImageTexture));
            }
            // force recreate texture
            textureWidth_  = 0;
            textureHeight_ = 0;

            emit_signal(kPixelFormatChangedSignalName, frameInfo.format);
        }

        auto& img0 = frameInfo.images[0];
        auto* tw   = textures_.ptrw();
        if (textureWidth_ != img0->get_width() || textureHeight_ != img0->get_height()) {
            textureWidth_  = img0->get_width();
            textureHeight_ = img0->get_height();
            for (uint32_t i = 0; i < textureCount; ++i) {
                tw[i]->set_image(frameInfo.images[i]);
            }
        } else {
            for (uint32_t i = 0; i < textureCount; ++i) {
                tw[i]->update(frameInfo.images[i]);
            }
        }
    } else if (frameInfo.frameTime < 0) {
        stop();
    }
}

double FfmpegMediaStream::get_length() const
{
    if (totalTime_ == 0) {
        totalTime_ = get_context_duration(avFormatContext_.get());
    }
    return totalTime_;
}

double FfmpegMediaStream::get_position() const
{
    return lastFrameTime_;
}

double FfmpegMediaStream::seek(double position)
{
    decltype(decodedImages_) frames; // To ensure frame are not freed in critical area
    {
        std::unique_lock<std::mutex> lck(decodedImagesMutex_);
        seekTo_ = position;
        time_   = position;
        frames  = std::move(decodedImages_);
        audioResampler_.flush();
    }
    // Tell the decoding thread that we want to seek
    hasDecodedImageCv_.notify_one();

    return 0.0;
}

#define CHECK_AV_ERROR(errcode)                                    \
    do {                                                           \
        if (errcode == 0)                                          \
            break;                                                 \
        char errBuf[512];                                          \
        av_strerror(ret, errBuf, sizeof(errBuf));                  \
        ERR_PRINT(String("av error: {0}").format(varray(errBuf))); \
    } while (false)

static int64_t now()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

template <int kElementSize>
inline void copy_video_frame(
    int width, int height,
    uint8_t* dst,
    uint8_t* src, int srcStride)
{
    auto dstStride = width * kElementSize;
    assert(dstStride <= srcStride);
    if (width == srcStride) {
        memcpy(dst, src, dstStride * height);
    } else {
        for (int i = 0; i < height; ++i) {
            memcpy(dst, src, dstStride);
            dst += dstStride;
            src += srcStride;
        }
    }
}

static void FillYuv420P(FfmpegMediaStream::FrameInfo& frameInfo, AVFrame* frame)
{
    auto width      = frame->width;
    auto height     = frame->height;
    auto halfWidth  = frame->width / 2;
    auto halfHeight = frame->height / 2;
    auto ySize      = width * height;
    auto uvSize     = halfWidth * halfHeight;

    Vector<uint8_t> yBuffer, uBuffer, vBuffer;
    yBuffer.resize(ySize);
    uBuffer.resize(uvSize);
    vBuffer.resize(uvSize);
    auto* yw = yBuffer.ptrw();
    auto* uw = uBuffer.ptrw();
    auto* vw = vBuffer.ptrw();

    copy_video_frame<1>(width, height, yw, frame->data[0], frame->linesize[0]);
    copy_video_frame<1>(halfWidth, halfHeight, uw, frame->data[1], frame->linesize[1]);
    copy_video_frame<1>(halfWidth, halfHeight, vw, frame->data[2], frame->linesize[2]);

    frameInfo.images[0] = Ref<Image> { memnew(Image(width, height, false, Image::FORMAT_R8, yBuffer)) };
    frameInfo.images[1] = Ref<Image> { memnew(Image(halfWidth, halfHeight, false, Image::FORMAT_R8, uBuffer)) };
    frameInfo.images[2] = Ref<Image> { memnew(Image(halfWidth, halfHeight, false, Image::FORMAT_R8, vBuffer)) };
}

static void FillNv12(FfmpegMediaStream::FrameInfo& frameInfo, AVFrame* frame)
{
    auto width      = frame->width;
    auto height     = frame->height;
    auto halfWidth  = frame->width / 2;
    auto halfHeight = frame->height / 2;
    auto ySize      = width * height;
    auto uvSize     = halfWidth * halfHeight * 2;

    Vector<uint8_t> yBuffer, uvBuffer;
    yBuffer.resize(ySize);
    uvBuffer.resize(uvSize);
    auto* yw  = yBuffer.ptrw();
    auto* uvw = uvBuffer.ptrw();

    copy_video_frame<1>(width, height, yw, frame->data[0], frame->linesize[0]);
    copy_video_frame<2>(halfWidth, halfHeight, uvw, frame->data[1], frame->linesize[1]);

    frameInfo.images[0] = Ref<Image> { memnew(Image(width, height, false, Image::FORMAT_R8, yBuffer)) };
    frameInfo.images[1] = Ref<Image> { memnew(Image(halfWidth, halfHeight, false, Image::FORMAT_RG8, uvBuffer)) };
}

static FfmpegMediaStream::FrameInfo AVFrame2Image(AVFrame* frame, AVFrame* tmpFrame)
{
    // TODO: other format

    FfmpegMediaStream::FrameInfo frameInfo {};

    // TODO: handle odd frame size?
    assert(frame->width % 2 == 0);
    assert(frame->height % 2 == 0);

    {
        // TODO: convert with gpu or render these formats directly using material and shader
        if (frame->format == AV_PIX_FMT_YUV420P) {
            frameInfo.format = FfmpegMediaStream::kPixelFormatYuv420P;
            FillYuv420P(frameInfo, frame);
        } else if (frame->format == AV_PIX_FMT_NV12) {
            frameInfo.format = FfmpegMediaStream::kPixelFormatNv12;
            FillNv12(frameInfo, frame);
        } else if (frame->format == AV_PIX_FMT_DXVA2_VLD || frame->format == AV_PIX_FMT_VIDEOTOOLBOX || frame->format == AV_PIX_FMT_D3D11) {
            auto ret = av_hwframe_transfer_data(tmpFrame, frame, 0);
            if (ret < 0) {
                ERR_PRINT("Failed to transfer hw frame");
                return frameInfo;
            }

            frameInfo.format = FfmpegMediaStream::kPixelFormatNv12;
            FillNv12(frameInfo, tmpFrame);
        } else {
            ERR_PRINT("Unsupported frame format");
            abort(); // TODO:
        }
    }

    return frameInfo;
}

bool FfmpegMediaStream::mix(AudioFrame* p_buffer, int p_frames)
{
    // Check the amount resampler can really handle.
    // If it cannot, wait "wait_resampler_phase_limit" times.
    // This mechanism contributes to smoother pause/unpause operation.
    // if (p_frames <= audioResampler_.get_num_of_ready_frames() || wait_resampler_limit <= wait_resampler) {
    //     wait_resampler = 0;
    //     return resampler.mix(p_buffer, p_frames);
    // }
    // wait_resampler++;

    if (p_frames <= audioResampler_.get_num_of_ready_frames() || waitResamplerLimit_ <= waitResampler_) {
        waitResampler_ = 0;
        return audioResampler_.mix(p_buffer, p_frames);
    }
    waitResampler_++;
    return false;
}

// Called from audio thread
void FfmpegMediaStream::_mix_audio()
{
    //    if (!stream.is_valid()) {
    //        return;
    //    }
    //    if (!playback.is_valid() || !playback->is_playing() || playback->is_paused()) {
    //        return;
    //    }
    if (!is_playing()) {
        return;
    }

    AudioFrame* buffer = mixBuffer_.ptrw();
    int buffer_size    = mixBuffer_.size();

    // Resample
    if (!mix(buffer, buffer_size)) {
        return;
    }

    float volume   = 1.0F; // TODO: set this
    AudioFrame vol = AudioFrame(volume, volume);

    int cc = AudioServer::get_singleton()->get_channel_count();

    static auto masterBusIndex = AudioServer::get_singleton()->get_bus_index("Master");
    if (cc == 1) {
        AudioFrame* target = AudioServer::get_singleton()->thread_get_channel_mix_buffer(masterBusIndex, 0);
        ERR_FAIL_COND(!target);

        for (int j = 0; j < buffer_size; j++) {
            target[j] += buffer[j] * vol;
        }

    } else {
        AudioFrame* targets[4];

        for (int k = 0; k < cc; k++) {
            targets[k] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(masterBusIndex, k);
            ERR_FAIL_COND(!targets[k]);
        }

        for (int j = 0; j < buffer_size; j++) {
            AudioFrame frame = buffer[j] * vol;
            for (int k = 0; k < cc; k++) {
                targets[k][j] += frame;
            }
        }
    }
}

void FfmpegMediaStream::decode_thread_routine()
{
    auto* formatContext     = avFormatContext_.get();
    auto* videoCodecContext = videoCodecContext_.get();
    auto* audioCodecContext = audioCodecContext_.get();
    auto* avPacket          = avPacket_.get();
    auto* avFrame           = avFrame_.get();
    auto* tmpFrame          = av_frame_alloc(); // TODO: Only alloc when decoder is hw decoder
    tmpFrame->format        = AV_PIX_FMT_NV12;
    std::unique_ptr<AVFrame, AvFrameDeleter> tmpFrame_(tmpFrame);

    while (state_ != State::kStateStopped) {
        // handle seek
        double seekTo = -1.0;
        {
            std::unique_lock<std::mutex> lck(decodedImagesMutex_);
            if (seekTo_ >= 0) {
                seekTo  = seekTo_;
                seekTo_ = -1.0;
            }
        }
        if (seekTo >= 0) {
            int seekFlags    = AVSEEK_FLAG_BACKWARD;
            bool seekSucceed = false;
            if (audioStreamIndex_ != AVERROR_DECODER_NOT_FOUND) {
                auto pts    = get_stream_time_pts(avFormatContext_->streams[audioStreamIndex_], seekTo);
                auto ret    = av_seek_frame(avFormatContext_.get(), audioStreamIndex_, pts, seekFlags);
                seekSucceed = ret == 0;
            }
            if (!seekSucceed && videoStreamIndex_ != AVERROR_DECODER_NOT_FOUND) {
                auto pts    = get_stream_time_pts(avFormatContext_->streams[videoStreamIndex_], seekTo);
                auto ret    = av_seek_frame(avFormatContext_.get(), videoStreamIndex_, pts, seekFlags);
                seekSucceed = ret == 0;
            }
            if (videoCodecContext != nullptr) {
                avcodec_flush_buffers(videoCodecContext);
            }
            if (audioCodecContext != nullptr) {
                avcodec_flush_buffers(audioCodecContext);
            }
            // TODO: Audios sounds strange, bug?

            (void)seekSucceed;
        }

        int ret = 0;
        do {
            if (state_ == State::kStateStopped) {
                return;
            }
            ret = av_read_frame(formatContext, avPacket);
        } while (ret == AVERROR(EAGAIN));
        std::unique_ptr<AVPacket, AvPacketDeleter> unrefOnExitScope(avPacket); // we need to unref the packet

        if (ret == AVERROR_EOF) {
            // TODO: flush decoder's internal frames
            WARN_PRINT("Video stream ended!");
            {
                std::unique_lock<std::mutex> lck(decodedImagesMutex_);
                decodedImages_.push_back(FrameInfo { PixelFormat::kPixelFormatNone, -1, { nullptr } });
            }
            return;
        }

        if (avPacket->stream_index == videoStreamIndex_) {
            int sendRet = 0;
            do {
                if (state_ == State::kStateStopped) {
                    return;
                }
                sendRet = avcodec_send_packet(videoCodecContext, avPacket);
                if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
                    CHECK_AV_ERROR(sendRet);
                    break;
                }

                do {
                    ret = avcodec_receive_frame(videoCodecContext, avFrame);
                    if (ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret < 0) {
                        CHECK_AV_ERROR(ret);
                        abort();
                        return;
                    }
                    auto frameInfo = AVFrame2Image(avFrame, tmpFrame);
                    if (frameInfo.format == PixelFormat::kPixelFormatNone) {
                        // Convert failed
                        ERR_PRINT("Failed to convert frame, discard");
                        continue;
                    }
                    frameInfo.frameTime = get_stream_time_seconds(avFormatContext_->streams[videoStreamIndex_], avFrame->pts);
                    {
                        std::unique_lock<std::mutex> lck(decodedImagesMutex_);
                        hasDecodedImageCv_.wait(lck, [this]() { return decodedImages_.size() < kMaxDecodedFrames_ || state_ == State::kStateStopped || seekTo_ >= 0.0; });
                        if (seekTo_ < 0.0) {
                            decodedImages_.push_back(std::move(frameInfo));
                        }
                    }
                } while (true);
            } while (sendRet == AVERROR(EAGAIN));
        } else if (avPacket->stream_index == audioStreamIndex_) {
            ret = avcodec_send_packet(audioCodecContext, avPacket);
            if (ret < 0) {
                CHECK_AV_ERROR(ret);
                continue;
            }

            ret = avcodec_receive_frame(audioCodecContext, avFrame);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            } else if (ret < 0) {
                CHECK_AV_ERROR(ret);
                abort();
                return;
            }

            if (avFrame->format == AV_SAMPLE_FMT_FLTP) {
                int todo = MIN(audioResampler_.get_writer_space(), avFrame->nb_samples);
                if (todo < avFrame->nb_samples) {
                    ERR_PRINT("Discarding audio sample");
                }

                float* wb = audioResampler_.get_write_buffer();
                int c     = audioResampler_.get_channel_count();

                for (int i = 0, j = 0; i < todo; ++i) {
                    for (int k = 0; k < c; ++k) {
                        wb[j++] = reinterpret_cast<const float*>(avFrame->data[k])[i];
                    }
                }
                audioResampler_.write(todo);
            } else {
                ERR_PRINT("Unhandled audio sample format");
            }
        }
    }
}

int FfmpegMediaStream::hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type)
{
    AVBufferRef* hwDeviceCtx { nullptr };
    int err = av_hwdevice_ctx_create(&hwDeviceCtx, type, nullptr, nullptr, 0);
    if (err < 0) {
        ERR_PRINT("Failed to create specified HW device.");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
    hwBuffer_.reset(hwDeviceCtx);

    return err;
}

bool FfmpegMediaStream::try_apply_hw_accelerator(AVCodecContext* codecContext, const AVCodec* codec, const String& hw)
{
    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
        if (config == nullptr) {
            break;
        }
        auto deviceTypeName = get_hw_type_name(config);
        if (deviceTypeName != hw) {
            continue;
        }
        if (hw_decoder_init(codecContext, config->device_type) == 0) {
            return true;
        }
    }
    return false;
}