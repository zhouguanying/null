#ifndef CONFIG_H
#define CONFIG_H

#include "includes.h"
#include "defines.h"

/**
 * config_params - configuration parameters
 * @field_name: param name in file
 * @field_len: length of field descriptor
 * @field_val: value associated with field
 * @field_str: string value associated with field
 * @change_pending: Indicates param has changed
 */
struct config_params
{
    char field_name[256];
    int field_len;
    int field_val;
    char *field_str;
    int change_pending;
};

/**
 * video_config_params - video configuration parameters
 * @framerate: video frame rate
 * @bitrate: bitrate used by compressor
 * @resolution: capture device resolution
 * @gop: group of pictures format
 * @rotation_angle: output image rotation angle
 * @output_ration:
 * @mirror_angle: mirror angle
 * @name: camera name
 */
struct video_config_params
{
    struct config_params framerate;
    struct config_params bitrate;
    struct config_params compression;
    struct config_params resolution;
    struct config_params gop;
    struct config_params rotation_angle;
    struct config_params output_ratio;
    struct config_params mirror_angle;
    struct config_params name;
};

/**
 * audio_config_params - audio configuration parameters
 * @stereo: stream is mono or stereo
 * @io: input source or output destination
 * @sample_rate: rate at which audio is sampled
 * @sample_size: size at which audio is sampled
 * @buffer_size: audio buffer size
 * @full_duplex: full duplex operation supported
 */
struct audio_config_params
{
    struct io
    {
        struct config_params stereo;
        struct config_params io;
        struct config_params sample_rate;
        struct config_params sample_size;
        struct config_params buffer_size;
    } rec, play;
    struct config_params full_duplex;
};

/**
 * parse_video_conf - parse video configuration file
 * @filename: configuration file
 * Returns video config context on success or < 0 on error
 */
struct video_config_params * parse_video_conf(char *filename);

/**
 * save_video_conf - commit video config to filename
 * @conf: configuration params
 * @filename: file to update
 * Returns 0 on success or < 0 on error
 */
int save_video_conf(struct video_config_params *conf, char *filename);

/**
 * parse_audio_conf - parse audio configuration file
 * @filename: configuration file
 * Returns audio config context on success or < 0 on error
 */
struct audio_config_params * parse_audio_conf(char *filename);

/**
 * save_audio_conf - commit audio config to filename
 * @conf: configuration params
 * @filename: file to update
 * Returns 0 on success or < 0 on error
 */
int save_audio_conf(struct audio_config_params *conf, char *filename);

/**
 * free_audio_conf - free audio config cache
 * @conf: configuration params
 * Returns 0 on success or < 0 on error
 */
int free_audio_conf(struct audio_config_params *conf);

/**
 * free_video_conf - free video config cache
 * @conf: configuration params
 * Returns 0 on success or < 0 on error
 */
int free_video_conf(struct video_config_params *conf);

/**
 * video_params_changed - check if config params have changed
 * @conf: configuration params
 * Returns 1 on success or 0
 */
int video_params_changed(struct video_config_params *conf);


#endif /* CONFIG_H */
