#ifndef CONFIG_H
#define CONFIG_H

#include "includes.h"
#include "defines.h"

struct config_params
{
    char field_name[256];
    int field_len;
    int field_val;
    char *field_str;
    int change_pending;
};

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

int save_video_conf(struct video_config_params *conf, char *filename);
struct audio_config_params * parse_audio_conf(char *filename);
int free_audio_conf(struct audio_config_params *conf);
int free_video_conf(struct video_config_params *conf);


#endif /* CONFIG_H */
