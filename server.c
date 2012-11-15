#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include "includes.h"
#include "mail_alarm.h"
#include "cli.h"
#include "config.h"
#include "revision.h"
#include "nand_file.h"
#include "vpu_server.h"
#include "server.h"
#include "playback.h"
#include "record_file.h"
#include "utilities.h"
#include "v4l2uvc.h"
#include "sound.h"
#include "picture_info.h"
#include "video_cfg.h"
#include "log_dbg.h"
#include "mediaEncode.h"

#define PID_FILE    "/var/run/v2ipd.pid"
#define LOG_FILE    "/tmp/v2ipd.log"
char *v2ipd_logfile = LOG_FILE; /* Fix me */

#define MAX_SEND_PACKET_NUM	5

int nand_fd;

extern void (*sigset(int sig, void (*disp)(int)))(int);
extern int sigignore(int sig);

#define ENCODER_DBG
#ifdef ENCODER_DBG
extern char *v2ipd_logfile;
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)    do {} while (0)
#endif

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

static pthread_mutex_t     v_thread_sleep_t_lock;
static int v_thread_sleep_time ;
static unsigned char do_update_flag = 0;

pthread_mutex_t global_ctx_lock;
struct sess_ctx *global_ctx_running_list = NULL;
int session_number = 0;

struct threadconfig threadcfg;
int msqid = -1;

pthread_mutex_t g_sess_id_mask_lock;
char g_sess_id_mask[MAX_NUM_IDS];
int daemon_msg_queue;
vs_share_mem* v2ipd_share_mem;

static int need_I_frame = 0;

static void send_alive(void)
{
    if (msqid < 0)
        return;

    int ret = -1;
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_RECORD_ALIVE;
    msg.msg[1] = 0;
    //gettimeofday(&alive_old_time, NULL);
    ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
}

int is_do_update()
{
    int update;
    update = (int)do_update_flag;
    return update;
}

void set_do_update()
{
    do_update_flag = 1;
}

void set_msg_do_update()
{
    int ret;
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_DO_UPDATE;
    msg.msg[1] = 0;
    ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
    if (ret == -1)
    {
        system("reboot &");
        exit(0);
    }
    exit(0);
}

void prepare_do_update()
{
    set_msg_do_update();
}

unsigned char checksum(unsigned char cksum, unsigned char *data, int size)
{
    int i;

    for (i = 0; i < size; i++)
        cksum ^= data[i];

    return cksum;
}

void init_sleep_time()
{
    pthread_mutex_init(&v_thread_sleep_t_lock , NULL);
    v_thread_sleep_time = (1000000 / threadcfg.framerate) >> 1;
}

void handle_video_thread()
{
    usleep(v_thread_sleep_time);
}

void init_g_sess_id_mask()
{
    pthread_mutex_init(&g_sess_id_mask_lock, NULL);
    memset(g_sess_id_mask, 0, sizeof(g_sess_id_mask));
    g_sess_id_mask[MAX_NUM_IDS - 1] = 1;
}

int get_sess_id()
{
    int i;
    int ret;
    ret = -1;
    pthread_mutex_lock(&g_sess_id_mask_lock);
    for (i = 0; i < MAX_NUM_IDS; i++)
    {
        if (g_sess_id_mask[i] == 0)
        {
            g_sess_id_mask[i] = 1;
            ret  = i;
            break;
        }
    }
    pthread_mutex_unlock(&g_sess_id_mask_lock);
    return ret;
}

void put_sess_id(int index)
{
    pthread_mutex_lock(&g_sess_id_mask_lock);
    g_sess_id_mask[index] = 0;
    pthread_mutex_unlock(&g_sess_id_mask_lock);
}

void v2ipd_restart_all()
{
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_RESTART_ALL;
    if (msgsnd(daemon_msg_queue, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0)
    {
        perror("msgsnd");
    }
    return;
}

void v2ipd_reboot_system()
{
    vs_ctl_message msg;
    system("/tmp/reboot-delay.sh &");
    dbg("reboot system\n");
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_REBOOT_SYSTEM;
    if (msgsnd(daemon_msg_queue, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0)
    {
        perror("msgsnd");
    }
    return;
}

void v2ipd_disable_write_nand()
{
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_DISABLE_WIRTE_NAND;
    dbg("disable write nand at first\n");
    if (msgsnd(daemon_msg_queue, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0)
    {
        perror("msgsnd");
    }
    return;
}

void v2ipd_request_timeover_protect()
{
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_REQUEST_TIMEOVER_PROTECT;
    if (msgsnd(daemon_msg_queue, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0)
    {
        perror("msgsnd");
    }
    return;
}

void v2ipd_stop_timeover_protect()
{
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_STOP_TIMEOVER_PROTECT;
    if (msgsnd(daemon_msg_queue, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) < 0)
    {
        perror("msgsnd");
    }
    return;
}

static inline int kill_tcp_connection(struct sess_ctx *sess)
{
    shutdown(sess->s2, 1);
    close(sess->s2);
    close(sess->s1);
    free(sess->myaddr);
    sess->myaddr = NULL;
    sess->is_tcp = 0;
    sess->s2 = -1;
    sess->s1 = -1;
    return 0;
}

static inline int kill_video(struct sess_ctx *sess)
{
    while (1)
        ;
    free_system_session(sess);
    return 0;
}

static inline int kill_connection(struct sess_ctx *sess)
{
    struct sess_ctx * tmp;
    sess->connected = 0;
    sess->playing = 0;
    sess->paused = 0;
    sess->motion_detected = 0;

    if (sess->is_tcp)
        ;
    else if (sess->is_file)
    {
        if (sess->file_fd >= 0)
        {
            close(sess->file_fd);
            sess->file_fd = -1;
        }
        sess->is_file = 0;
    }
    else ;
    pthread_mutex_lock(&global_ctx_lock);
    tmp = global_ctx_running_list;
    if (tmp == sess)
    {
        global_ctx_running_list = global_ctx_running_list->next;
        session_number--;
    }
    else
    {
        while (tmp->next != NULL)
        {
            if (tmp->next == sess)
            {
                tmp->next = sess->next;
                session_number--;
                break;
            }
            tmp = tmp->next;
        }
    }
    pthread_mutex_unlock(&global_ctx_lock);
    return kill_video(sess);
}

int free_system_session(struct sess_ctx *sess)
{
    if (sess == NULL)
        return -1;
    if (sess->name != NULL)
        free(sess->name);
    if (sess->video.fp != NULL)
        fclose(sess->video.fp);
    if (sess->audio.fp != NULL)
        fclose(sess->audio.fp);
    if (sess->video.params != NULL)
        free_video_conf(sess->video.params);
    if (sess->audio.params != NULL)
        free_audio_conf(sess->audio.params);
    if (sess->audio.cfgfile != NULL)
        free(sess->audio.cfgfile);
    if (sess->video.cfgfile != NULL)
        free(sess->video.cfgfile);
    if (sess->s1 >= 0)
        close(sess->s1);
    if (sess->s2 >= 0)
        close(sess->s2);
    if (sess->to != NULL)
        free(sess->to);
    if (sess->myaddr != NULL)
        free(sess->myaddr);
    if (sess->pipe_fd >= 0)
        close(sess->pipe_fd);
    if (sess->pipe_name != NULL)
    {
        unlink(sess->pipe_name);
        free(sess->pipe_name);
    }
    if (sess->file_fd >= 0)
        close(sess->file_fd);

#ifdef USE_CLI
    if (sess->cli_sess != NULL)
        cli_deinit(sess->cli_sess);
#endif
    if (sess->id >= 0)
        put_sess_id(sess->id);
    pthread_mutex_destroy(&sess->sesslock);
    free(sess);

    return 0;
}

struct sess_ctx *new_system_session(char *name)
{
    struct sess_ctx *sess = NULL;

    if (name == NULL)
        return NULL;

    sess = malloc(sizeof(*sess));
    memset(sess, 0, sizeof(struct sess_ctx));
    sess->id = get_sess_id();
    if (sess->id < 0)
    {
        free(sess);
        return NULL;
    }
    sess->s1 = -1;
    sess->s2 = -1;
    sess->pipe_fd = -1;
    sess->file_fd = -1;

    sess->name = malloc(strlen(name) + 1);
    if (sess->name == NULL)
        goto error1;
    memset(sess->name, 0, strlen(name) + 1);
    memcpy(sess->name, name, strlen(name));

    pthread_mutex_init(&sess->sesslock, NULL);

    return sess;

error1:
    put_sess_id(sess->id);
    free(sess);

    return NULL;
}

static void sig_handler(int signum)
{
    if (signum == SIGINT || signum == SIGIO)
        exit(-1);
    else if (signum == SIGPIPE)
        dbg("SIGPIPE\n");
}

struct vdIn *vdin_camera = NULL;

static char* get_data(int* size, int *width, int *height)
{
    const char *videodevice = "/dev/video0";
#if !defined IPED_98
    int format = V4L2_PIX_FMT_MJPEG;
#else
	int format = V4L2_PIX_FMT_MJPEG;
#endif
    int grabmethod = 1;
    char* buf;
    int trygrab = 5;

    pthread_mutex_lock(&vdin_camera->tmpbufflock);
retry:
    if (uvcGrab(vdin_camera) < 0)
    {
        trygrab--;
        log_debug("uvcGrad failed trygrab %d\n", trygrab);
        if (trygrab <= 0)
        {
            close_v4l2(vdin_camera);
            if (init_videoIn(vdin_camera, (char *) videodevice, vdin_camera->width, vdin_camera->height, format, grabmethod) < 0)
            {
                log_warning("init camera device error\n");
                exit(0);
            }
        }
        usleep(100000);
        goto retry;
    }
    send_alive();
#if !defined IPED_98
    buf = malloc(vdin_camera->buf.bytesused + DHT_SIZE);
#else
    buf = malloc(vdin_camera->buf.bytesused);
#endif
    if (!buf)
    {
        printf("malloc buf error\n");
        *size = 0;
        pthread_mutex_unlock(&vdin_camera->tmpbufflock);
        return 0;
    }
#if 0
    memcpy(buf, vdin_camera->tmpbuffer + sizeof(picture_info_t), vdin_camera->buf.bytesused + DHT_SIZE);
    *size = vdin_camera->buf.bytesused + DHT_SIZE;
#else
    memcpy(buf, vdin_camera->framebuffer, vdin_camera->buf.bytesused);
    *size = vdin_camera->buf.bytesused;
#endif
    *width = vdin_camera->width;
    *height = vdin_camera->height;
    pthread_mutex_unlock(&vdin_camera->tmpbufflock);
    return buf;
}

void restart_v4l2(int width , int height);

void change_camera_status(struct sess_ctx *sess , int sess_in)
{
    return ;
    if (strncmp(threadcfg.monitor_mode , "inteligent", 10) != 0)
        return;
    if (sess_in)
    {
        if (sess->sc->video_socket_is_lan || strncmp(threadcfg.monitor_mode , "inteligent2",11)==0)
        {
            if (strncmp(threadcfg.record_resolution, "vga", 3) != 0)
            {
                memset(threadcfg.record_resolution , 0 , sizeof(threadcfg.record_resolution));
                sprintf(threadcfg.record_resolution , "vga");
                memcpy(threadcfg.resolution , threadcfg.record_resolution , sizeof(threadcfg.resolution));
                restart_v4l2(640, 480);
            }
        }
        else
        {
            if (strncmp(threadcfg.record_resolution, "qvga", 4) != 0)
            {
                memset(threadcfg.record_resolution , 0 , sizeof(threadcfg.record_resolution));
                sprintf(threadcfg.record_resolution , "qvga");
                memcpy(threadcfg.resolution , threadcfg.record_resolution , sizeof(threadcfg.resolution));
                restart_v4l2(320, 240);
            }
        }
    }
    else
    {
        if (strncmp(threadcfg.record_resolution, threadcfg.original_resolution, strlen(threadcfg.original_resolution)) != 0)
        {
            memcpy(threadcfg.record_resolution , threadcfg.original_resolution , sizeof(threadcfg.original_resolution));
            memcpy(threadcfg.resolution , threadcfg.original_resolution , sizeof(threadcfg.original_resolution));
            if (strncmp(threadcfg.original_resolution, "vga", 3) == 0)
                restart_v4l2(640, 480);
            else
                restart_v4l2(320, 240);
        }
    }
}

void add_sess(struct sess_ctx *sess)
{
//    pthread_mutex_lock(&global_ctx_lock);
    sess->next = global_ctx_running_list;
    global_ctx_running_list = sess;
    session_number ++;
    if (session_number == 1)
        change_camera_status(sess, 1);
//   pthread_mutex_unlock(&global_ctx_lock);
}

void  del_sess(struct sess_ctx *sess)
{
    struct sess_ctx **p;
    pthread_mutex_lock(&global_ctx_lock);
    p = &global_ctx_running_list;
    while (*p != NULL)
    {
        if ((*p) == sess)
        {
            *p = (*p)->next;
            session_number--;
            if (g_cli_ctx->arg == sess)
                g_cli_ctx->arg = NULL;
            if (session_number == 0)
                change_camera_status(sess, 0);
            break;
        }
        p = &((*p)->next);
    }
    pthread_mutex_unlock(&global_ctx_lock);
}

void take_sess_up(struct sess_ctx *sess)
{
    pthread_mutex_lock(&sess->sesslock);
    sess->running = 1;
    sess->soft_reset = 0;
    pthread_mutex_unlock(&sess->sesslock);
}

void take_sess_down(struct sess_ctx *sess)
{
    if (!sess)
        return;
    sess->sc->close_all = 1;
    pthread_mutex_lock(&sess->sesslock);
    sess->ucount--;
    sess->running = 0;
    if (sess->ucount <= 0)
    {
        pthread_mutex_unlock(&sess->sesslock);
        free_system_session(sess);
    }
    else
        pthread_mutex_unlock(&sess->sesslock);
}

#define UPDATE_FILE_HEAD_SIZE   11
int udt_recv(int udtsocket , int sock_t , char *buf , int len , struct sockaddr *from , int *fromlen);

int do_net_update(void *arg)
{
    struct sess_ctx *sess = (struct sess_ctx *)arg;
    int socket = -1;
    int size;
    FILE * kfp = NULL;
    FILE * sfp = NULL;
    char __scrc;
    char __kcrc;
    char scrc;
    char kcrc;
    char buf[1024];
    char *p;
    char cmd;
    int r;
    int reboot_flag = 0;
    unsigned int kernal_f_size;
    unsigned int system_f_size;
    unsigned int __kernal_f_size;
    unsigned int __system_f_size;
    struct stat    st;

	sess->session_type = SESSION_TYPE_UPDATE;
    add_sess(sess);
    take_sess_up(sess);
    if (is_do_update())
        goto exit;
    socket = sess->sc->video_socket;
    size = 0;
    while (size < 1024)
    {
        if (sess->is_tcp)
            r = recv(socket , buf + size, 1024 - size, 0);
        else
            r = udt_recv(socket , SOCK_STREAM , buf + size , 1024 - size, NULL, NULL);
        if (r <= 0)
            break;
        size += r;
    }
    r = size;
    if (r != 1024)
        goto exit;
    if (strncmp(buf , "version", 7) == 0)
    {
        p = buf + 10;
        if (stat(ENCRYPTION_FILE_PATH , &st) != 0)
            goto exit;
    }
    else
    {
        p = buf;
        if (stat(ENCRYPTION_FILE_PATH , &st) == 0)
            goto exit;
    }
    cmd = *p;
    p++;
    memcpy(&__system_f_size , p , sizeof(__system_f_size));
    p += sizeof(__system_f_size);
    memcpy(&__kernal_f_size , p, sizeof(__kernal_f_size));
    p += sizeof(__kernal_f_size);
    __scrc = *p;
    p++;
    __kcrc = *p;
    p++;
    kernal_f_size = 0;
    system_f_size = 0;
    scrc = 0;
    kcrc = 0;
    set_do_update();
    reboot_flag = 1;
    switch (cmd)
    {
    case 0:
        sfp = fopen(SYSTEM_UPDATE_FILE , "w");
        if (!sfp)
            goto exit;
        system_f_size  = (1024 - (p - buf));
        scrc = checksum(scrc, (unsigned char *)p, system_f_size);
        if (fwrite(p, 1, system_f_size, sfp) != system_f_size)
            goto exit;
        while (system_f_size < __system_f_size)
        {
            if (sess->is_tcp)
                r = recv(socket , buf, 1024, 0);
            else
                r = udt_recv(socket , SOCK_STREAM , buf , 1024 , NULL , NULL);
            if (r <= 0)
            {
                herror(" do update recvmsg error");
                goto exit;
            }
            system_f_size += r;
            scrc = checksum(scrc, (unsigned char *)buf, r);
            if ((unsigned int)r != fwrite(buf, 1, r, sfp))
                goto exit;
        }
        if (system_f_size != __system_f_size)
            goto exit;
        if (scrc != __scrc)
            goto exit;
        fclose(sfp);
        sfp = NULL;
        prepare_do_update();
        memset(buf , 0 , 1024);
        sprintf(buf , "flashcp  %s  /dev/mtd1", SYSTEM_UPDATE_FILE);
        system(buf);
        system("reboot &");
        exit(0);
        break;
    case 1:
        kfp = fopen(KERNAL_UPDATE_FILE , "w");
        if (!kfp)
            goto exit;
        kernal_f_size  = (1024 - (p - buf));
        kcrc = checksum(kcrc, (unsigned char *)p, kernal_f_size);
        if (fwrite(p, 1, kernal_f_size, kfp) != kernal_f_size)
            goto exit;
        while (kernal_f_size < __kernal_f_size)
        {
            if (sess->is_tcp)
                r = recv(socket , buf, 1024, 0);
            else
                r = udt_recv(socket , SOCK_STREAM , buf , 1024 , NULL , NULL);
            if (r <= 0)
            {
                herror("tcp do update recvmsg error");
                goto exit;
            }
            kernal_f_size += r;
            kcrc = checksum(kcrc, (unsigned char *)buf , r);
            if ((unsigned int)r != fwrite(buf, 1, r, kfp))
                goto exit;
        }
        if (kernal_f_size != __kernal_f_size)
            goto exit;
        if (kcrc != __kcrc)
            goto exit;
        fclose(kfp);
        kfp = NULL;
        prepare_do_update();
        memset(buf , 0 , 1024);
        sprintf(buf , "flashcp  %s  /dev/mtd0", KERNAL_UPDATE_FILE);
        system(buf);
        system("reboot &");
        exit(0);
        break;
    case 2:
        sfp = fopen(SYSTEM_UPDATE_FILE , "w");
        if (!sfp)
            goto exit;
        kfp = fopen(KERNAL_UPDATE_FILE , "w");
        if (!kfp)
            goto exit;
        system_f_size  = (1024 - (p - buf));
        scrc = checksum(scrc, (unsigned char *)p, system_f_size);
        if (fwrite(p, 1, system_f_size, sfp) != system_f_size)
            goto exit;
        p = NULL;
        while (system_f_size < __system_f_size)
        {
            if (sess->is_tcp)
                r = recv(socket , buf, 1024, 0);
            else
                r = udt_recv(socket , SOCK_STREAM , buf , 1024 , NULL , NULL);
            if (r <= 0)
            {
                herror("tcp do update recvmsg error");
                goto exit;
            }
            if (__system_f_size - system_f_size < (unsigned int)r)
            {
                scrc = checksum(scrc, (unsigned char *)buf, __system_f_size - system_f_size);
                if (fwrite(buf, 1, __system_f_size - system_f_size , sfp) != (__system_f_size - system_f_size))
                {
                    goto exit;
                }
                p = buf;
                p += (__system_f_size - system_f_size);
                system_f_size = __system_f_size;
                break;
            }
            system_f_size += r;
            scrc = checksum(scrc, (unsigned char *)buf, r);
            if ((unsigned int)r != fwrite(buf, 1, r, sfp))
                goto exit;
        }
        if (system_f_size != __system_f_size)
            goto exit;
        kernal_f_size = 0;
        if (p)
        {
            kernal_f_size = r - (p - buf);
            kcrc = checksum(kcrc, (unsigned char *)p, kernal_f_size);
            if (fwrite(p, 1, kernal_f_size, kfp) != kernal_f_size)
                goto exit;
        }
        while (kernal_f_size < __kernal_f_size)
        {
            if (sess->is_tcp)
                r = recv(socket , buf, 1024, 0);
            else
                r = udt_recv(socket , SOCK_STREAM , buf , 1024 , NULL , NULL);
            if (r <= 0)
            {
                herror("tcp do update recvmsg error:%s\n");
                goto exit;
            }
            kernal_f_size += r;
            kcrc = checksum(kcrc, (unsigned char *)buf , r);
            if ((unsigned int)r != fwrite(buf, 1, r, kfp))
                goto exit;
        }
        if (kernal_f_size != __kernal_f_size)
            goto exit;
        if (scrc != __scrc || kcrc != __kcrc)
            goto exit;
        fclose(sfp);
        fclose(kfp);

        prepare_do_update();
        memset(buf , 0 , 1024);
        sprintf(buf , "flashcp  %s  /dev/mtd0", KERNAL_UPDATE_FILE);
        system(buf);
        sleep(1);
        memset(buf , 0 , 1024);
        sprintf(buf , "flashcp  %s  /dev/mtd1", SYSTEM_UPDATE_FILE);
        system(buf);
        system("reboot &");
        exit(0);
        break;
    default:
        goto exit;
    }
exit:
    if (reboot_flag || sfp || kfp || socket >= 0)
    {
        clean_socket_container(0xffffffffffffffffULL, 1);
        system("reboot &");
        exit(0);
    }
    del_sess(sess);
    take_sess_down(sess);
    return 0;
}

int udt_send(int udtsocket , int sock_t, char *buf , int len);

//by chf: check whick kind of frame monitor needed
/*
return:
-1: don't need any frame now
0:  need I frame
1:  need other frame as normal
*/
int check_monitor_queue_status(void)
{
	struct sess_ctx* sess = global_ctx_running_list;
	int count = 0;

	if( is_do_update() ){
		return MONITOR_STATUS_NEED_NOTHING;
	}

	if( need_I_frame ){
		need_I_frame = 0;
		return MONITOR_STATUS_NEED_I_FRAME;
	}

	//by chf: how many monitor session
	while( sess != NULL ){
		if( sess->session_type == SESSION_TYPE_MONITOR ){
			count++;
		}
		sess = sess->next;
	}
	if( count == 0 ){
		return MONITOR_STATUS_NEED_NOTHING;
	}
	
    pthread_mutex_lock(&global_ctx_lock);
	if( count == 1 ){
		sess = global_ctx_running_list;
		while( sess!= NULL ){
			if( sess->session_type == SESSION_TYPE_MONITOR ){
				break;
			}
			sess = sess->next;
		}
		if( sess->send_list.total_packet_num < MAX_SEND_PACKET_NUM ){
			pthread_mutex_unlock(&global_ctx_lock);
			return MONITOR_STATUS_NEED_ANY;
		}
		else{
			printf("packet queue is full now, total %d packets\n", sess->send_list.total_packet_num );
			pthread_mutex_unlock(&global_ctx_lock);
			return MONITOR_STATUS_NEED_NOTHING;
		}
	}
	else{
		printf("**********************multi monitor session is not support now***********************************\n");
		pthread_mutex_unlock(&global_ctx_lock);
		sleep(3);
		return MONITOR_STATUS_NEED_NOTHING;
	}
	
	pthread_mutex_unlock(&global_ctx_lock);
	return MONITOR_STATUS_NEED_ANY;
}

int write_monitor_packet_queue(char* buf, int size)
{
	struct sess_ctx* sess = global_ctx_running_list;
	int ret = 0;

	if( size == 0 )
		return -1;
	if( is_do_update() ){
		return -1;
	}
	
	pthread_mutex_lock(&global_ctx_lock);//by chf: because we are writing into all monitor queue, so we use global lock instead of private lock
	while( sess != NULL ){
		if( sess->session_type == SESSION_TYPE_MONITOR ){
			if( sess->send_list.total_packet_num < MAX_SEND_PACKET_NUM && sess->send_list.current_state == PACKET_QUEUE_NORMAL ){
				SEND_PACKET* packet = malloc(sizeof(SEND_PACKET));
				if( packet == NULL ){
					printf("no buffer for send packet\n");
					ret  = -1;
					goto err_exit;
				}
				if( (packet->date_buf = malloc( size ) ) == NULL ){
					printf("no buffer for send packet data\n");
					ret  = -1;
					goto err_exit;
				}
				memcpy( packet->date_buf, buf, size );
				packet->size = size;
				list_add_tail(&packet->list,&sess->send_list.send_packet_list_head);
				sess->send_list.total_packet_num++;
			}
			else if(sess->send_list.total_packet_num >= MAX_SEND_PACKET_NUM){
				sess->send_list.current_state = PACKET_QUEUE_OVERFLOWED;
				printf("meet a session packet queue overflow\n");
			}
		}
		sess = sess->next;
	}

	pthread_mutex_unlock(&global_ctx_lock);
	return 0;
err_exit:
	pthread_mutex_unlock(&global_ctx_lock);
	return ret;
}

SEND_PACKET* get_monitor_queue_packet(struct sess_ctx* sess)
{
	SEND_PACKET* packet;
	
	pthread_mutex_lock(&global_ctx_lock);
	if( sess->send_list.total_packet_num == 0 ){
		pthread_mutex_unlock(&global_ctx_lock);
		return 0;
	}

	packet = list_first_entry(&sess->send_list.send_packet_list_head,SEND_PACKET,list);
	list_del(&packet->list);
	sess->send_list.total_packet_num--;
	pthread_mutex_unlock(&global_ctx_lock);
	return packet;
}

static int init_monitor_packet_queue(struct sess_ctx* sess)
{
	pthread_mutex_init(&sess->send_list.lock, NULL);
	INIT_LIST_HEAD(&sess->send_list.send_packet_list_head);
	sess->send_list.total_packet_num = sess->send_list.total_size = 0;
	sess->send_list.current_state = PACKET_QUEUE_NORMAL;
	return 0;
}

static void free_packet(SEND_PACKET* packet)
{
	if( packet == NULL ){
		return;
	}
	free( packet->date_buf );
	free( packet );
	packet = NULL;
	return;
}

static void free_monitor_packet_queue(struct sess_ctx* sess)
{
	SEND_PACKET* packet;
	struct list_head* p;
	
	pthread_mutex_lock(&global_ctx_lock);
	list_for_each(p,&sess->send_list.send_packet_list_head){
		packet = list_entry(p,SEND_PACKET,list);
		//by chf:
		list_del(&packet->list);
		sess->send_list.total_packet_num--;
		free_packet( packet );
	}
	if( sess->send_list.total_packet_num != 0 ){
		sess->send_list.total_packet_num = 0;
		printf("******************************************something strange happened during free packet queue******************************************\n");
	}
	pthread_mutex_unlock(&global_ctx_lock);
	
	return;
}

int start_video_monitor(struct sess_ctx* sess)
{
    log_debug("start \n");
    int ret;
    int socket = -1;
    char* buffer = NULL;
    int size;
    pthread_t tid;
    int attempts;
	SEND_PACKET* packet;

    log_debug("888888888888888888 \n");
    if (is_do_update())
    {
        log_warning("do update\n");
        goto exit;
    }
	sess->session_type = SESSION_TYPE_MONITOR;
	init_monitor_packet_queue(sess);
    add_sess(sess);
    take_sess_up(sess);
    sess->ucount = 1;
    socket = sess->sc->video_socket;

    log_debug("start sound duplex %d\n", threadcfg.sound_duplex);
    if (threadcfg.sound_duplex)
    {
        if (pthread_create(&tid, NULL, sound_start_session, sess) < 0)
        {
            goto exit;
        }
    }

	need_I_frame = 1;

	start_monitor_capture();
    attempts = 0;
    int const send_stride = 16 * 1024;

	
    for (;;)
    {
        packet = get_monitor_queue_packet(sess);
		if( packet == NULL ){
			//handle_video_thread();
			usleep(20*1000);
			continue;
		}
        buffer = packet->date_buf;
		size = packet->size;
        //log_debug("get video data %p size %d end\n", buffer, size);
        int i = 0;
        while (size > 0)
        {
            if (!sess->running)
            {
                free_packet( packet );
                goto exit;
            }
            if (size >= send_stride)
            {
                if (sess->is_tcp)
                    ret = send(socket, buffer + i, send_stride, 0);
                else
                    ret = udt_send(socket , SOCK_STREAM , buffer + i , send_stride);

            }
            else
            {
                if (sess->is_tcp)
                    ret = send(socket, buffer + i, size, 0);
                else
                    ret = udt_send(socket , SOCK_STREAM , buffer + i , size);
            }

            if (ret <= 0)
            {
                attempts ++;
                if (attempts <= 10)
                {
                    dbg("attempts send data now = %d\n", attempts);
                    continue;
                }
                printf("sent data error,the connection may currupt!\n");
                printf("ret==%d\n", ret);
                free_packet( packet );
                goto exit;
            }
            //printf("sess->is_tcp = %d , sent message =%d\n",sess->is_tcp , ret);
            attempts = 0;
            size -= ret;
            i += ret;
        }
        free_packet( packet );
    }

exit:
    /* Take down session */
	free_monitor_packet_queue(sess);
    del_sess(sess);
    take_sess_down(sess);
	stop_monitor_capture();

    return 0;
}


#define RECORD_STATE_STOP          0
#define RECORD_STATE_SLOW         1
#define RECORD_STATE_FAST        2
#define RECORD_STATE_NORMAL    3
static nand_record_file_header *record_header;

static nand_record_file_internal_header video_internal_header =
{
    {0, 0, 0, 1, 0xc},
    {0, 1, 0, 0},
};

static nand_record_file_internal_header audio_internal_header =
{
    {0, 0, 0, 1, 0xc},
    {0, 2, 0, 0},
};

#define COMPARE_STEP  3
#define VGA_LV1   200
#define VGA_LV2   600
#define VGA_LV3   1200
#define QVGA_LV1 100
#define QVGA_LV2 200
#define QVGA_LV3 400

static  int sensitivity_diff_size[4] = {10000, 300, 350, 450};
int ignore_pic_count = 6;
pthread_mutex_t ignore_pic_lock;

void set_ignore_count(int num)
{
    pthread_mutex_lock(&ignore_pic_lock);
    ignore_pic_count = num;
    pthread_mutex_unlock(&ignore_pic_lock);
}

void dec_ignore_count()
{
    pthread_mutex_lock(&ignore_pic_lock);
    ignore_pic_count --;
    pthread_mutex_unlock(&ignore_pic_lock);
}

static void init_sensitivity_diff_size(int width , int height)
{
    if (width == 320 && height == 240)
    {
        sensitivity_diff_size[1] = QVGA_LV1;
        sensitivity_diff_size[2] = QVGA_LV2;
        sensitivity_diff_size[3] = QVGA_LV3;
    }
    else if (width == 640 && height == 480)
    {
        sensitivity_diff_size[1] = VGA_LV1;
        sensitivity_diff_size[2] = VGA_LV2;
        sensitivity_diff_size[3] = VGA_LV3;
    }
}

char pause_record = 0;
char force_close_file = 0;

static inline char * gettimestamp()
{
    static char timestamp[15];
    time_t t;
    struct tm *curtm;
    if (time(&t) == -1)
    {
        printf("get time error\n");
        exit(0);
    }
    curtm = localtime(&t);
    sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", curtm->tm_year + 1900, curtm->tm_mon + 1,
            curtm->tm_mday, curtm->tm_hour, curtm->tm_min, curtm->tm_sec);
    return timestamp;
}

static inline void write_syn_sound(int *need_video_internal_head)
{
    static unsigned int i = 0;
    char *buf;
    int ret;
    int size;
    if (!threadcfg.sdcard_exist)
        return ;
    *need_video_internal_head = 0;
    buf = sound_amr_buffer_fetch(MAX_NUM_IDS - 1, & size);
    if (buf)
    {
        i++;
        if (!(i % 60))
            dbg("get sound data size == %d\n", size);
        ret = nand_write((char *)&audio_internal_header, sizeof(audio_internal_header));
        if (ret == 0)
        {
            ret = nand_write(buf, size);
            if (ret == 0)
                *need_video_internal_head = 1;
        }
        free(buf);
    }
}

int snd_soft_restart();
struct vdIn * init_camera(void);

#define NO_RECORD_FILE   "/data/norecord"
int start_video_record(struct sess_ctx* sess)
{
    int ret;
    char* buffer;
    int size;
    int i;
    int size0 = 0;
    int size1 = 0;
    int size2 = 0;
    char *timestamp;
    struct timeval starttime, endtime;
    struct timeval prev_write_sound_time;
    struct timeval alive_old_time;
    struct timeval mail_last_time;
    struct timeval index_table_15sec_last_time;
    vs_ctl_message msg , rmsg;
    unsigned long long timeuse;
    unsigned long long usec_between_image = 0;
    int frameratechange = 0;
    pthread_t mail_alarm_tid = 0;
    char swidth[12];
    char sheight[12];
    char FrameRateUs[12];
    //int type;
    //int num_pic_to_ignore = 150;
    int prev_width;
    int prev_height;
    int width ;
    int height;
    int email_alarm;
    int record_slow_speed = 0;
    int record_fast_speed = 0;
    int record_normal_speed;
    int record_normal_duration = 3;
    int record_last_state = 0;
    int timestamp_change = 0;
    int pictures_to_write = 0;
    int record_fast_duration = 0;
    int sensitivity_index = threadcfg.record_sensitivity;;
    int record_mode = 0;
    int need_write_internal_head = 0;
    int internal_head_for_sound = 0;
    char *attr_table = NULL;
    char *time_15sec_table = NULL;
    char * attr_pos = NULL ;
    char * record_15sec_pos = NULL;
    unsigned int *attr_table_size ;
    unsigned int *record_15sec_table_size;
    index_table_item_t      table_item;
    int pic_size[COMPARE_STEP];
    int size_count = 0;
    int pic_to_alarm = 0;
    int j;
    struct stat st;

    if (!vdin_camera)
    {
        if ((vdin_camera = (struct vdIn *)init_camera()) == NULL)
        {
            printf("init camera error\n");
            return -1;
        }
    }

    for (i = 1; i <= _NSIG; i++)
    {
        if (i == SIGIO || i == SIGINT)
            sigset(i, sig_handler);
        else
            sigignore(i);
    }
    pthread_mutex_init(&ignore_pic_lock , NULL);
    memset(nand_shm_file_end_head , 0xFF , 512);
    record_header = (struct nand_record_file_header *) nand_shm_file_end_head;
    attr_table = nand_shm_addr + RECORD_SHM_ATTR_TABLE_POS;
    time_15sec_table = nand_shm_addr + RECORD_SHM_15SEC_TABLE_POS;
    memset(attr_table , 0 , INDEX_TABLE_SIZE);
    memset(time_15sec_table , 0 , INDEX_TABLE_SIZE);
    attr_table_size = (unsigned int *)attr_table;
    record_15sec_table_size = attr_table_size + 1;
    attr_pos = (char *)(record_15sec_table_size + 1);
    record_15sec_pos = time_15sec_table;

    i = 0;
    width = vdin_camera->width;
    height = vdin_camera->height;
    prev_height = height = vdin_camera->height;
    prev_width = width = vdin_camera->width;
    init_sensitivity_diff_size(width,  height);

    if (threadcfg.email_alarm)
    {
        if (!threadcfg.mailbox[0] || !sender[0] || !senderpswd[0] || !mailserver[0])
        {
            threadcfg.email_alarm = 0;
        }
        else
        {
            init_mail_attatch_data_list(threadcfg.mailbox);
            if (pthread_create(&mail_alarm_tid, NULL, (void *) mail_alarm_thread, NULL) < 0)
            {
                mail_alarm_tid = 0;
            }
        }
    }

    usec_between_image = 0;
    if (strncmp(threadcfg.record_mode, "no_record", strlen("no_record")) == 0)
    {
        record_mode = 0;
        threadcfg.sdcard_exist = 0;
    }
    else if (strncmp(threadcfg.record_mode, "inteligent", strlen("inteligent")) == 0)
    {
        goto NORMAL_MODE;
        record_mode = 2;
        record_fast_duration = threadcfg.record_fast_duration;
        record_slow_speed = threadcfg.record_slow_speed;
        record_fast_speed = threadcfg.record_fast_speed;
        usec_between_image = (unsigned long long)1000000 / record_fast_speed;
    }
    else
    {
NORMAL_MODE:
        record_mode = 3;
        record_normal_duration = threadcfg.record_normal_duration;
        usec_between_image = (unsigned long long)1000000 / threadcfg.record_normal_speed;
    }
    if (stat(NO_RECORD_FILE , &st) == 0)
    {
        threadcfg.sdcard_exist = 0;
        record_mode = 0;
    }

        threadcfg.sdcard_exist = 0;
        record_mode = 0;

    record_normal_speed = threadcfg.record_normal_speed;
    email_alarm = threadcfg.email_alarm;
    sensitivity_index = threadcfg.record_sensitivity;
    record_last_state = RECORD_STATE_FAST;


    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_START_RECORDING;
    msg.msg[1] = 0;
    gettimeofday(&alive_old_time, NULL);
    ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
    if (ret == -1)
        exit(0);

    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_RECORD_ALIVE;
    msg.msg[1] = 0;


    sprintf(FrameRateUs, "%08llu", (unsigned long long)20000);
    memcpy(audio_internal_header.FrameRateUs, FrameRateUs, sizeof(audio_internal_header.FrameRateUs));
    memset(audio_internal_header.FrameWidth, 0, sizeof(audio_internal_header.FrameWidth));
    memset(audio_internal_header.FrameHeight, 0, sizeof(audio_internal_header.FrameHeight));

    sprintf(swidth, "%04d", width);
    sprintf(sheight, "%04d", height);
    sprintf(FrameRateUs, "%08llu", usec_between_image);
    memcpy(video_internal_header.FrameRateUs, FrameRateUs, sizeof(video_internal_header.FrameRateUs));
    memcpy(video_internal_header.FrameHeight, sheight, sizeof(video_internal_header.FrameHeight));
    memcpy(video_internal_header.FrameWidth, swidth, sizeof(video_internal_header.FrameWidth));


    timestamp = gettimestamp();
    memcpy(audio_internal_header.StartTimeStamp, timestamp, sizeof(audio_internal_header.StartTimeStamp));
    memcpy(video_internal_header.StartTimeStamp, timestamp, sizeof(video_internal_header.StartTimeStamp));

    sound_amr_buffer_reset();

    if (threadcfg.sdcard_exist)
    {
        table_item.location = 0;
        memcpy(record_15sec_pos, &table_item , sizeof(index_table_item_t));
        (*record_15sec_table_size) += sizeof(index_table_item_t);
        record_15sec_pos += sizeof(index_table_item_t);
    }

    gettimeofday(&starttime, NULL);
    mail_last_time.tv_sec = starttime.tv_sec;
    mail_last_time.tv_usec = starttime.tv_usec;
    audio_internal_header.flag[0] = SET_ALL_FLAG_CHANGE;
    memcpy(&prev_write_sound_time, &starttime, sizeof(struct timeval));
    video_internal_header.flag[0] = SET_ALL_FLAG_CHANGE;
    memcpy(&index_table_15sec_last_time , &starttime , sizeof(struct timeval));
    while (1)
    {
        if (is_do_update())
        {
            close_v4l2(vdin_camera);
            dbg("is do update return now\n");
            goto __out;
        }
        gettimeofday(&endtime, NULL);
        if (abs(endtime.tv_sec - alive_old_time.tv_sec) >= 3)
        {
            ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
            if (ret == -1)
            {
                dbg("send daemon message error\n");
                if (is_do_update())
                {
                    close_v4l2(vdin_camera);
                    dbg("is do update return now\n");
                    goto __out;
                }
                system("reboot &");
                exit(0);
            }
            memcpy(&alive_old_time, &endtime, sizeof(struct timeval));
        }
#if 0
        int trygrab = 10;
#endif

        if (uvcGrab(vdin_camera) < 0)
        {
#if 0
            trygrab--;
            log_debug("uvcGrad failed trygrab %d\n", trygrab);
            if (trygrab <= 0)
            {
                close_v4l2(vdin_camera);
                if (init_videoIn(vdin_camera, (char *) videodevice, vdin_camera->width, vdin_camera->height, V4L2_PIX_FMT_YUYV, 1) < 0)
                {
                    log_warning("init camera device error\n");
                    exit(0);
                }
            }
#endif
            usleep(1000 * 20);
        }
        continue;

        buffer = get_data(&size, &width, &height);
        //mail alarm
        if (email_alarm && mail_alarm_tid)
        {
            pic_size[size_count] = size;
            size_count = (size_count + 1) % COMPARE_STEP;
            if (size_count == 0)
            {
                size2 = 0;
                for (j = 0 ; j < COMPARE_STEP ; j++)
                    size2 += pic_size[j];
                size2 /= COMPARE_STEP;
            }
            if (pic_to_alarm <= 0)
            {
                if (size_count == 0)
                {
                    if (size0 && size1 && (abs(size2 - size1) >= sensitivity_diff_size[sensitivity_index] || abs(size2 - size0) >= sensitivity_diff_size[sensitivity_index]))
                    {
                        printf("#####diff0 = %d ,diff1 = %d , size0 = %d , size1= %d , size2 = %d#######\n", size2 - size1 , size2 - size0 , size0 , size1 , size2);
                        pic_to_alarm = 6;
                        mail_last_time.tv_sec = 0;
                    }
                    size0 = size1;
                    size1 = size2;
                }

            }
            else
            {
                gettimeofday(&endtime , NULL);
                timeuse = (unsigned long long)1000000 * abs(endtime.tv_sec - mail_last_time.tv_sec) + endtime.tv_usec - mail_last_time.tv_usec;
                if (timeuse > 333333ULL)
                {
                    if (ignore_pic_count <= 0)
                    {
                        char *image = malloc(size);
                        if (image)
                        {
                            memcpy(image, buffer, size);
                            add_image_to_mail_attatch_list_no_block(image,  size);
                        }
                    }
                    else
                        dec_ignore_count();
                    memcpy(&mail_last_time , &endtime , sizeof(endtime));
                    pic_to_alarm -- ;
                }
                if (size_count == 0)
                {
                    size0 = size1;
                    size1 = size2;
                }
            }
        }

        if (threadcfg.sdcard_exist)
        {
            gettimeofday(&endtime, NULL);
            if (abs(endtime.tv_sec - index_table_15sec_last_time.tv_sec) >= 15)
            {
                table_item.location = nand_get_position();
                if (*record_15sec_table_size + *attr_table_size + sizeof(index_table_item_t) + 8 <= INDEX_TABLE_SIZE)
                {
                    memcpy(record_15sec_pos, &table_item , sizeof(index_table_item_t));
                    (*record_15sec_table_size) += sizeof(index_table_item_t);
                    memcpy(&index_table_15sec_last_time , &endtime, sizeof(struct timeval));
                    record_15sec_pos += sizeof(index_table_item_t);
                }
                else
                    goto FORCE_CLOSE_FILE;
            }
        }

        if (pictures_to_write > 0 && abs(size - size0) > sensitivity_diff_size[sensitivity_index])
        {
            if (record_mode == 2)
                pictures_to_write = record_fast_speed * record_fast_duration;
            else if (record_mode == 1)
                pictures_to_write = record_normal_speed * record_normal_duration;
        }

        switch (record_mode)
        {
        case 0:/*not record*/
            pictures_to_write = 0;
            break;
        case 1:
        {
            if (pictures_to_write <= 0)
            {
                if (abs(size - size0) > sensitivity_diff_size[sensitivity_index])
                {
                    usec_between_image = (unsigned long long)1000000 / record_normal_speed;
                    pictures_to_write = record_normal_speed * record_normal_duration;
                    gettimeofday(&starttime, NULL);
                    if (record_last_state == RECORD_STATE_STOP)
                    {
                        sound_amr_buffer_clean(MAX_NUM_IDS - 1);
                        timestamp = gettimestamp();
                        memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
                        memcpy(&prev_write_sound_time, &starttime, sizeof(struct timeval));
                        record_last_state = RECORD_STATE_FAST;
                        timestamp_change = 1;
                    }
                }
                else
                {
                    pictures_to_write = 0;
                    if (record_last_state == RECORD_STATE_FAST)
                    {
                        write_syn_sound(&internal_head_for_sound);
                        record_last_state = RECORD_STATE_STOP;
                    }
                }
            }
            else
            {
                if (record_normal_speed < 25)
                {
                    gettimeofday(&endtime, NULL);
                    timeuse = (unsigned long long)1000000 * abs(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;
                    if (abs(timeuse) >= usec_between_image)
                    {
                        memcpy(&starttime, &endtime, sizeof(struct timeval));
                    }
                    else
                    {
                        size0 = size;
                        free(buffer);
                        continue;
                    }
                }
            }
            break;
        }
        case 2:
        {
            if (pictures_to_write <= 0)
            {
                if (abs(size - size0) > sensitivity_diff_size[sensitivity_index])
                {
                    pictures_to_write = record_fast_speed * record_fast_duration;
                    if (record_last_state == RECORD_STATE_SLOW)
                    {
                        record_last_state = RECORD_STATE_FAST;
                        frameratechange = 1;
                        usec_between_image = (unsigned long long)1000000 / record_fast_speed;
                        memset(&starttime, 0, sizeof(struct timeval));
                    }
                }
                else
                {
                    if (record_last_state == RECORD_STATE_FAST)
                    {
                        record_last_state = RECORD_STATE_SLOW;
                        frameratechange = 1;
                        usec_between_image = (unsigned long long) 1000000 / record_slow_speed;
                    }
                    pictures_to_write = 1;
                }
                gettimeofday(&endtime, NULL);
                timeuse = (unsigned long long)1000000 * abs(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;
                if (abs(timeuse) >= usec_between_image)
                {
                    memcpy(&starttime, &endtime, sizeof(struct timeval));
                }
                else
                {
                    pictures_to_write = 0;
                }
            }
            else
            {
                if (record_fast_speed < 25)
                {
                    gettimeofday(&endtime, NULL);
                    timeuse = (unsigned long long)1000000 * abs(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;
                    if (abs(timeuse) >= usec_between_image)
                    {
                        memcpy(&starttime, &endtime, sizeof(struct timeval));
                    }
                    else
                    {
                        size0 = size;
                        free(buffer);
                        continue;
                    }
                }
            }
            break;
        }
        case 3:
        {
            usec_between_image = (unsigned long long) 1000000 / record_normal_speed;
            record_last_state = RECORD_STATE_NORMAL;
            gettimeofday(&endtime , NULL);
            timeuse = (unsigned long long)1000000 * abs(endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec - starttime.tv_usec;
            if (abs(timeuse) >= usec_between_image)
            {
                pictures_to_write = 1;
                memcpy(&starttime , &endtime, sizeof(struct timeval));
            }
            else
                pictures_to_write = 0;
            break;
        }
        default:
            exit(0);
        }

        if (pictures_to_write <= 0)
        {
            free(buffer);
            usleep(1000);
            continue;
        }

        pictures_to_write --;

        if (timestamp_change || frameratechange || prev_width != width || prev_height != height)
        {
            if (prev_width != width || prev_height != height)
            {
                init_sensitivity_diff_size(width,  height);
                set_ignore_count(6);
            }
            internal_head_for_sound = 0;
            need_write_internal_head = 1;
            timestamp_change = 0;
            frameratechange = 0;
            prev_width = width;
            prev_height = height;
        }
        if (internal_head_for_sound)
            need_write_internal_head = 1;
        if (need_write_internal_head)
        {
            if (threadcfg.sdcard_exist)
            {
                timestamp = gettimestamp();
                sprintf(swidth, "%04d", width);
                sprintf(sheight, "%04d", height);
                sprintf(FrameRateUs, "%08llu", usec_between_image);
                memcpy(video_internal_header.StartTimeStamp, timestamp, sizeof(video_internal_header.StartTimeStamp));
                memcpy(video_internal_header.FrameRateUs, FrameRateUs, sizeof(video_internal_header.FrameRateUs));
                memcpy(video_internal_header.FrameHeight, sheight, sizeof(video_internal_header.FrameHeight));
                memcpy(video_internal_header.FrameWidth, swidth, sizeof(video_internal_header.FrameWidth));

                table_item.location = nand_get_position();
                if (*record_15sec_table_size + *attr_table_size + sizeof(index_table_item_t) + 8 <= INDEX_TABLE_SIZE)
                {
                    if (nand_write((char *)&video_internal_header, sizeof(video_internal_header)) == 0 && !internal_head_for_sound)
                    {
                        memcpy(attr_pos, &table_item , sizeof(index_table_item_t));
                        (*attr_table_size) += sizeof(index_table_item_t);
                        attr_pos += sizeof(index_table_item_t);
                    }
                }
                else
                {
                    need_write_internal_head = 0;
                    internal_head_for_sound = 0;
                    goto FORCE_CLOSE_FILE;
                }
            }
            need_write_internal_head = 0;
            internal_head_for_sound = 0;
        }
retry:
        if (threadcfg.sdcard_exist)
        {
            ret = nand_write(buffer, size);
            if (force_close_file || msgrcv(msqid, &rmsg, sizeof(rmsg) - sizeof(long), VS_MESSAGE_RECORD_ID , IPC_NOWAIT) > 0)
            {
                memcpy(attr_pos , time_15sec_table , *record_15sec_table_size);
                nand_write_index_table(attr_table);
                nand_prepare_close_record_header(record_header);
                nand_write_end_header(record_header);
                snd_soft_restart();
            }
            if (ret == 0)
            {
                i++;
                usleep(1);
                if (i % 1000 == 0)
                    dbg("write pictures: %d\n", i);
            }
            else if (ret == VS_MESSAGE_NEED_START_HEADER)
            {
                nand_prepare_record_header(record_header);
                sprintf(swidth, "%04d", width);
                sprintf(sheight, "%04d", height);
                sprintf(FrameRateUs, "%08llu", usec_between_image);
                memcpy(record_header->FrameWidth, swidth, 4);
                memcpy(record_header->FrameHeight, sheight, 4);
                memcpy(record_header->FrameRateUs, FrameRateUs, 8);
                nand_write_start_header(record_header);
                goto retry;
            }

            else if (ret == VS_MESSAGE_NEED_END_HEADER)
            {
FORCE_CLOSE_FILE:
                memcpy(attr_pos , time_15sec_table , *record_15sec_table_size);
                nand_write_index_table(attr_table);
                nand_prepare_close_record_header(record_header);
                nand_write_end_header(record_header);

                memset(attr_table , 0 , INDEX_TABLE_SIZE);
                memset(time_15sec_table , 0 , INDEX_TABLE_SIZE);
                attr_table_size = (unsigned int *)attr_table;
                record_15sec_table_size = attr_table_size + 1;
                attr_pos = (char *)(record_15sec_table_size + 1);
                record_15sec_pos = time_15sec_table;
                gettimeofday(&index_table_15sec_last_time , NULL);

                table_item.location = 0;
                memcpy(record_15sec_pos, &table_item , sizeof(index_table_item_t));
                (*record_15sec_table_size) += sizeof(index_table_item_t);
                dbg("write 15sec location  pos = %u , write in %p , 15sec table size = %u\n", table_item.location , record_15sec_pos, *record_15sec_table_size);
                record_15sec_pos += sizeof(index_table_item_t);
                sound_amr_buffer_clean(MAX_NUM_IDS - 1);
                timestamp = gettimestamp();
                memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
                gettimeofday(&prev_write_sound_time , NULL);
                goto retry;
            }
            else
                goto FORCE_CLOSE_FILE;
        }
        free(buffer);
        gettimeofday(&endtime , NULL);
        timeuse = (endtime.tv_sec - prev_write_sound_time.tv_sec) * 1000000UL +
                  endtime.tv_usec - prev_write_sound_time.tv_usec;
        if (timeuse >= 1000000ULL)
        {
            write_syn_sound(&internal_head_for_sound);
            memcpy(&prev_write_sound_time , &endtime  , sizeof(endtime));
            timestamp = gettimestamp();
            memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
        }
    }

__out:
    while (is_do_update())
    {
        ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
        sleep(3);
    }
    return 0;
}

