#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

static int update_video(struct video_config_params *c, char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL)
    {
        printf("%s input error\n", __func__);
        return -1;
    }

    /* Validate frame rate */
    if (c->framerate.field_val <= 0 || c->framerate.field_val > 30)
        c->framerate.field_val = 30; /* 30 fps as default */
    fprintf(fp, "%s%d\n", c->framerate.field_name,
            c->framerate.field_val);

    /* Validate bitrate */
    if (c->bitrate.field_val <= 0)
        c->bitrate.field_val = 0; /* VBR  as default */
    fprintf(fp, "%s%d\n", c->bitrate.field_name, c->bitrate.field_val);

    /* Validate compression */
    if (c->compression.field_str == NULL ||
            (strncmp("h264", c->compression.field_str, 4) != 0 &&
             strncmp("h263", c->compression.field_str, 4) != 0 &&
             strncmp("mpeg4", c->compression.field_str, 5) != 0 &&
             strncmp("mjpeg", c->compression.field_str, 5) != 0))
    {
        free(c->compression.field_str);
        c->compression.field_str =
            strdup("h264"); /* H264 default */
    }
    fprintf(fp, "%s%s\n",  c->compression.field_name,
            c->compression.field_str);

    /* Validate resolution */
    if (c->resolution.field_str == NULL ||
            (strncmp("d1_ntsc", c->resolution.field_str, 7) != 0 &&
             strncmp("d1_pal", c->resolution.field_str, 6) != 0 &&
             strncmp("vga", c->resolution.field_str, 3) != 0 &&
             strncmp("qcif", c->resolution.field_str, 4) != 0 &&
             strncmp("qvga", c->resolution.field_str, 4) != 0 &&
             strncmp("cif", c->resolution.field_str, 3) != 0 &&
             strncmp("sqcif", c->resolution.field_str, 5) != 0 &&
             strncmp("4cif", c->resolution.field_str, 4) != 0))
    {
        free(c->resolution.field_str);
        c->resolution.field_str = strdup("vga"); /* VGA default */
    }
    fprintf(fp, "%s%s\n",  c->resolution.field_name,
            c->resolution.field_str);

    /* Camera name */
    fprintf(fp, "%s%s\n",  c->name.field_name, c->name.field_str);

    /* Validate GOP */
    if (c->gop.field_val < 0 || c->gop.field_val > 60)
        c->gop.field_val = 0; /* First frame is I frame */
    fprintf(fp, "%s%d\n",  c->gop.field_name, c->gop.field_val);

    fprintf(fp, "%s%d\n", c->output_ratio.field_name,
            c->output_ratio.field_val);

    /* Validate mirror angle */
    if (c->mirror_angle.field_val != 0 &&
            c->mirror_angle.field_val != 90 &&
            c->mirror_angle.field_val != 180 &&
            c->mirror_angle.field_val != 270 &&
            c->mirror_angle.field_val != 360)
    {
        c->mirror_angle.field_val = 0;
    }
    fprintf(fp, "%s%d\n",  c->mirror_angle.field_name,
            c->mirror_angle.field_val);

    /* Validate mirror angle */
    if (c->rotation_angle.field_val != 0 &&
            c->rotation_angle.field_val != 90 &&
            c->rotation_angle.field_val != 180 &&
            c->rotation_angle.field_val != 270 &&
            c->rotation_angle.field_val != 360)
    {
        c->rotation_angle.field_val = 0;
    }
    fprintf(fp, "%s%d\n",  c->rotation_angle.field_name,
            c->rotation_angle.field_val);

    fclose(fp);

    return 0;
}

/**
 * save_video_conf - commit video config to filename
 * @conf: configuration params
 * @filename: file to update
 * Returns 0 on success or < 0 on error
 */
int save_video_conf(struct video_config_params *conf, char *filename)
{
    if (conf == NULL || filename == NULL)
    {
        printf("%s input error\n", __func__);
        return -1;
    }

    if (update_video(conf, filename) < 0)
        return -1;
    else
        return 0;
}

/**
 * free_audio_conf - free audio config cache
 * @conf: configuration params
 * Returns 0 on success or < 0 on error
 */
int free_audio_conf(struct audio_config_params *conf)
{
    if (conf == NULL)
    {
        printf("%s error freeing config params\n", __func__);
        return -1;
    }
    else
    {
        free(conf);
        return 0;
    }
}

/**
 * free_video_conf - free video config cache
 * @conf: configuration params
 * Returns 0 on success or < 0 on error
 */
int free_video_conf(struct video_config_params *conf)
{
    if (conf == NULL)
    {
//                printf("%s error freeing config params\n", __func__);
        return -1;
    }
    else
    {
        free(conf->compression.field_str);
        free(conf->resolution.field_str);
        free(conf->name.field_str);
        free(conf);
        return 0;
    }
}

