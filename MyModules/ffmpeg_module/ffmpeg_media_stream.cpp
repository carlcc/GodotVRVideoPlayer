#include "ffmpeg_media_stream.h"
#include <scene/audio/audio_stream_player.h>
#include <thirdparty/misc/yuv2rgb.h>

static double get_stream_time_seconds(AVStream* stream, int64_t pts)
{
    return pts * stream->time_base.num / (double)stream->time_base.den;
}

static double get_stream_duration(AVStream* stream)
{
    return get_stream_time_seconds(stream, stream->duration);
}

void FfmpegMediaStream::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_texture"), &FfmpegMediaStream::get_texture);
    ClassDB::bind_method(D_METHOD("update", "delta"), &FfmpegMediaStream::update);
    ClassDB::bind_method(D_METHOD("play"), &FfmpegMediaStream::play);
    ClassDB::bind_method(D_METHOD("stop"), &FfmpegMediaStream::stop);
    ClassDB::bind_method(D_METHOD("pause"), &FfmpegMediaStream::pause);
    ClassDB::bind_method(D_METHOD("get_length"), &FfmpegMediaStream::get_length);
    ClassDB::bind_method(D_METHOD("get_position"), &FfmpegMediaStream::get_position);
    ClassDB::bind_method(D_METHOD("seek", "position"), &FfmpegMediaStream::seek);
    ClassDB::bind_method(D_METHOD("init", "filePath"), &FfmpegMediaStream::init);
}

bool FfmpegMediaStream::init(const String& filePath)
{
    avioContext_ = std::make_unique<AvIoContextWrapper>(filePath);
    auto* avio   = avioContext_.get();

    if (avio->file.is_null()) {
        ERR_PRINT("Cannot open file '" + filePath + "'.");
        return false;
    }

    filePath_ = filePath;
    //
    const AVInputFormat* inputFormat = nullptr;

    auto ret = av_probe_input_buffer(avio->context, &inputFormat, "", nullptr, 0, 0);
    if (ret < 0) {
        // TODO:
    } else {
        // TODO:
    }

    AVFormatContext* formatContext = avformat_alloc_context();
    formatContext->pb              = avio->context;
    formatContext->flags           = AVFMT_FLAG_CUSTOM_IO;
    avFormatContext_.reset(formatContext);
    ret = avformat_open_input(&formatContext, "", inputFormat, nullptr);
    if (ret != 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        ERR_PRINT(String("Failed to call avformat_open_input(): {0}, {1}").format(varray(buf, filePath)));

        return false;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        ERR_PRINT("Failed to find stream info");
        return false;
    }
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

    if (videoStreamIndices_.is_empty() && audioStreamIndices_.is_empty()) {
        ERR_PRINT("Failed to find a video stream and audio stream.");
        return false;
    }

    // decoder
    avPacket_.reset(av_packet_alloc());
    avFrame_.reset(av_frame_alloc());

    if (!videoStreamIndices_.is_empty()) { // TODO: Multiple stream?
        videoStreamIndex_ = videoStreamIndices_[0];
        auto codecId      = avFormatContext_->streams[videoStreamIndex_]->codecpar->codec_id;
        auto* codec       = avcodec_find_decoder(codecId);
        auto codecContext = avcodec_alloc_context3(codec);
        videoCodecContext_.reset(codecContext);
        avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex_]->codecpar);
        codecContext->thread_count = 8;
        if (0 != avcodec_open2(codecContext, codec, nullptr)) {
            ERR_PRINT(String("Open codec {0} failed").format(avcodec_get_name(codecId)));
            return false;
        }
    }
    if (!audioStreamIndices_.is_empty()) {
        audioStreamIndex_ = audioStreamIndices_[0];
        auto codecId      = avFormatContext_->streams[audioStreamIndex_]->codecpar->codec_id;
        auto* codec       = avcodec_find_decoder(codecId);
        auto codecContext = avcodec_alloc_context3(codec);
        audioCodecContext_.reset(codecContext);
        avcodec_parameters_to_context(codecContext, formatContext->streams[audioStreamIndex_]->codecpar);
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

FfmpegMediaStream::FfmpegMediaStream()
{
    texture_ = Ref<ImageTexture>(memnew(ImageTexture));
}

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
    if (state_ == State::kPlaying) {
        return;
    }
    if (state_ == State::kStopped) {
        assert(!decodingThread_.joinable());
        decodingThread_ = std::thread([this]() {
            decode_thread_routine();
        });
        if (audioStreamIndex_ != -1) {
            mixBuffer_.resize(AudioServer::get_singleton()->thread_get_mix_buffer_size()); // resize before register
            AudioServer::get_singleton()->add_mix_callback(&FfmpegMediaStream::static_mix, this);
        }
    }

    state_ = State::kPlaying;
}

void FfmpegMediaStream::stop()
{
    AudioServer::get_singleton()->remove_mix_callback(&FfmpegMediaStream::static_mix, this);

    {
        std::unique_lock<std::mutex> lck(decodedImagesMutex_);
        state_ = State::kStopped;
    }
    hasDecodedImageCv_.notify_one();
    if (decodingThread_.joinable()) {
        decodingThread_.join();
    }
}

void FfmpegMediaStream::pause()
{
    if (state_ != State::kPlaying) {
        ERR_PRINT("The stream is not being played");
        return;
    }

    state_ = State::kPaused;
}

void FfmpegMediaStream::update(double delta)
{
    if (state_ != State::kPlaying) {
        return;
    }
    time_ += delta;

    Ref<Image> img { nullptr };
    {
        std::unique_lock<std::mutex> lck(decodedImagesMutex_);
        if (!decodedImages_.empty()) {
            auto frameTime = decodedImages_.front().frameTime;
            if (time_ > frameTime) {
                img = decodedImages_.front().image;
                decodedImages_.pop();

                lck.unlock();
                hasDecodedImageCv_.notify_one();
            }
        }
    }
    if (img != nullptr) {
        if (textureWidth_ != img->get_width() || textureHeight_ != img->get_height()) {
            texture_->set_image(img);
            textureWidth_  = img->get_width();
            textureHeight_ = img->get_height();
        } else {
            texture_->update(img);
        }
    }
}

double FfmpegMediaStream::get_length() const
{
    return get_stream_duration(avFormatContext_->streams[videoStreamIndex_]); // TODO:
}

double FfmpegMediaStream::get_position() const
{
    return time_;
}

double FfmpegMediaStream::seek(double position)
{
    return 0;
}

#define CHECK_AV_ERROR(errcode)                            \
    do {                                                   \
        if (errcode == 0)                                  \
            break;                                         \
        char errBuf[512];                                  \
        av_strerror(ret, errBuf, sizeof(errBuf));          \
        ERR_PRINT(String("av error: {0}").format(errBuf)); \
    } while (false)

static Ref<Image> AVFrame2Image(AVFrame* frame)
{
    // TODO: other format
    int pitch = 4;
    Vector<uint8_t> rgbBuffer;
    rgbBuffer.resize(frame->width * frame->height * pitch);

    {
        auto* dst = rgbBuffer.ptrw();
        if (frame->format == AV_PIX_FMT_YUV420P) {
            yuv420_2_rgb8888(dst,
                frame->data[0],              // y
                frame->data[1],              // u
                frame->data[2],              // v
                frame->width, frame->height, // w, h
                frame->linesize[0],          // y stride
                frame->linesize[1],          // uv stride
                frame->width * 4             // dst stride
            );

        } else {
            abort(); // TODO:
        }
    }

    // zero copy image creation
    Ref<Image> img = memnew(Image(frame->width, frame->height, 0, Image::FORMAT_RGBA8, rgbBuffer));
    return img;
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

    auto& videoTimeBase = formatContext->streams[videoStreamIndex_]->time_base;

    while (state_ != State::kStopped) {
        auto ret = av_read_frame(formatContext, avPacket);
        std::unique_ptr<AVPacket, AvPacketDeleter> unrefOnExitScope(avPacket); // we need to unref the packet

        if (ret == AVERROR_EOF) {
            WARN_PRINT("Video stream ended!");
            return;
        }

        // TODO: Audio
        if (avPacket->stream_index == videoStreamIndex_) {
            ret = avcodec_send_packet(videoCodecContext, avPacket);
            if (ret < 0) {
                CHECK_AV_ERROR(ret);
                continue;
            }

            ret = avcodec_receive_frame(videoCodecContext, avFrame);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            } else if (ret < 0) {
                CHECK_AV_ERROR(ret);
                abort();
                return;
            }

            auto img = AVFrame2Image(avFrame);
            // TODO: The right way to calculate frame time
            auto frameTime = get_stream_time_seconds(avFormatContext_->streams[videoStreamIndex_], avFrame->pts);
            {
                std::unique_lock<std::mutex> lck(decodedImagesMutex_);
                hasDecodedImageCv_.wait(lck, [this]() { return decodedImages_.size() < kMaxDecodedFrames_ || state_ == State::kStopped; });
                decodedImages_.push(FrameInfo { frameTime, std::move(img) });
            }
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
