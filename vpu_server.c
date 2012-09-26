#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <linux/fs.h>

#include "vpu_server.h"
#include "cli.h"

#define FNT_DBG
#ifdef FNT_DBG
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt, __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif



int dev_mem_fd;

#define MULTI_PROCESS

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

/*
*************************** get the config file**********************************
*/

static struct video_config_params video_template =
{
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

/* Local helper methods */

static char * get_line(char *s, int size, FILE *stream, int *line,
                       char **_pos)
{
    char *pos, *end = NULL;

    while (fgets(s, size, stream))
    {
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

char *strndup(const char *s, size_t n);

static int populate_video(struct video_config_params *c, char *filename)
{
    FILE *fp;
    char buf[256];
    char *pos;
    int line = 0;

    if ((fp = fopen(filename, "r")) == NULL)
    {
        printf("%s input error\n", __func__);
        return -1;
    }

    /* Parse file and populate configuration params */
    while (get_line(buf, sizeof(buf), fp, &line, &pos))
    {
        if (strncmp(pos, c->framerate.field_name,
                    c->framerate.field_len) == 0)
        {
            c->framerate.field_val = atoi(pos +
                                          c->framerate.field_len);
        }
        else if (strncmp(pos, c->bitrate.field_name,
                         c->bitrate.field_len)
                 == 0)
        {
            c->bitrate.field_val = atoi(pos +
                                        c->bitrate.field_len);
        }
        else if (strncmp(pos, c->compression.field_name,
                         c->compression.field_len) == 0)
        {
            free(c->compression.field_str);
            c->compression.field_str =
                strndup(pos + c->compression.field_len, 5);
        }
        else if (strncmp(pos, c->resolution.field_name,
                         c->resolution.field_len) == 0)
        {
            free(c->resolution.field_str);
            c->resolution.field_str = strndup(pos +
                                              c->resolution.field_len, 7);
        }
        else if (strncmp(pos, c->gop.field_name,
                         c->gop.field_len) == 0)
        {
            c->gop.field_val = atoi(pos + c->gop.field_len);
        }
        else if (strncmp(pos, c->rotation_angle.field_name,
                         c->rotation_angle.field_len) == 0)
        {
            c->rotation_angle.field_val =
                atoi(pos + c->rotation_angle.field_len);
        }
        else if (strncmp(pos, c->output_ratio.field_name,
                         c->output_ratio.field_len) == 0)
        {
            c->output_ratio.field_val = atoi(pos +
                                             c->output_ratio.field_len);
        }
        else if (strncmp(pos, c->mirror_angle.field_name,
                         c->mirror_angle.field_len) == 0)
        {
            c->mirror_angle.field_val = atoi(pos +
                                             c->mirror_angle.field_len);
        }
        else if (strncmp(pos, c->name.field_name,
                         c->name.field_len) == 0)
        {
            free(c->name.field_str);
            c->name.field_str = strndup(pos +
                                        c->name.field_len, 20);
        }
        else ;
    }

    fclose(fp);

    return 0;
}

/**
 * parse_video_conf - parse video configuration file
 * @filename: configuration file
 * Returns video config context on success or < 0 on error
 */
static struct video_config_params * parse_video_conf(char *filename)
{
    struct video_config_params *c;

    if (filename == NULL || (c = malloc(sizeof(*c))) == NULL)
    {
        printf("%s input e rror\n", __func__);
        return NULL;
    }
    memset(c, 0, sizeof(*c));

    /* Populate config structure */
    memcpy(c, (char *) &video_template, sizeof(*c));
    if (populate_video(c, filename) < 0)
    {
        free(c);
        return NULL;
    }
    else
        return c;
}

static int get_video_resolution(struct video_config_params *p,int* width, int* height)
{
    char* str = NULL;
    /* Screen resolution */
    if (strncmp(p->resolution.field_str, "d1_ntsc", 7) == 0)
    {
        *width = 720;
        *height = 486;
        str = "D1 NTSC (720 x 486)";
    }
    else if (strncmp(p->resolution.field_str, "d1_pal", 6) == 0)
    {
        *width = 720;
        *height = 576;
        str = "D1 PAL (720 x 576)";
    }
    else if (strncmp(p->resolution.field_str, "qvga", 4) == 0)
    {
        *width = 320;
        *height = 240;
        str = "QVGA (320 x 240)";
    }
    else if (strncmp(p->resolution.field_str, "cif", 3) == 0)
    {
        *width = 352;
        *height = 288;
        str = "CIF (352 x 288)";
    }
    else if (strncmp(p->resolution.field_str, "qcif", 4) == 0)
    {
        *width = 176;
        *height = 144;
        str = "QCIF (176 x 144)";
    }
    else if (strncmp(p->resolution.field_str, "sqcif", 5) == 0)
    {
        *width = 128;
        *height = 96;
        str = "SQCIF (128 x 96)";
    }
    else if (strncmp(p->resolution.field_str, "4cif", 4) == 0)
    {
        *width = 704;
        *height = 576;
        str = "4CIF (704 x 576)";
    }
    else if (strncmp(p->resolution.field_str,
                     "svga", 4) == 0)
    {
        *width = 800;
        *height = 600;
        str = "SVGA (800 x 600)";
    }
    else
    {
        /* Defaults to VGA */
        *width = 640;
        *height = 480;
        str = "VGA (640 x 480)";
    }
//	dbg("resolution:%s\n", str);
    return 0;
}

static int get_video_bitrate(struct video_config_params *p,int* bitrate)
{
    *bitrate = p->bitrate.field_val;
    return 0;
}

static int get_video_framerate(struct video_config_params *p,int* framerate)
{
    *framerate = p->framerate.field_val;
    return 0;
}

static vs_record_parameter* record_para = NULL;
static vs_record_parameter* monitor_para = NULL;
vs_record_parameter* vs_get_record_para(void)
{
    struct video_config_params* video_config;
    if( record_para == NULL )
    {
        video_config = parse_video_conf(RECORD_PAR_FILE);
        if( NULL == video_config )
        {
            return NULL;
        }
        record_para = malloc( sizeof(vs_record_parameter) );
        if( NULL == record_para )
            return NULL;
        get_video_resolution(video_config, &record_para->width, &record_para->height);
        get_video_bitrate(video_config, &record_para->bitrate);
        get_video_framerate(video_config, &record_para->framerate);
        return record_para;
    }
    return record_para;
}

vs_record_parameter* vs_get_monitor_para(void)
{
    struct video_config_params* video_config;
    if( monitor_para == NULL )
    {
        video_config = parse_video_conf(MONITOR_PAR_FILE);
        if( NULL == video_config )
        {
            return NULL;
        }
        monitor_para = malloc( sizeof(vs_record_parameter) );
        if( NULL == monitor_para )
            return NULL;
        get_video_resolution(video_config, &monitor_para->width, &monitor_para->height);
        get_video_bitrate(video_config, &monitor_para->bitrate);
        get_video_framerate(video_config, &monitor_para->framerate);
        return monitor_para;
    }
    return monitor_para;
}

int vs_get_record_config()
{
    int fd;
    char* buffer;
    fd = open("/dev/nand-data", O_RDONLY);
    buffer = malloc( 10000 );
    if( NULL == buffer )
    {
        printf("malloc buffer error\n");
        close(fd);
        return -1;
    }
    memset(buffer, 0, 10000);
    if( read(fd, buffer, 10000) != 10000 )
    {
        printf("read error\n");
        close(fd);
        return -1;
    }
    close(fd);
//	dbg("get record mode: %d\n",buffer[4096]);
    return (int)buffer[4096];
}

int vs_set_record_config(int mode)
{
    int fd;
    char* buffer;

    printf("%s\n", __func__);
    fd = open("/dev/nand-data", O_RDWR|O_SYNC);
    if( fd < 0 )
    {
        perror("open nand-data");
        return -1;
    }
    buffer = malloc( 512*1024 );
    memset(buffer, 0, 512*1024);
    if( read(fd, buffer, 512*1024) != 512*1024 )
    {
        printf("read error\n");
        return -1;
    }
    buffer[4096] = (char)mode;
//	dbg("write data file\n");
    close(fd);
    fd = open("/dev/nand-data", O_RDWR|O_SYNC);
    if( fd < 0 )
    {
        perror("open nand-data");
        return -1;
    }
//	dbg("write file\n");
    write( fd, buffer, 512*1024 );
//	ioctl(fd, BLKFLSBUF, NULL );
    close(fd);
    system("sync");
    system("/nand-flush /dev/nand-data");
    system("sync");
    return 0;
}


