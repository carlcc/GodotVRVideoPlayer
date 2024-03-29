#include "register_types.h"
#include "ffmpeg_media_stream.h"
#include "video_stream_ffmpeg.h"

static Ref<ResourceFormatLoaderFfmpeg> resource_loader_ffmpeg;

void initialize_ffmpeg_module_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    resource_loader_ffmpeg.instantiate();
    ResourceLoader::add_resource_format_loader(resource_loader_ffmpeg, true);

    GDREGISTER_CLASS(VideoStreamFfmpeg);
    GDREGISTER_CLASS(FfmpegCodec);
    GDREGISTER_CLASS(FfmpegCodecHwConfig);
    GDREGISTER_CLASS(VideoStreamPlaybackFfmpeg);
    GDREGISTER_CLASS(FfmpegMediaStream);
}

void uninitialize_ffmpeg_module_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    ResourceLoader::remove_resource_format_loader(resource_loader_ffmpeg);
    resource_loader_ffmpeg.unref();
}
