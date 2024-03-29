#include "video_stream_ffmpeg.h"
#include <cassert>
#include <core/config/project_settings.h>
#include <thirdparty/misc/yuv2rgb.h>

void VideoStreamPlaybackFfmpeg::video_frame_write(AVFrame* frame)
{
    // TODO: other format
    int pitch = 4;
    frame_data_.resize(frame->width * frame->height * pitch);

    {
        auto* dst = frame_data_.ptrw();
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

    Ref<Image> img = memnew(Image(frame->width, frame->height, 0, Image::FORMAT_RGBA8, frame_data_)); // zero copy image creation

    texture_->set_image(img); // zero copy send to rendering server

    frames_pending_ = 1;
}

void VideoStreamPlaybackFfmpeg::clear()
{
    avioWrapper_   = nullptr;
    formatContext_ = nullptr;
    codecContext_  = nullptr;
    packet_        = nullptr;
    frame_         = nullptr;

    frames_pending_ = 0;
    videoStreamIndices_.clear();
    audioStreamIndices_.clear();
    subtitleStreamIndices_.clear();

    playing_            = false;
    paused_             = false;
    time_               = 0.0;
    delay_compensation_ = 0.0;
    videobuf_time_      = 0.0;
}

void VideoStreamPlaybackFfmpeg::play()
{
    if (!playing_) {
        time_ = 0;
    } else {
        stop();
    }

    playing_ = true;

    delay_compensation_ = ProjectSettings::get_singleton()->get("audio/video/video_delay_compensation_ms");
    delay_compensation_ /= 1000.0;
}

void VideoStreamPlaybackFfmpeg::stop()
{
    if (playing_) {
        clear();
        set_file(file_name_); // reset
    }
    playing_ = false;
    time_    = 0; // TODO:
}

bool VideoStreamPlaybackFfmpeg::is_playing() const
{
    return playing_;
}

void VideoStreamPlaybackFfmpeg::set_paused(bool p_paused)
{
    paused_ = p_paused;
}

bool VideoStreamPlaybackFfmpeg::is_paused() const
{
    return paused_;
}

void VideoStreamPlaybackFfmpeg::set_loop(bool p_enable)
{
}

bool VideoStreamPlaybackFfmpeg::has_loop() const
{
    return false;
}

double VideoStreamPlaybackFfmpeg::get_length() const
{
    return 0;
}

String VideoStreamPlaybackFfmpeg::get_stream_name() const
{
    return "";
}

int VideoStreamPlaybackFfmpeg::get_loop_count() const
{
    return 0;
}

double VideoStreamPlaybackFfmpeg::get_playback_position() const
{
    return get_time();
}

void VideoStreamPlaybackFfmpeg::seek(double p_time)
{
    WARN_PRINT_ONCE("Seeking in not yet implemented");
}

void VideoStreamPlaybackFfmpeg::set_file(const String& p_file)
{
    time_ = 0.0;

    avioWrapper_ = std::make_unique<AvIoContextWrapper>(p_file);
    auto* avio   = avioWrapper_.get();

    if (avio->file.is_null()) {
        avioWrapper_ = nullptr;
        ERR_PRINT("Cannot open file '" + p_file + "'.");
        return;
    }

    file_name_ = p_file;
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
    formatContext_.reset(formatContext);
    ret = avformat_open_input(&formatContext, "", inputFormat, nullptr);
    if (ret != 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, ret);
        ERR_PRINT(String("Failed to call avformat_open_input(): {0}, {1}").format(varray(buf, p_file)));

        // TODO:
        return;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        ERR_PRINT("Failed to find stream info");
        return;
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
        ERR_PRINT("Failed to find a video stream.");
        return;
    }

    // decoder
    packet_.reset(av_packet_alloc());
    frame_.reset(av_frame_alloc());

    if (!videoStreamIndices_.is_empty()) { // TODO: Multiple stream?
        videoStreamIndex_ = videoStreamIndices_[0];
        auto codecId      = formatContext_->streams[videoStreamIndex_]->codecpar->codec_id;
        auto* codec       = avcodec_find_decoder(codecId);
        auto codecContext = avcodec_alloc_context3(codec);
        codecContext_.reset(codecContext);
        avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex_]->codecpar);
        codecContext->thread_count = 8;
        if (0 != avcodec_open2(codecContext, codec, nullptr)) {
            ERR_PRINT(String("Open codec {0} failed").format(avcodec_get_name(codecId)));
            codecContext_ = nullptr;
        }
    }

    // TODO: Audio
}

Ref<Texture2D> VideoStreamPlaybackFfmpeg::get_texture() const
{
    return texture_;
}

#define CHECK_AV_ERROR(errcode)                            \
    do {                                                   \
        if (errcode == 0)                                  \
            break;                                         \
        char errBuf[512];                                  \
        av_strerror(ret, errBuf, sizeof(errBuf));          \
        ERR_PRINT(String("av error: {0}").format(errBuf)); \
    } while (false)

void VideoStreamPlaybackFfmpeg::update(double p_delta)
{
    if (avioWrapper_ == nullptr || avioWrapper_->file.is_null()) {
        return;
    }

    if (!playing_ || paused_) {
        // printf("not playing\n");
        return;
    };

    time_ += p_delta;

    if (videobuf_time_ > get_time()) {
        return; // no new frames need to be produced
    }

    auto* codecContext = codecContext_.get();
    auto* avPacket     = packet_.get();
    auto* avFrame      = frame_.get();
    bool frame_done    = false;
    while (!frame_done) {
        auto ret = av_read_frame(formatContext_.get(), packet_.get());
        std::unique_ptr<AVPacket, AvPacketDeleter> unrefOnExitScope(packet_.get());

        if (ret == AVERROR_EOF) {
            // L_INFO("EOF, replay from start");
            // int kSeekFlags[] {
            //     AVSEEK_FLAG_BACKWARD,
            //     AVSEEK_FLAG_BYTE,
            //     AVSEEK_FLAG_ANY,
            //     AVSEEK_FLAG_FRAME,
            // };
            // // 不同文件支持的seek模式不一样，几种seek模式都试试
            // for (auto flag : kSeekFlags) {
            //     ret = av_seek_frame(formatContext_.get() - 1,videoStreamIndex_, 0, flag);
            //     if (ret == 0) {
            //         break;
            //     }
            // }
            // if (ret != 0) {
            //     L_ERROR("Cannot rewind video stream, stop capturing");
            //     return;
            // }
            WARN_PRINT("Video stream ended!");
            stop();
            break;
        }
        if (avPacket->stream_index != videoStreamIndex_) {
            continue;
        }

        {
            ret = avcodec_send_packet(codecContext, avPacket);
            if (ret < 0) {
                CHECK_AV_ERROR(ret);
                continue;
            }

            ret = avcodec_receive_frame(codecContext, avFrame);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            } else if (ret < 0) {
                CHECK_AV_ERROR(ret);
                return;
            }

            video_frame_write(avFrame);
            frame_done     = true;
            videobuf_time_ = avFrame->pts * avFrame->time_base.num / (double)avFrame->time_base.den;
        }
    }
}

void VideoStreamPlaybackFfmpeg::set_mix_callback(AudioMixCallback p_callback, void* p_userdata)
{
    // TODO:
}

int VideoStreamPlaybackFfmpeg::get_channels() const
{
    // TODO:
    return 0;
}

int VideoStreamPlaybackFfmpeg::get_mix_rate() const
{
    // TODO:
    return 0;
}

void VideoStreamPlaybackFfmpeg::set_audio_track(int p_idx)
{
    // TODO:
}

VideoStreamPlaybackFfmpeg::VideoStreamPlaybackFfmpeg()
{
    texture_ = Ref<ImageTexture>(memnew(ImageTexture));
}

VideoStreamPlaybackFfmpeg::~VideoStreamPlaybackFfmpeg()
{
}

void VideoStreamPlaybackFfmpeg::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_texture"), &VideoStreamPlaybackFfmpeg::get_texture);
    ClassDB::bind_method(D_METHOD("update", "delta"), &VideoStreamPlaybackFfmpeg::update);
    ClassDB::bind_method(D_METHOD("play"), &VideoStreamPlaybackFfmpeg::play);
    ClassDB::bind_method(D_METHOD("stop"), &VideoStreamPlaybackFfmpeg::stop);
    // ClassDB::bind_method(D_METHOD("pause"), &VideoStreamPlaybackFfmpeg::update);
}

void VideoStreamFfmpeg::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_file", "file"), &VideoStreamFfmpeg::set_file);
    ClassDB::bind_method(D_METHOD("get_file"), &VideoStreamFfmpeg::get_file);
    ClassDB::bind_method(D_METHOD("instantiate_playback"), &VideoStreamFfmpeg::instantiate_playback);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "file", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_file", "get_file");
}

Ref<Resource> ResourceFormatLoaderFfmpeg::load(const String& p_path, const String& p_original_path, Error* r_error, bool p_use_sub_threads, float* r_progress, ResourceFormatLoader::CacheMode p_cache_mode)
{
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
    if (f.is_null()) {
        if (r_error) {
            *r_error = ERR_CANT_OPEN;
        }
        return Ref<Resource>();
    }

    VideoStreamFfmpeg* stream = memnew(VideoStreamFfmpeg);
    stream->set_file(p_path);

    Ref<VideoStreamFfmpeg> ogv_stream = Ref<VideoStreamFfmpeg>(stream);

    if (r_error) {
        *r_error = OK;
    }

    return ogv_stream;
}

static HashSet<String>& get_all_supported_extensions()
{
    struct FfmpeSupportedFormats {
        FfmpeSupportedFormats()
        {
            h.insert("mp4");
            h.insert("avi");
            h.insert("mkv");
            h.insert("flv");
        }
        HashSet<String> h;
    };

    static FfmpeSupportedFormats f;
    return f.h;
};

void ResourceFormatLoaderFfmpeg::get_recognized_extensions(List<String>* p_extensions) const
{
    for (auto& s : get_all_supported_extensions()) {
        p_extensions->push_back(s);
    }
}

bool ResourceFormatLoaderFfmpeg::handles_type(const String& p_type) const
{
    return ClassDB::is_parent_class(p_type, "VideoStream");
}

String ResourceFormatLoaderFfmpeg::get_resource_type(const String& p_path) const
{
    String el = p_path.get_extension().to_lower();
    if (get_all_supported_extensions().has(el)) {
        return "VideoStreamFfmpeg";
    }
    return "";
}
