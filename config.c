/*
 * Copyright 2004-2008 Freescale Semiconductor, Inc. All Rights Reserved.
 * 
 * Author Erik Anvik "Au-Zone Technologies, Inc."  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU Lesser General 
 * Public License.  You may obtain a copy of the GNU Lesser General 
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 */

#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

/* Externals */
extern char *strndup(const char *s, size_t n);

/* Debug */
#define CFG_DBG
#ifdef CFG_DBG
extern char *v2ipd_logfile;
#define dbg(fmt, args...)  \
    do {                                                           \
        FILE *f = fopen(v2ipd_logfile, "a+");                      \
        fprintf(f, __FILE__ ": %s: " fmt, __func__, ## args); \
        fclose(f);                                                 \
    } while (0)
#else
#define dbg(fmt, args...) do {} while (0)
#endif

/* Local file param template */

static struct video_config_params video_template = {
        {"framerate=",      10, 0, 0},
        {"bitrate=",        8,  0, 0},
        {"compression=",    12, 0, 0},
        {"resolution=",     11, 0, 0},
        {"gop=",            4,  0, 0},
        {"rotation_angle=", 15, 0, 0},
        {"output_ratio=",   13, 0, 0},
        {"mirror_angle=",   13, 0, 0},
        {"name=",           5, 0, 0}
};

/* TODO - Implement for audio */
static struct audio_config_params audio_template;

/* Local helper methods */

static char * get_line(char *s, int size, FILE *stream, int *line,
                       char **_pos)
{
	char *pos, *end = NULL;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		/* Skip white space from the beginning of line. */
		while (*pos == ' ' || *pos == '\t' || *pos == '\r')
			pos++;

		/* Skip comment lines and empty lines */
		if (*pos == ';' || *pos == '\n' || *pos == '\0')
			continue;

		/* Remove trailing white space. */
		while (end > pos &&
		       (*end == '\n' || *end == ' ' || *end == '\t' ||
			*end == '\r'))
			*end-- = '\0';

		if (*pos == '\0')
			continue;

		if (_pos)
			*_pos = pos;
		return pos;
	}

	if (_pos)
		*_pos = NULL;
	return NULL;
}

static int populate_video(struct video_config_params *c, char *filename)
{
        FILE *fp;
        char buf[256];
        char *pos;
	int line = 0;

        if ((fp = fopen(filename, "r")) == NULL) {
                printf("%s input error\n", __func__);
                return -1;
        }

        /* Parse file and populate configuration params */
        while (get_line(buf, sizeof(buf), fp, &line, &pos)) {
                if (strncmp(pos, c->framerate.field_name, 
                                        c->framerate.field_len) == 0) {
                        c->framerate.field_val = atoi(pos + 
                                        c->framerate.field_len); 
                        dbg("%s=%d", c->framerate.field_name, 
                                        c->framerate.field_val);
                } else if (strncmp(pos, c->bitrate.field_name, 
                                        c->bitrate.field_len)
                        == 0) {
                        c->bitrate.field_val = atoi(pos + 
                                        c->bitrate.field_len); 
                        dbg("%s=%d", c->bitrate.field_name, 
                                        c->bitrate.field_val);
                } else if (strncmp(pos, c->compression.field_name,
                           c->compression.field_len) == 0) {
                        free(c->compression.field_str);
                        c->compression.field_str =
                        strndup(pos + c->compression.field_len, 5); 
                        dbg("%s=%s", c->compression.field_name, 
                                        c->compression.field_str);
                } else if (strncmp(pos, c->resolution.field_name,
                           c->resolution.field_len) == 0) {
                        free(c->resolution.field_str);
                        c->resolution.field_str = strndup(pos + 
                                        c->resolution.field_len, 7); 
                        dbg("%s=%s", c->resolution.field_name, 
                                        c->resolution.field_str);
                } else if (strncmp(pos, c->gop.field_name, 
                                        c->gop.field_len) == 0) {
                        c->gop.field_val = atoi(pos + c->gop.field_len); 
                        dbg("%s=%d", c->gop.field_name, c->gop.field_val);
                } else if (strncmp(pos, c->rotation_angle.field_name,
                           c->rotation_angle.field_len) == 0) {
                        c->rotation_angle.field_val =
                                atoi(pos + c->rotation_angle.field_len); 
                        dbg("%s=%d", c->rotation_angle.field_name,
                        c->rotation_angle.field_val);
                } else if (strncmp(pos, c->output_ratio.field_name,
                           c->output_ratio.field_len) == 0) {
                        c->output_ratio.field_val = atoi(pos + 
                                        c->output_ratio.field_len); 
                        dbg("%s=%d", c->output_ratio.field_name, 
                                        c->output_ratio.field_val);
                } else if (strncmp(pos, c->mirror_angle.field_name,
                           c->mirror_angle.field_len) == 0) {
                        c->mirror_angle.field_val = atoi(pos + 
                                        c->mirror_angle.field_len); 
                        dbg("%s=%d", c->mirror_angle.field_name, 
                                        c->mirror_angle.field_val);
                } else if (strncmp(pos, c->name.field_name,
                           c->name.field_len) == 0) {
                        free(c->name.field_str);    
                        c->name.field_str = strndup(pos + 
                                        c->name.field_len, 20); 
                        dbg("%s=%s", c->name.field_name, 
                                        c->name.field_str);
                } else ;
        }
        
        fclose(fp);
        
        return 0;
}

static int update_video(struct video_config_params *c, char *filename)
{
    FILE *fp;

        if ((fp = fopen(filename, "w")) == NULL) {
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
                strncmp("mjpeg", c->compression.field_str, 5) != 0)) {
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
                strncmp("4cif", c->resolution.field_str, 4) != 0)) {
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
                c->mirror_angle.field_val != 360) {
                c->mirror_angle.field_val = 0; 
        }
        fprintf(fp, "%s%d\n",  c->mirror_angle.field_name,
                c->mirror_angle.field_val);

        /* Validate mirror angle */
        if (c->rotation_angle.field_val != 0 &&
                c->rotation_angle.field_val != 90 &&
                c->rotation_angle.field_val != 180 &&
                c->rotation_angle.field_val != 270 &&
                c->rotation_angle.field_val != 360) {
                c->rotation_angle.field_val = 0; 
        }
        fprintf(fp, "%s%d\n",  c->rotation_angle.field_name,
                c->rotation_angle.field_val);

        fclose(fp);

        return 0;
}

static int populate_audio(struct audio_config_params *c, char *filename)
{
        dbg("not supported");
        return -1;
}

static int update_audio(struct audio_config_params *c, char *filename)
{
        dbg("not supported");
        return -1;
}

/**
 * parse_video_conf - parse video configuration file
 * @filename: configuration file
 * Returns video config context on success or < 0 on error
 */
struct video_config_params * parse_video_conf(char *filename)
{
        struct video_config_params *c;

        if (filename == NULL || (c = malloc(sizeof(*c))) == NULL) {
                printf("%s input e rror\n", __func__);     
                return NULL;
        }
        memset(c, 0, sizeof(*c));

        /* Populate config structure */ 
        memcpy(c, (char *) &video_template, sizeof(*c));
        if (populate_video(c, filename) < 0) {
                free(c);
                return NULL;
        } else
                return c;
}

/**
 * save_video_conf - commit video config to filename
 * @conf: configuration params
 * @filename: file to update
 * Returns 0 on success or < 0 on error
 */
int save_video_conf(struct video_config_params *conf, char *filename)
{
        if (conf == NULL || filename == NULL) {
                printf("%s input error\n", __func__);
                return -1;
        }

        if (update_video(conf, filename) < 0) {
                dbg("error");
                return -1;
        } else
                return 0;
}

/**
 * parse_audio_conf - parse audio configuration file
 * @filename: configuration file
 * Returns audio config context on success or < 0 on error
 */
struct audio_config_params * parse_audio_conf(char *filename)
{
        struct audio_config_params *c;

        if (filename == NULL || (c = malloc(sizeof(*c))) == NULL) {
                printf("%s input error\n", __func__);     
                return NULL;
        }
        memset(c, 0, sizeof(*c));

        /* Populate config structure */ 
        memcpy(c, (char *) &audio_template, sizeof(*c));
        if (populate_audio(c, filename) < 0) {
                free(c);
                return NULL;
        } else
                return c;
}

/**
 * save_audio_conf - commit audio config to filename
 * @conf: configuration params
 * @filename: file to update
 * Returns 0 on success or < 0 on error
 */
int save_audio_conf(struct audio_config_params *conf, char *filename)
{
        if (conf == NULL || filename == NULL) {
                printf("%s input error\n", __func__);
                return -1;
        }

        if (update_audio(conf, filename) < 0) {
                dbg("error");
                return -1;
        } else
                return 0;
}

/**
 * free_audio_conf - free audio config cache
 * @conf: configuration params
 * Returns 0 on success or < 0 on error
 */
int free_audio_conf(struct audio_config_params *conf)
{
        if (conf == NULL) {
                printf("%s error freeing config params\n", __func__);
                return -1;
        } else {
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
        if (conf == NULL) {
//                printf("%s error freeing config params\n", __func__);
                return -1;
        } else {
                free(conf->compression.field_str);
                free(conf->resolution.field_str);
                free(conf->name.field_str);
                free(conf);
                return 0;
        }
}

/**
 * video_params_changed - check if config params have changed
 * @conf: configuration params
 * Returns 1 on success or 0 
 */
int video_params_changed(struct video_config_params *conf)
{
    if (conf != NULL && 
            (conf->framerate.change_pending ||
            conf->bitrate.change_pending ||
            conf->name.change_pending ||
            conf->compression.change_pending ||
            conf->resolution.change_pending ||
            conf->gop.change_pending ||
            conf->rotation_angle.change_pending ||
            conf->output_ratio.change_pending ||
            conf->mirror_angle.change_pending))
            return 1;
    else
            return 0;
}





