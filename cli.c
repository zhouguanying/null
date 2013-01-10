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
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <linux/fs.h>
#include <sys/statfs.h>
#include "includes.h"
#include "uart.h"
#include "cfg_network.h"
#include "cli.h"
#include "config.h"
#include "revision.h"
#include "nand_file.h"
#include "vpu_server.h"
#include "server.h"
#include "playback.h"
#include "record_file.h"
#include "utilities.h"
#include "sound.h"
#include "video_cfg.h"
#include "v4l2uvc.h"
#include "amixer.h"
#include "udttools.h"
#include "socket_container.h"
#include "stun.h"

#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)

struct cli_sess_ctx *g_cli_ctx = NULL; /* Single session only */

static struct cli_handler cli_cmd_handler;

static int cli_socket = -1;
SOCKET_TYPE cli_st;

static struct cli_sess_ctx *new_session(void *arg)
{
    struct cli_sess_ctx *sess = NULL;

    /* Create a new session */
    if ((sess = malloc(sizeof(*sess))) == NULL) return NULL;
    memset(sess, 0, sizeof(*sess));

    sess->port = CLI_PORT;
    sess->arg = arg; /* cache the arg used by the callbacks */
    g_cli_ctx = sess;

    return sess;
}

static int free_session(struct cli_sess_ctx *sess)
{
    if (sess == NULL) return -1;

    if (sess->running)          sess->running = 0;
    if (sess->sock >= 0)        close(sess->sock);
    if (sess->saddr != NULL)    free(sess->saddr);
    //if (sess->sock_ctx != NULL) free(sess->sock_ctx);

    free(sess);
    g_cli_ctx = NULL;

    return 0;
}

extern char *do_cli_cmd_bin(void *sess, char *cmd, int cmd_len, int size, int *rsp_len);

char *set_transport_type_rsp(struct sess_ctx *sess , int *size)
{
    char *buf;
    if (!sess)
        return NULL;

    buf = malloc(6);
    if (sess->is_tcp)
        sprintf(buf , "tcp");
    else if (sess->is_rtp)
        sprintf(buf , "rtp");
    else if (sound_talking)
        sprintf(buf, "tlk");
    else
    {
        free(buf);
        return NULL;
    }
    buf[3] = (char)threadcfg.brightness;
    buf[4] = (char)threadcfg.contrast;
    buf[5] = (char)threadcfg.volume;
    *size = 6;
    return buf;
}

static char * handle_cli_request(struct cli_sess_ctx *sess, u8 *req,
                                 ssize_t req_len, u8 *unused,
                                 int *rsp_len, struct sockaddr_in from)
{
    struct cli_handler *p;
    struct sess_ctx * tmp;
    char cmd[1024];
    char *argv[2];
    int i;
    char* ptr;
    char * rsp;

    if (strncmp((char*)req, "UpdateSystem", 12) == 0)
    {
        return do_cli_cmd_bin(NULL, (char*)req, req_len, (int)NULL, rsp_len);
    }

    if (sess->cmd == NULL)
    {
        dbg("Error parsing command table - handler not installed");
        return 0;
    }

    p = sess->cmd;

    i = 0;
    memset(cmd, 0, sizeof(cmd));

    if (req[req_len - 1] != '\0');
        req_len -= 1;

    memcpy(cmd, req, req_len);
    argv[0] = argv[1] = 0;
    ptr = cmd;
    i = req_len;
    while (i--)
    {
        if (*ptr != ' ')
        {
            argv[0] = ptr;
            break;
        }
        ptr++;
    }
    while (i--)
    {
        if (*ptr == ':')
        {
            ptr++;
            argv[1] = ptr;
            break;
        }
        ptr++;
    }

    printf("argv[0]==%s\n", argv[0]);
    printf("argv[1]==%s\n", argv[1]);
    if (p->handler != NULL)
    {
        pthread_mutex_lock(&global_ctx_lock);
        sess->from = from;
        tmp = (struct sess_ctx*)sess->arg;
        if (tmp != NULL && tmp->from.sin_addr.s_addr == sess->from.sin_addr.s_addr && tmp->from.sin_port == sess->from.sin_port)
        {
            if (strncmp(argv[0], "set_transport_type", 18) == 0)
            {
                if (tmp->running)
                {
                    pthread_mutex_unlock(&global_ctx_lock);
                    dbg("session is already running\n");
                    return set_transport_type_rsp(tmp, rsp_len);
                }
                else
                {
                    memset(&tmp->from, 0, sizeof(struct sockaddr_in));
                    goto NEW_SESSION;
                }
            }
            goto done;
        }
        tmp = global_ctx_running_list;
        while (tmp != NULL)
        {
            if (tmp->from.sin_addr.s_addr == sess->from.sin_addr.s_addr && tmp->from.sin_port == sess->from.sin_port)
            {
                if (strncmp(argv[0], "set_transport_type", 18) == 0)
                {
                    if (tmp->running)
                    {
                        pthread_mutex_unlock(&global_ctx_lock);
                        dbg("set_transport_type session already running\n");
                        return set_transport_type_rsp(tmp, rsp_len);
                    }
                    else
                    {
                        memset(&tmp->from, 0, sizeof(struct sockaddr_in));
                        goto NEW_SESSION;
                    }
                }
                sess->arg = tmp;
                goto done;
            }
            tmp = tmp->next;
        }
        if (strncmp(argv[0], "set_transport_type", 18) == 0)
        {
            if (session_number >= MAX_CONNECTIONS)
            {
                pthread_mutex_unlock(&global_ctx_lock);
                return strdup("connected max ");
            }
NEW_SESSION:
            tmp = new_system_session("ipcam");
            if (!tmp)
            {
                pthread_mutex_unlock(&global_ctx_lock);
                return NULL;
            }
            memcpy(&tmp->from, &sess->from, sizeof(struct sockaddr_in));
            sess->arg = tmp;
            goto done;
        }

        /*check if it is blong to the first class command*/
        if (strncmp(argv[0], "get_firmware_info", 17) == 0)
            goto done;
        else if (strncmp(argv[0], "GetRecordStatue", 15) == 0)
            goto done;
        else if (strncmp(argv[0], "GetNandRecordFile", 17) == 0)
            goto done;
        else if (strncmp(argv[0], "ReplayRecord", 12) == 0)
            goto done;
        else if (strncmp(argv[0], "GetConfig", 9) == 0)
            goto done;
        else if (strncmp(argv[0], "SetConfig", 9) == 0)
            goto done;
        else if (strncmp(argv[0], "search_wifi", 11) == 0)
            goto done;
        else if (strncmp(argv[0], "set_pswd", 8) == 0)
            goto done;
        else if (strncmp(argv[0], "check_pswd", 10) == 0)
            goto done;
        else if (strncmp(argv[0], "pswd_state", 10) == 0)
            goto done;
        else if (strncmp(argv[0], "SetTime", 7) == 0)
            goto done;
        else if (strncmp(argv[0], "GetTime", 7) == 0)
            goto done;
        else if (strncmp(argv[0], "DeleteAllFiles", 14) == 0)
            goto done;
        else if (strncmp(argv[0], "DeleteFile", 10) == 0)
            goto done;
        else if (strncmp(argv[0] , "getversion", 10) == 0)
            goto done;
        pthread_mutex_unlock(&global_ctx_lock);
        return NULL;
done:
        dbg("ready to do cli, cmd=%s\n", argv[0]);
        rsp = p->handler(sess->arg, argv[0], argv[1], i + 1, rsp_len);
        pthread_mutex_unlock(&global_ctx_lock);
        return rsp;
    }

    return NULL;
}

extern int msqid;

static inline void do_cli_start()
{
    int ret;
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_DO_CLI_START;
    msg.msg[1] = 0;
    ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
    if (ret == -1)
    {
        system("reboot &");
        exit(0);
    }
}

static inline void do_cli_alive()
{
    int ret;
    vs_ctl_message msg;
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_DO_CLI_ALIVE;
    msg.msg[1] = 0;
    ret = msgsnd(msqid , &msg, sizeof(vs_ctl_message) - sizeof(long), 0);
    if (ret == -1)
    {
        system("reboot &");
        exit(0);
    }
}

int tcp_get_readable_socket(int *sockfds, int sockfdnums, struct timeval *tv)
{
    fd_set rfds;
    int i;
    int ret;
    int maxfdp1 = -1;

    FD_ZERO(&rfds);

    if (sockfdnums <= 0)
        return -1;

    for (i = 0; i < sockfdnums ; i++)
    {
        FD_SET(sockfds[i] , &rfds);
        if (sockfds[i ] > maxfdp1)
            maxfdp1 = sockfds[i];
    }

    ret = select(maxfdp1 + 1 , &rfds , NULL , NULL, tv);
    if (ret == 0)
        return UDT_SELECT_TIMEOUT;
    if (ret == -1)
        return UDT_SELECT_ERROR;
    for (i = 0 ; i < sockfdnums ; i++)
    {
        if (FD_ISSET(sockfds[i] , &rfds))
            return sockfds[i];
    }
    return UDT_SELECT_UNKONW_ERROR;
}

int get_readable_socket(cmd_socket_t *sockfds, int sockfdnums, struct timeval *tv, SOCKET_TYPE *type)
{
    int udtsockfds[64];
    int tcpsockfds[64];
    int i , udtsocketnum , tcpsocketnum;
    int socket;

    udtsocketnum = tcpsocketnum  = 0;

    for (i = 0 ; i < sockfdnums ; i++)
    {
        switch (sockfds[i].type)
        {
        case TCP_SOCKET:
            tcpsockfds[tcpsocketnum] = sockfds[i].socket;
            tcpsocketnum ++;
            break;
        case UDT_SOCKET:
            udtsockfds[udtsocketnum] = sockfds[i].socket;
            udtsocketnum ++;
            break;
        default:
            system("reboot&");
            exit(0);
            break;
        }
    }
    if (udtsocketnum > 0)
    {
        socket = udt_get_readable_socket(udtsockfds, udtsocketnum,  tv);
        if (socket >= 0)
        {
            *type = UDT_SOCKET;
            return socket;
        }
    }
    if (tcpsocketnum > 0)
    {
        socket = tcp_get_readable_socket(tcpsockfds, tcpsocketnum,  tv);
        if (socket >= 0)
        {
            *type = TCP_SOCKET;
            return socket;
        }
    }
    return -1;
}

int cmd_send_msg(int sock , SOCKET_TYPE st , char *buf , int len)
{
    switch (st)
    {
        case TCP_SOCKET:
            return stun_tcp_sendmsg(sock,  buf,  len);
            break;
        case UDT_SOCKET:
            return stun_sendmsg(sock, buf, len);
            break;
        default:
            break;
    }
    return -1;
}

int cmd_recv_msg(int sock , SOCKET_TYPE st , char *buf , int len , struct sockaddr *addr , int *addrlen)
{
    switch (st)
    {
        case TCP_SOCKET:
            return stun_tcp_recvmsg(sock,  buf,  len , addr, addrlen);
            break;
        case UDT_SOCKET:
            return stun_recvmsg(sock, buf, len , addr , addrlen);
            break;
        default:
            break;
    }
    return -1;
}

static int do_cli(struct cli_sess_ctx *sess)
{
#define  CLI_BUF_SIZE  1024
    cmd_socket_t uset[64];
    int fdnums;
    int sockfd;
    SOCKET_TYPE st;
    u8 req[CLI_BUF_SIZE];
    char *rsp;
    int rsp_len;
    struct timeval tv , now , last_alive_time;
    struct sockaddr_in from;
    int fromlen;
    int req_len;
    int s, ret;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    init_socket_container_list();
    do_cli_start();
    gettimeofday(&last_alive_time , NULL);

    for (;;)
    {
        printf("\n\n\n");
        dbg("try to get cli cmd\n");
LOOP_START:
        for (;;)
        {
            get_cmd_socket(uset, &fdnums);
            if (fdnums > 0)
                break;
            sleep(3);
            do_cli_alive();
            gettimeofday(&last_alive_time , NULL);
        }
        sockfd = get_readable_socket(uset, fdnums,  &tv , &st);
        if (sockfd < 0)
        {
            do_cli_alive();
            gettimeofday(&last_alive_time , NULL);
            check_cmd_socket();
            sleep(1);
            goto LOOP_START;
        }
        dbg("get ready sockfd = %d\n", sockfd);
        req_len = cmd_recv_msg(sockfd , st, (char *)req, CLI_BUF_SIZE - 1 , (struct sockaddr *) &from , &fromlen);
        if (req_len < 0)
        {
            dbg("req_len < 0\n");
            do_cli_alive();
            gettimeofday(&last_alive_time , NULL);
            //check_cmd_socket();
            close_socket_container(get_socket_container(sockfd , 0));
            sleep(1);
            goto LOOP_START;
        }
        gettimeofday(&now , NULL);
        if (now.tv_sec - last_alive_time.tv_sec >= 3)
        {
            do_cli_alive();
            last_alive_time = now;
        }
        req[req_len] = 0;
        dbg("get cli cmd: %s ip ==%s  ,  port ==%d\n", req , inet_ntoa(from.sin_addr) , ntohs(from.sin_port));
        if (is_do_update())
        {
            dbg("is do update exit now\n");
            for (;;)
            {
                do_cli_alive();
                sleep(3);
            }
        }
        rsp_len = 0;
        cli_socket = sockfd;
        cli_st = st;
        rsp = handle_cli_request(sess, req, req_len, NULL, &rsp_len, from);
        dbg("rsp=%s\n", rsp);
        if (rsp != NULL)   /* command ok so send response */
        {
            fromlen = sizeof(struct sockaddr_in);
            if (rsp_len)
            {
                //1  it seems never occur
                //printf("enter seen never happen rsp_len==%d\n",rsp_len);
                s = 0;
                while (rsp_len > 0)
                {
                    if (rsp_len > 1000)
                        ret = cmd_send_msg(sockfd , st,  rsp + s , 1000);
                    else
                        ret = cmd_send_msg(sockfd , st,  rsp + s , rsp_len);
                    if (ret < 0)
                        break;
                    s += ret;
                    rsp_len -= ret;
                }
            }
            else
            {
                rsp_len = strlen(rsp);
                s = 0;
                while (rsp_len > 0)
                {
                    if (rsp_len > 1000)
                        ret = cmd_send_msg(sockfd , st,  rsp + s , 1000);
                    else
                        ret = cmd_send_msg(sockfd, st, rsp + s , rsp_len);
                    if (ret < 0)
                        break;
                    s += ret;
                    rsp_len -= ret;
                }
            }
            free(rsp);
        }
        check_cmd_socket();
    }
    return 0;
}

/**
 * cli_init - init cli session
 * @arg: optional arg passed as argumment to handlers
 * Returns 0 on success or -1 on error
 */
static  int check_cli_pswd(char *arg , char **r)
{
    if (!threadcfg.pswd[0])
    {
        *r = strdup(PASSWORD_NOT_SET);
        return -1;
    }

    while (*arg && *arg != ':')
        arg++;

    if (*arg != ':')
    {
        *r = strdup(PASSWORD_FAIL);
        return -1;
    }
    *arg = 0;
    arg++;

    if (strlen(arg) != strlen(threadcfg.pswd) ||
        strncmp(arg, threadcfg.pswd, strlen(arg)) != 0)
    {
        *r = strdup(PASSWORD_FAIL);
        return -1;
    }
    return 0;
}

struct cli_sess_ctx * cli_init(void *arg)
{
    struct cli_sess_ctx *sess;

    if (g_cli_ctx != NULL ||
        (sess = new_session(arg)) == NULL)
    {
        return NULL;
    }

    if (pthread_create(&sess->tid, NULL, (void *) do_cli, sess) < 0)
    {
        free_session(sess);
        return NULL;
    }
    else
        return sess;
}

int cli_deinit(struct cli_sess_ctx *sess)
{
    return free_session(sess);
}

int cli_bind_cmds(struct cli_sess_ctx *sess, struct cli_handler *cmds)
{
    if (sess == NULL || cmds == NULL)
        return -1;
    else
    {
        sess->cmd = cmds;
        return 0;
    }
}

int set_dest_addr(struct sess_ctx *sess, char *arg)
{
    if (sess == NULL || sess->to == NULL || arg == NULL)
        return -1;
    else
    {
        if (sess->is_udp || sess->is_rtp)
        {
            inet_aton(arg, &sess->to->sin_addr);
        }
        return 0;
    }
}

int set_dest_port(struct sess_ctx *sess, char *arg)
{
    u16 port;

    if (sess == NULL || sess->to == NULL || arg == NULL)
        return -1;
    else
    {
        if (sess->is_udp || sess->is_rtp)
        {
            port = (u16) strtoul(arg, NULL, 10);
            sess->to->sin_port = htons(port);
        }
        return 0;
    }
}

int start_recording_video(struct sess_ctx *sess)
{
    while (1)
        ;
}

int stop_recording_video(struct sess_ctx *sess)
{
    sess->paused = 1;
    return 0;
}


int start_vid(struct sess_ctx *sess, char *arg)
{
    if (sess != NULL && !sess->playing)
    {
        if (sess->is_tcp && !sess->connected)
            return 0;
        if (start_recording_video(sess) < 0)
            return -1;
        else
        {
            sess->playing = 1;
            return 0;
        }
    }
    else if (sess->paused)
    {
        sess->paused = 0;
        return 0;
    }
    else
        return -1;
}

int pause_vid(struct sess_ctx *sess, char *arg)
{
    if (sess != NULL && sess->playing)
        return stop_recording_video(sess);
    else
        return -1;
}

int stop_vid(struct sess_ctx *sess, char *arg)
{
    if (sess != NULL)
        return raise(SIGPIPE);
    else
        return -1;
}

static char *set_transport_type(struct sess_ctx *sess , char *arg , int *rsp_len)
{
    struct socket_container *sc;
    playback_t *pb;
    char *r;
    sc = get_socket_container(cli_socket , 1);
    if (!sc)
    {
        g_cli_ctx->arg = NULL;
        free_system_session(sess);
        return NULL;
    }
    sc->connected = 1;
    if (sess == NULL)
    {
        close_socket_container(sc);
        return NULL;
    }
    if (check_cli_pswd(arg, & r) != 0)
    {
        g_cli_ctx->arg = NULL;
        free_system_session(sess);
        close_socket_container(sc);
        return r;
    }
    if (sc->video_socket < 0 || sc->audio_socket < 0)
    {
        g_cli_ctx->arg = NULL;
        free_system_session(sess);
        close_socket_container(sc);
        return NULL;
    }
    if (sc->audio_st != sc->video_st)
    {
        g_cli_ctx->arg = NULL;
        free_system_session(sess);
        close_socket_container(sc);
        return NULL;
    }
    r = malloc(16);
	memset(r,0,16);
    if (!r)
    {
        g_cli_ctx->arg = NULL;
        free_system_session(sess);
        close_socket_container(sc);
        return NULL;
    }
    switch (sc->video_st)
    {
    case TCP_SOCKET:
        sprintf(r, "tcp");
        sess->is_tcp = 1;
        break;
    case UDT_SOCKET:
        sprintf(r, "rtp");
        sess->is_rtp = 1;
        break;
    default:
        exit(0);
    }

    // special: connection not allowed when talking
    if (sound_talking)
        sprintf(r, "tlk");

    sess->sc = sc;
    if (strncmp(arg , "update", 6) == 0)
    {
        if (pthread_create(&sess->tid, NULL, (void *) do_net_update, sess) < 0)
        {
            g_cli_ctx->arg = NULL;
            free_system_session(sess);
            close_socket_container(sc);
            free(r);
            return NULL;
        }
    }
    else
    {
        playback_remove_dead();
        pthread_mutex_lock(&list_lock);
        pb = playback_find(sess->from);
        pthread_mutex_unlock(&list_lock);
        if (pb && (playback_get_status(pb) == PLAYBACK_STATUS_OFFLINE))
        {
            pb->sess = sess;
            if (pthread_create(&pb->thread_id, NULL, (void *) playback_thread, pb) < 0)
            {
                g_cli_ctx->arg = NULL;
                playback_set_dead(pb);
                free_system_session(sess);
                close_socket_container(sc);
                free(r);
                return NULL;
            }
        }
        else
        {
            if (pthread_create(&sess->tid, NULL, (void *) start_video_monitor, sess) < 0)
            {
                g_cli_ctx->arg = NULL;
                free_system_session(sess);
                close_socket_container(sc);
                free(r);
                return NULL;
            }
        }
    }

#ifdef IPED_98
    r[3] = (char)threadcfg.brightness;
    r[4] = (char)threadcfg.contrast;
    r[5] = (char)threadcfg.volume;
	r[6] = ':'; 
	r[7] = 'm';
	r[8] = 'p';
	r[9] = 'e';
	r[10] = 'g';
	r[11] = '4';
	r[12] = ':'; 
    r[13] = (char)threadcfg.record_quality;
    *rsp_len = 14;
#else
	r[3] = (char)threadcfg.brightness;
	r[4] = (char)threadcfg.contrast;
	r[5] = (char)threadcfg.volume;
	*rsp_len = 6;
#endif
    return r;

}

static char *get_transport_type(struct sess_ctx *sess, char *arg)
{
    char *resp;

    if (sess == NULL)
        return NULL;

    if (sess->is_tcp)
    {
        if ((resp = strdup("tcp")) == NULL)
            return NULL;
    }
    else if (sess->is_udp)
    {
        if ((resp = strdup("udp")) == NULL)
            return NULL;
    }
    else if (sess->is_rtp)
    {
        if ((resp = strdup("rtp")) == NULL)
            return NULL;
    }
    else if (sess->is_pipe)
    {
        if ((resp = strdup("pipe")) == NULL)
            return NULL;
    }
    else if (sess->is_file)
    {
        if ((resp = strdup("file")) == NULL)
            return NULL;
    }
    else
        return NULL;

    return resp;
}

static int set_gopsize(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        p->gop.field_val = strtoul(arg, NULL, 10);
        p->gop.change_pending = 1;
        dbg("GOP==%d", p->gop.field_val);
        return 0;
    }
    else
        return -1;
}

static char *get_gopsize(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = malloc(RESP_BUF_SZ)) == NULL)
            return NULL;
        memset(resp, 0, RESP_BUF_SZ);
        sprintf(resp, "%d", p->gop.field_val);
        dbg("GOP==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_bitrate(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        p->bitrate.field_val = strtoul(arg, NULL, 10);
        p->bitrate.change_pending = 1;
        dbg("Bitrate==%d", p->bitrate.field_val);
        return 0;
    }
    else
        return -1;
}

static char *get_bitrate(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = malloc(RESP_BUF_SZ)) == NULL)
            return NULL;
        memset(resp, 0, RESP_BUF_SZ);
        sprintf(resp, "%d", p->bitrate.field_val);
        dbg("Bitrate==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_framerate(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        p->framerate.field_val = strtoul(arg, NULL, 10);
        p->framerate.change_pending = 1;
        dbg("Framerate==%d", p->framerate.field_val);
        return 0;
    }
    else
        return -1;
}

static char *get_framerate(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = malloc(RESP_BUF_SZ)) == NULL)
            return NULL;
        memset(resp, 0, RESP_BUF_SZ);
        sprintf(resp, "%d", p->framerate.field_val);
        dbg("Framerate==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_rotation_angle(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        p->rotation_angle.field_val = strtoul(arg, NULL, 10);
        p->rotation_angle.change_pending = 1;
        dbg("Rotation_angle==%d", p->rotation_angle.field_val);
        return 0;
    }
    else
        return -1;
}

static char *get_rotation_angle(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = malloc(RESP_BUF_SZ)) == NULL)
            return NULL;
        memset(resp, 0, RESP_BUF_SZ);
        sprintf(resp, "%d", p->rotation_angle.field_val);
        dbg("Rotation_angle==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_output_ratio(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        p->output_ratio.field_val = strtoul(arg, NULL, 10);
        p->output_ratio.change_pending = 1;
        dbg("Output_ratio==%d", p->output_ratio.field_val);
        return 0;
    }
    else
        return -1;
}

static char *get_output_ratio(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = malloc(RESP_BUF_SZ)) == NULL)
            return NULL;
        memset(resp, 0, RESP_BUF_SZ);
        sprintf(resp, "%d", p->output_ratio.field_val);
        dbg("Output_ratio==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_mirror_angle(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        p->mirror_angle.field_val = strtoul(arg, NULL, 10);
        p->mirror_angle.change_pending = 1;
        dbg("Mirror_angle==%d", p->mirror_angle.field_val);
        return 0;
    }
    else
        return -1;
}

static char *get_mirror_angle(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = malloc(RESP_BUF_SZ)) == NULL)
            return NULL;
        memset(resp, 0, RESP_BUF_SZ);
        sprintf(resp, "%d", p->mirror_angle.field_val);
        dbg("Mirror_angle==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_compression(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        free(p->compression.field_str);
        p->compression.field_str = strdup(arg);
        p->compression.change_pending = 1;
        dbg("Compression==%s", p->compression.field_str);
        return 0;
    }
    else
        return -1;
}

static char *get_compression(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = strdup(p->compression.field_str)) == NULL)
            return NULL;
        dbg("Compression==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_resolution(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        free(p->resolution.field_str);
        p->resolution.field_str = strdup(arg);
        p->resolution.change_pending = 1;
        dbg("Resolution==%s", p->resolution.field_str);
        return 0;
    }
    else
        return -1;
}

static char *get_resolution(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = strdup(p->resolution.field_str)) == NULL)
            return NULL;
        dbg("Resolution==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int set_camera_name(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;

    if (sess != NULL && sess->video.params != NULL && arg != NULL)
    {
        p = sess->video.params;
        free(p->name.field_str);
        p->name.field_str = strdup(arg);
        p->name.change_pending = 1;
        dbg("Camera Name==%s", p->name.field_str);
        return 0;
    }
    else
        return -1;
}

static char *get_camera_name(struct sess_ctx *sess, char *arg)
{
    struct video_config_params *p;
    char *resp;

    if (sess != NULL && sess->video.params != NULL)
    {
        p = sess->video.params;
        if ((resp = strdup(p->name.field_str)) == NULL)
            return NULL;
        dbg("Camera Name==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static int update_video_conf(struct sess_ctx *sess, char *arg)
{
    if (sess != NULL && sess->video.params != NULL &&
            sess->video.params != NULL)
    {
        if (save_video_conf(sess->video.params,
                            sess->video.cfgfile) < 0)
        {
            dbg("error saving params to %s", sess->video.cfgfile);
        }
        else
            dbg("params updated");
    }
    return 0;
}

static int reset_video_conf(struct sess_ctx *sess, char *arg)
{
    return -1;
}

static int start_aud(struct sess_ctx *sess, char *arg)
{
    return -1;
}

static int stop_aud(struct sess_ctx *sess, char *arg)
{
    return -1;
}

static int restart_server(struct sess_ctx *sess, char *arg)
{
    if (sess == NULL)
        return 0;

    printf("enter restar_server now begin\n");
    playback_exit(sess->from);
    pthread_mutex_lock(&sess->sesslock);
    sess->running = 0;
    pthread_mutex_unlock(&sess->sesslock);
    printf("\nnow wait the monitor stop!\n ");
    return 0;
}

static char *get_firmware_revision(struct sess_ctx *sess, char *arg)
{
    char *resp;

    if (sess != NULL)
    {
        if ((resp = strdup(FIRMWARE_REVISION)) == NULL)
            return NULL;
        dbg("Firmware revision==%s\n", resp);
        return resp;
    }
    else
        return NULL;
}

static char *get_firmware_info(struct sess_ctx *sess, char *arg)
{
    char *resp;
    resp = malloc(64);
    if (!resp)
        return NULL;
    memset(resp , 0 , sizeof(resp));
    sprintf(resp, "%x", threadcfg.cam_id);
    return resp;
}

static char *get_video_state(struct sess_ctx *sess, char *arg)
{
    char *resp;
    char *state;

    if (sess != NULL)
    {
        if (sess->recording)
            if (sess->paused)
                state = "is_paused";
            else
                state = "is_recording";
        else if (sess->playing)
            if (sess->paused)
                state = "is_paused";
            else
                state = "is_playing";
        else
            state = "is_stopped";

        if ((resp = strdup(state)) == NULL)
            return NULL;
        dbg("Video state==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static char *get_motion_state(struct sess_ctx *sess, char *arg)
{
    char *resp;
    char *state;

    if (sess != NULL)
    {
        if (sess->connected && sess->motion_detected)
            state = "is_true";
        else
            state = "is_false";

        if ((resp = strdup(state)) == NULL)
            return NULL;
        dbg("motion state==%s", resp);
        return resp;
    }
    else
        return NULL;
}

static char *get_motion_detection(struct sess_ctx *sess, char *arg)
{
    return NULL;
}

static int enable_motion_detection(struct sess_ctx *sess, char *arg)
{
    return 0;
}

static int disable_motion_detection(struct sess_ctx *sess, char *arg)
{
    return 0;
}

static int SetRs485BautRate(char *arg)
{
    return 0;
}

static char stopcmd[8] = {0xa0 , 00 , 00 , 00 , 00 , 00 , 0xaf , 0x0f};

static int Rs485Cmd(char* arg)
{
    int length;
    char* buffer;

    length = arg[0];
    buffer = &arg[1];
    /*
    *buffer format         0xa0    0x0  {4bytes cmd}   0xaf  {last byte is the prev 7 bytes' xor}
    * up      0x00,0x08,0x00,0x30
    *down   0x00,0x10,0x00,0x30
    *left      0x00,0x04,0x30,0x00
    *right    0x00,0x02,0x30,0x00
    *stop     0x00,0x00,0x00,0x00
    */
    set_ignore_count(18);
    if (length == 8)
    {
        UartWrite(buffer, 8);
        if (memcmp(stopcmd , buffer , 8) != 0)
            usleep(150000);
    }
    else if (length == 9)
    {
        UartWrite(buffer , 8);
        usleep(150000);
        UartWrite(stopcmd , 8);
    }
    else
        dbg("get unkown length %d\n", length);

    return 0;
}

char *search_wifi(char *arg)
{
    int numssid;
    char *buf;
    int length;
    char slength[5];
    if (!arg)
        return NULL;
    if (*arg == '0')
    {
        buf = get_parse_scan_result(&numssid , NULL);
        if (!buf)
            return NULL;
        length = strlen(buf);
        memmove(buf + 4 , buf, length);
        memset(buf, 0, 4);
        sprintf(slength, "%04d", length);
        memcpy(buf, slength, 4);
        return buf;
    }
    else
        dbg("invalid argument\n");
    return NULL;
}

int snd_soft_restart();

char *get_clean_video_cfg(int *size)
{
    FILE *fp;
    char *cfg_buf;

    fp = fopen(RECORD_PAR_FILE, "r");
    if (!fp)
    {
        system("cp /video.cfg  /data/video.cfg");
        usleep(100000);
        fp = fopen(RECORD_PAR_FILE, "r");
        if (!fp)
            return NULL;
    }
    cfg_buf = (char *)malloc(2048);
    if (!cfg_buf)
    {
        printf("malloc buf for config file error\n");
        fclose(fp);
        return NULL;
    }
    memset(cfg_buf , 0 , 2048);
    fseek(fp , 0 ,  SEEK_END);
    *size = ftell(fp);
    fseek(fp , 0 , SEEK_SET);
    if (fread(cfg_buf , *size , 1 , fp) != 1)
    {
        printf("read configure file error\n");
        free(cfg_buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return cfg_buf;
}

static char *SetPswd(char*arg)
{
    FILE *fp;
    char *p;
    struct stat st;
    char buf[512];
    if (!arg)
    {
        return strdup(PASSWORD_FAIL);
    }
    if (*arg == ':')
    {
        if (stat(PASSWORD_FILE , &st) == 0)
            return strdup(PASSWORD_FAIL);
        else
        {
            arg++;
            if (strncmp(arg, PASSWORD_PART_ARG, strlen(PASSWORD_PART_ARG)) == 0)
            {
                fp = fopen(PASSWORD_FILE , "w");
                if (fp)
                {
                    if (fwrite(arg, 1, strlen(arg), fp) == strlen(arg))
                    {
                        fclose(fp);
                        memset(threadcfg.pswd , 0 , sizeof(threadcfg.pswd));
                        memcpy(threadcfg.pswd , arg , strlen(arg));
                        return strdup(PASSWORD_OK);
                    }
                    else
                    {
                        fclose(fp);
                        sprintf(buf, "rm %s", PASSWORD_FILE);
                        system(buf);
                        return strdup(PASSWORD_FAIL);
                    }
                }
            }
            return strdup(PASSWORD_FAIL);
        }
    }
    if (strncmp(arg, PASSWORD_PART_ARG, strlen(PASSWORD_PART_ARG)) != 0)
    {
        return strdup(PASSWORD_FAIL);
    }
    p = arg;
    while (*p && *p != ':')p++;
    if (*p != ':')
        return strdup(PASSWORD_FAIL);
    *p = 0;
    p++;
    if (strncmp(p, PASSWORD_PART_ARG, strlen(PASSWORD_PART_ARG)) != 0)
    {
        return strdup(PASSWORD_FAIL);
    }
    if (strlen(arg) != strlen(threadcfg.pswd) || strncmp(arg, threadcfg.pswd, strlen(arg)) != 0)
        return strdup(PASSWORD_FAIL);
    fp = fopen(PASSWORD_FILE , "w");
    if (fp)
    {
        if (fwrite(p, 1, strlen(p), fp) == strlen(p))
        {
            fclose(fp);
            memset(threadcfg.pswd , 0 , sizeof(threadcfg.pswd));
            memcpy(threadcfg.pswd, p, strlen(p));
            return strdup(PASSWORD_OK);
        }
        else
        {
            fclose(fp);
            sprintf(buf, "rm %s", PASSWORD_FILE);
            system(buf);
            return strdup(PASSWORD_FAIL);
        }
    }
    return strdup(PASSWORD_FAIL);
}

static char *CheckPswd(char *arg)
{
    FILE *fp;
    struct stat st;
    char buf[512];
    int pswd_size;
    if (!arg)
        return strdup(PASSWORD_FAIL);
    if (strncmp(arg, PASSWORD_PART_ARG, strlen(PASSWORD_PART_ARG)) != 0)
        return strdup(PASSWORD_FAIL);
    if (stat(PASSWORD_FILE , &st) == 0)
    {
        fp = fopen(PASSWORD_FILE, "r");
        if (fp)
        {
            memset(buf, 0, 512);
            pswd_size = fread(buf, 1, 512, fp);
            if (pswd_size > 0)
            {
                if (strlen(arg) == strlen(buf) && strncmp(arg, buf, strlen(arg)) == 0)
                {
                    fclose(fp);
                    return strdup(PASSWORD_OK);
                }
            }
            fclose(fp);
        }
    }
    return strdup(PASSWORD_FAIL);
}

static char *PswdState()
{
    struct stat st;
    if (stat(PASSWORD_FILE , &st) == 0)
        return strdup(PASSWORD_SET);
    else
        return strdup(PASSWORD_NOT_SET);
}

static char *cli_playback_set_status(struct sess_ctx *sess, char *arg)
{
    int value;
    char *rsp = NULL;
    char buf[32];
    if (!sess || !arg)
        return NULL;
    if (strncmp(arg, "SEEK", 4) == 0)
    {
        arg += 4;
        if (*arg != ':')
            return NULL;
        arg++;
        memset(buf, 0, 32);
        memcpy(buf, arg, strlen(arg));
        if (sscanf(buf, "%d", &value) != 1)
            return NULL;
        cmd_playback_set_status(sess->from, PLAYBACK_STATUS_SEEK, &value);
        rsp = strdup("SEEK");
    }
    else if (strncmp(arg, "PAUSED", 6) == 0)
    {
        cmd_playback_set_status(sess->from, PLAYBACK_STATUS_PAUSED, NULL);
        rsp = strdup("PAUSED");
    }
    else if (strncmp(arg, "RUNNING", 7) == 0)
    {
        cmd_playback_set_status(sess->from, PLAYBACK_STATUS_RUNNING, NULL);
        rsp = strdup("RUNNING");
    }
    return rsp;
}

static char *get_version_in_binary()
{
    char *v;
    int v1 , v2 , v3;
    v = (char *)malloc(32);
    memset(v, 0, 32);
    memcpy(v, APP_VERSION , strlen(APP_VERSION));
    sscanf(v , "version=%d.%d.%d", &v1 , &v2, &v3);
    v[7] = (char)v1;
    v[8] = (char)v2;
    v[9] = (char)v3;
    return v;
}

static char *GetVersion(int *rsp_len)
{
    char *buf;
    struct stat st;
    buf = get_version_in_binary();
    if (stat(ENCRYPTION_FILE_PATH , &st) == 0)
    {
        *rsp_len = 10;
    }
    else
    {
        *rsp_len = 3;
        buf[0] = buf[7];
        buf[1] = buf[8];
        buf[2] = buf[9];
    }
    return buf;
}

static char *GetConfig(char *arg , int *rsp_len)
{
    char ConfigType;
    int length = 0;
    char* ret = 0;
    char *p;
    char *s;
    int size;
    int cfg_len;
    char slength[5];
    if (!arg)
        return NULL;
    if (check_cli_pswd(arg, &p) != 0)
        return p;
    ConfigType = (int) * arg;
    ret = malloc(4096);
    if (!ret) return NULL;
    memset(ret , 0 , 4096);
    *rsp_len = 4;
    p = ret + 4;
    if (ConfigType == '1')
    {
        sprintf(p, APP_VERSION);
        (*rsp_len) += strlen(p);
        p += strlen(p);
        sprintf(p, "cam_id=%x\n", threadcfg.cam_id);
        (*rsp_len) += strlen(p);
        p += strlen(p);
        sprintf(p, MODEL_NAME);
        (*rsp_len) += strlen(p);
        p += strlen(p);
        s = get_clean_video_cfg(&cfg_len);
        if (!s)
        {
            length = 0;
        }
        else
        {
            memcpy(p , s , cfg_len);
            length = cfg_len;
            free(s);
        }
        (*rsp_len) += length;
        p += length;
        sprintf(p, "system_time=%s\n", gettimestamp());
        (*rsp_len) += strlen(p);
        p += strlen(p);
        sprintf(p, "tfcard_maxsize=%d\n", get_sdcard_size());
        (*rsp_len) += strlen(p);
        size = *rsp_len;

        sprintf(slength, "%4d", size - 4);
        memcpy(ret , slength , 4);
    }
    else
    {
        free(ret);
        printf("GetConfig invalid parmeter\n");
        printf("ConfigType==%c\n", ConfigType);
        return NULL;
    }
    return ret;
}

int set_raw_config_value(char *buffer);
extern char force_close_file ;

static void *save_parameters(char *arg)
{

}

static char* restore_parameters(char *arg)
{
	char buf[100];
	memset(buf, 0, 100);
	sprintf(buf,"%s:%d:%d:%d", "720p", 100, 100, 100);
	return strdup(buf);
}

static char *SetConfig(char *arg)
{
    char ConfigType;
    char *buf;
    char *p;
    int data_len;
    char sdata_len[5];
    int ret;
    int i;
    int size;

    if (!arg)
        return  NULL;
    ConfigType = (int) * arg;
    if (ConfigType == '0')
    {
        buf = (char *)malloc(256);
        if (!buf)
        {
            return NULL;
        }
        memset(buf, 0 , 256);
        sprintf(buf, "rm %s", PASSWORD_FILE);
        system(buf);
        memset(buf, 0 , 256);
        sprintf(buf, "cp %s %s", MONITOR_PAR_FILE , RECORD_PAR_FILE);
        system(buf);
        free(buf);
        dbg("reset the value to default\n");
        if (threadcfg.sdcard_exist)
            force_close_file = 1;
        else
            snd_soft_restart();
        return NULL;
    }
    if (ConfigType != '1' && ConfigType != '3')
    {
        dbg("invalid agument\n");
        return NULL;
    }

    if (ConfigType == '1')
    {
        buf = (char *)malloc(4096);
        if (!buf)
        {
            printf("error malloc buf for video.cfg\n");
            return NULL;
        }
        memset(buf, 0, 4096);
        ret = cmd_recv_msg(cli_socket , cli_st,  buf, 4096, NULL, NULL);
        if (ret <= 0)
        {
            dbg("error recv configure file\n");
            free(buf);
            return NULL;
        }
        memcpy(sdata_len , buf, 4);
        sdata_len[4] = 0;
        for (i  = 0 ; i < 5 ; i++)
        {
            if (sdata_len[i] >= '0' && sdata_len[i] <= '9')
                break;
        }
        if (i == 5)
        {
            dbg("error get configure file len\n");
            free(buf);
            return NULL;
        }
        p = sdata_len + i;
        if (sscanf(p , "%d", &data_len) != 1)
        {
            dbg("error get configure file len\n");
            free(buf);
            return NULL;
        }
        size = ret - 4;
        memmove(buf , buf + 4, size);
        memset(buf + size , 0 , 4);
        while (size < data_len)
        {
            ret =  stun_recvmsg(cli_socket,  buf + size, 4096 - size, NULL, NULL);
            if (ret <= 0)
            {
                dbg("recv configure file error\n");
                free(buf);
                return NULL;
            }
            size += ret;
        }
        set_raw_config_value(buf);
        free(buf);
        if (threadcfg.sdcard_exist)
            force_close_file = 1;
        else
            snd_soft_restart();
        return NULL;

    }
    else
        return NULL;
}

int set_system_time(char *time);

static void SetTime(char *arg)
{
    if (!arg)
        return;
    set_system_time(arg);
    return;
}

static char *AudioTalkOn(struct sess_ctx *sess)
{
    int r = sound_start_talk(sess);

    if (r == 0)
        return strdup("talk_ok");
    else
        return strdup("talk_busy");
}

static void AudioTalkOff(void)
{
    sound_stop_talk();
}

static void TalkVolume(char *arg)
{
    int hp_v, mc_v;

    if (!arg || sscanf(arg, "%i:%i", &hp_v, &mc_v) != 2)
        return;

    alsa_set_mic_volume(mc_v, 0);
    alsa_set_hp_volume(hp_v, 0);
}

extern struct vdIn *vdin_camera;

static void set_brightness(char *arg)
{
    char buf[32];
    int value;
	struct configstruct *allconfig;
	int elements;
	
    if (!arg)
        return;
    memset(buf, 0, 32);
    memcpy(buf, arg, strlen(arg));
    if (sscanf(buf, "%d", &value) != 1)
    {
        dbg("error brightness\n");
        return;
    }
#if 0
	allconfig = extract_config_file(&elements);
	set_value(allconfig,elements,CFG_BRIGHTNESS,0,&value);
	write_config_value(allconfig,elements);
	free(allconfig);
#endif

	encoder_para_changed_brightness(value);
}

static void set_contrast(char *arg)
{
    char buf[32];
    int value;
    if (!arg)
        return;
    memset(buf, 0, 32);
    memcpy(buf, arg, strlen(arg));
    if (sscanf(buf, "%d", &value) != 1)
    {
        dbg("error contrast\n");
        return;
    }
#ifdef ENCODER_IN_ONE_PROCESS
    if (v4l2_contrl_contrast(vdin_camera, value) == 0)
        threadcfg.contrast = value;
#else
	encoder_para_changed_contrast(value);
#endif
}

static void set_volume(char *arg)
{
    int value;

    if (!arg || sscanf(arg, "%d", &value) != 1)
        return;

    if (alsa_set_mic_volume(value, 1) == 0 &&
        alsa_set_hp_volume(value, 1) == 0)
    {
        threadcfg.volume = value;
    }
}

static void set_vide_format(char* arg)
{
	if( strncmp(arg, "qvga", 4) == 0 ){
		memset(threadcfg.resolution, 0, sizeof(threadcfg.resolution));
		memcpy(threadcfg.resolution, "qvga", 4);
		change_video_format = 1;
		return;
	}
	else if( strncmp(arg,"vga",3)== 0 ){
		memset(threadcfg.resolution, 0, sizeof(threadcfg.resolution));
		memcpy(threadcfg.resolution, "vga", 3);
		change_video_format = 1;
		return;
	}
	else if( strncmp(arg,"720p", 4 ) == 0 ){
		memset(threadcfg.resolution, 0, sizeof(threadcfg.resolution));
		memcpy(threadcfg.resolution, "720p", 4);
		change_video_format = 1;
		return;
	}
	else{
		return;
	}
}

static void set_vide_quality(char* arg)
{
    char buf[32];
    int value;
    if (!arg)
        return;
    memset(buf, 0, 32);
    memcpy(buf, arg, strlen(arg));
    if (sscanf(buf, "%d", &value) != 1)
    {
        dbg("error quality\n");
        return;
    }
	encoder_para_changed_quality(value);
	return;
}

static char* GetTime(char* arg)
{
    char* buffer;
    time_t timep;
    struct tm * gtm;

    buffer = malloc(20);
    memset(buffer, 0, 20);
    time(&timep);
    gtm = localtime(&timep);
    sprintf(buffer, "%04d%02d%02d%02d%02d%02d", gtm->tm_year + 1900, gtm->tm_mon + 1, gtm->tm_mday, gtm->tm_hour, gtm->tm_min, gtm->tm_sec);
    return buffer;
}

//0000003a:0007d060:20000101000253-20000101000255
#define MAX_ITEMS_ONE_SEND 20
static char FileNameBuffer[MAX_ALLOWED_FILE_NUMBER * 50];
static int total_file_number;

static char* GetNandRecordFile(char* arg)
{
    int start_sector;
    int next_secotor;
    char* time;
    int max_file_num;
    int i;
    int id = 0;
    char* buffer;
    int max_count;
    int rsp_size;
    char rsp_size_str[5];
    char *ret;

    if (!arg)
        return NULL;
    id = dec_string_to_int(arg, 8);
    dbg("get a id=%d\n", id);
    if (id == 0)
    {
        i = 0;
        memset(FileNameBuffer, 0, sizeof(FileNameBuffer));
        nand_open_simple("/dev/nand-user");
        max_file_num = nand_get_max_file_num();
        dbg("max file num=%d\n", max_file_num);
        //after nand_open, the oldest sector is found, so we get it
        start_sector = 0;
        time = nand_get_file_time(start_sector);
        if (time != 0 && time != (char*)0xffffffff)       // 0xffffffff means a deleted file
        {
            dbg("file at %d, time=%s\n", start_sector, time);
            memcpy(&FileNameBuffer[i * 48], time, 47);
            FileNameBuffer[i * 48 + 47] = 0;
            i++;
        }
        else if (time == (char*)0xffffffff)
            ;

        do
        {
            next_secotor = nand_get_next_file_start_sector(start_sector);
            if (next_secotor == -1)
                break;
            time = nand_get_file_time(next_secotor);
            if (!time || time == (char*)0xffffffff)
            {
                start_sector = next_secotor;
                continue;
            }
            memcpy(&FileNameBuffer[i * 48], time, 47);
            FileNameBuffer[i * 48 + 47] = 0;
            i++;
            start_sector = next_secotor;
        }
        while (1);
        nand_close_simple();
        total_file_number = i;
    }
    ret = malloc(total_file_number * 50 + 8 + 4 + 1);
    memset(ret, 0, total_file_number * 50 + 8 + 4 + 1);
    buffer = ret + 4;
    sprintf(buffer, "%08d", id);
    max_count = total_file_number;
    for (i = 0; i < max_count; i++)
    {
        sprintf(&buffer[8 + i * 48], "%s\n", &FileNameBuffer[id * 48 * 20 + i * 48]);
    }

    dbg("\n%s\n", &buffer[8]);
    rsp_size = strlen(buffer);
    sprintf(rsp_size_str , "%04d", rsp_size);
    memcpy(ret , rsp_size_str , 4);
    return ret;
}

static int IsFileExist(char* filename)
{
    int ret;
    struct stat file;
    ret = stat(filename, &file);
    if (ret == 0)
    {
        ret = 1;
    }
    else
    {
        ret = 0;
    }
    return ret;
}

static int IsDhcpMode()
{
    int ret;
    struct stat file;
    ret = stat("/tmp/static", &file);
    if (ret == 0)
    {
        ret = 0;
    }
    else
    {
        ret = 1;
    }
    return ret;
}

static char* version = "V 1.43M";

static char* GetRecordStatue(char* arg)
{
    char* buffer;
    int size;
    int state = 1;
    char* ip;
    int ret;

    size = nand_get_size("/dev/nand-user");
    size = size / (1024 * 1024 / 512); // GET SIZE IN  M
    buffer = malloc(200);
    memset(buffer, 0, 200);
    if (state != 2 && state != 3)
    {
        state = 1;
    }
    ret = IsDhcpMode();
    if (ret == 0)
    {
        ip = "s";
    }
    else
    {
        ip = "d";
    }

    sprintf(buffer, "%s\n%8d\n%8d\n%1d\n%s\n", version, size, 0, state, ip);
    return buffer;
}

static int vs_set_record_config(int mode)
{
    int fd;
    char* buffer;

    printf("%s\n", __func__);
    fd = open("/dev/nand-data", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        perror("open nand-data");
        return -1;
    }
    buffer = malloc(512 * 1024);
    memset(buffer, 0, 512 * 1024);
    if (read(fd, buffer, 512 * 1024) != 512 * 1024)
    {
        printf("read error\n");
        return -1;
    }
    buffer[4096] = (char)mode;
    close(fd);
    fd = open("/dev/nand-data", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        perror("open nand-data");
        return -1;
    }
    write(fd, buffer, 512 * 1024);
    close(fd);
    system("sync");
    system("/nand-flush /dev/nand-data");
    system("sync");
    return 0;
}

static void SetRecordStatue(char* arg)
{
    char mode;

    if (arg[0] == '2')
    {
        mode = 2;
    }
    else if (arg[0] == '3')
    {
        mode = 3;
    }
    else
    {
        mode = 1;
    }
    usleep(500);
    vs_set_record_config(mode);

    v2ipd_restart_all();
    return;
}

static int ArrangIpAddress(char* ip)
{
    int i, j;
    char tmp[20];
    char p_dot, *p1;
    int valid = 0;
    memset(tmp, 0, 20);
    p1 = ip;
    p_dot = 1;
    for (i = 0, j = 0; i < 15; p1++, i++)
    {
        if (*p1 == '.')
        {
            tmp[j++] = *p1;
            p_dot = 1;
            continue;
        }
        else if (p1[0] == '0' && p1[1] == '0' && p1[2] == '0' && p_dot == 1)
        {
            tmp[j++] = '0';
            i += 2;
            p1 += 2;
            continue;
        }
        else if (*p1 == '0' && p_dot == 1)
        {
            continue;
        }
        else
        {
            tmp[j++] = *p1;
            p_dot = 0;
            valid = 1;
            continue;
        }
    }
    if (valid == 0)
    {
        return -1;
    }
    memcpy(ip, tmp, strlen(tmp) + 1);
    return 0;
}

extern vs_share_mem* v2ipd_share_mem;

static void SetIpAddress(char* arg)
{
    char buffer_ip[20];
    char buffer_mask[20];
    char buffer_gateway[20];
    char cmd[100];

    dbg("ip address=:%s\n", arg);
    if (IsDhcpMode())
    {
        return;
    }

    v2ipd_share_mem->v2ipd_to_client0_msg = VS_MESSAGE_STOP_MONITOR;
    usleep(500);


    memset(buffer_ip, 0, 20);
    memset(buffer_mask, 0, 20);
    memset(buffer_gateway, 0, 20);
    memcpy(buffer_ip, arg, 15);
    memcpy(buffer_mask, &arg[15 + 1], 15);
    memcpy(buffer_gateway, &arg[15 + 1 + 15 + 1], 15);
    if (ArrangIpAddress(buffer_ip) == -1)
    {
        return;
    }
    if (-1 == ArrangIpAddress(buffer_mask))
    {
        return;
    }
    if (-1 == ArrangIpAddress(buffer_gateway))
    {
        return;
    }
    memset(cmd, 0, 100);
    sprintf(cmd, "ifconfig eth0 %s netmask %s", buffer_ip, buffer_mask);
    dbg("%s\n", cmd);
    system(cmd);
    system("wait");
    sprintf(cmd, "route add default   gw  %s", buffer_gateway);
    dbg("%s\n", cmd);
    system(cmd);
    system("wait");

    if (strlen(arg) != 48)
    {
        dbg("invalid ip length %d\n", strlen(arg));
        return;
    }
    int fd;
    char* buffer;
    fd = open("/dev/nand-data", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        perror("open nand-data");
        return;
    }
    buffer = malloc(512 * 1024);
    memset(buffer, 0, 512 * 1024);
    if (read(fd, buffer, 512 * 1024) != 512 * 1024)
    {
        printf("read error\n");
        return;
    }
    memcpy(&buffer[0x4000], arg, strlen(arg));
    close(fd);
    fd = open("/dev/nand-data", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        perror("open nand-data");
        return;
    }
    write(fd, buffer, 512 * 1024);
    close(fd);
    system("sync");
    system("/nand-flush /dev/nand-data");
    system("sync");
    return;
}

static void ReplayRecord(struct sess_ctx *sess, char* arg)
{
    int file_id;
    int seek;
    if (!arg)
        return;
    file_id = (int) hex_string_to_uint(arg, 8);
    seek = (int)hex_string_to_uint(&arg[8], 8);
    printf("%s: address=0x%x, port=%d, seek=%d\n",
           __func__, g_cli_ctx->from.sin_addr.s_addr,
           ntohs(g_cli_ctx->from.sin_port), seek);
    playback_new(g_cli_ctx->from, file_id, seek);
    return;
}

static char* DeleteAllFiles(struct sess_ctx *sess, char* arg)
{
    char * ret;
    ret = malloc(20);
    ret = "DeleteAllFilesOK";
    nand_clean();
    return ret;
}


static char* DeleteFile(struct sess_ctx *sess, char* arg)
{
    char *ret;
    int file_id, file_end_id;

    ret = malloc(50);
    file_id = (int) hex_string_to_uint(arg, 8);
    file_end_id = (int)hex_string_to_uint(&arg[8], 8);
    if (strlen(arg) != 16)
    {
        sprintf(ret, "DeleteFile%08xERROR", file_id);
        return ret;
    }
    sprintf(ret, "DeleteFile%08xOK", file_id);
    nand_invalid_file(file_id, file_end_id);
    return ret;
}

static char *reboot(void)
{
    system("reboot");
    return NULL;
}

static const char *cli_cmds = "null";

static char *do_cli_cmd(void *sess,
                        char *cmd,
                        char *param,
                        int   size,
                        int  *rsp_len)
{
    char *resp = NULL;
    *rsp_len   = 0;

    /*I think we should seperate two class commands
    * the first class we do not need to check the sessction ,because excute  this commands do not need a sessction!
    * the second class we must check the sesstion , if the sessction do not exist it must something wrong!
    * I am sorry that the man who the first one setup this program cannot foreseen the future extension of this program!
    * now I have to patch it in a clumsy way ^_^
    */

    /*the first class */

    if (!cmd)
        return NULL;
    if (strncmp(cmd, "get_firmware_info", 17) == 0)
        return resp = get_firmware_info(sess, NULL);
    else if (strncmp(cmd, "GetRecordStatue", 15) == 0)
        return resp = GetRecordStatue(param);
    else if (strncmp(cmd, "GetNandRecordFile", 17) == 0)
        return resp = GetNandRecordFile(param);
    else if (strncmp(cmd, "ReplayRecord", 12) == 0)
    {
        ReplayRecord(sess, param);
        return resp;
    }
    else if (strncmp(cmd, "GetConfig", 9) == 0)
        return  resp = GetConfig(param , rsp_len);
    else if (strncmp(cmd, "SetConfig", 9) == 0)
    {
        resp =  SetConfig(param);
        return resp;
    }
    else if (strncmp(cmd, "search_wifi", 11) == 0)
        return search_wifi(param);
    else if (strncmp(cmd, "set_pswd", 8) == 0)
    {
        return SetPswd(param);
    }
    else if (strncmp(cmd, "check_pswd", 10) == 0)
    {
        return CheckPswd(param);
    }
    else if (strncmp(cmd, "pswd_state", 10) == 0)
    {
        return PswdState();
    }
    else if (strncmp(cmd, "SetTime", 7) == 0)
    {
        SetTime(param);
        return NULL;
    }
    else if (strncmp(cmd, "GetTime", 7) == 0)
        return   GetTime(param);
    else if (strncmp(cmd, "DeleteAllFiles", 14) == 0)
        return DeleteAllFiles(sess, param);
    else if (strncmp(cmd, "DeleteFile", 10) == 0)
        return DeleteFile(sess, param);
    else if (strncmp(cmd , "getversion", 10) == 0)
        return GetVersion(rsp_len);
    else if (strncmp(cmd , "AudioTalkOn", 11) == 0)
    {
        resp = AudioTalkOn(sess);
        return resp;
    }
    else if (strncmp(cmd , "AudioTalkOff", 12) == 0)
    {
        AudioTalkOff();
        return NULL;
    }
    else if (strncmp(cmd , "TalkVolume", 10) == 0)
    {
        TalkVolume(param);
        return NULL;
    }

    /*the second class*/
    if (sess == NULL || cmd == NULL)
        return NULL;

    if (strncmp(cmd, "set_brightness", 14) == 0)
        set_brightness(param);
    else if (strncmp(cmd, "set_contrast", 12) == 0)
        set_contrast(param);
    else  if (strncmp(cmd, "set_volume", 10) == 0)
        set_volume(param);
    else if (strncmp(cmd, "set_dest_addr", 13) == 0)
        set_dest_addr(sess, param);
    else if (strncmp(cmd, "set_dest_port", 13) == 0)
        set_dest_port(sess, param);
    else if (strncmp(cmd, "start_video", 11) == 0)
        start_vid(sess, NULL);
    else if (strncmp(cmd, "pause_video", 11) == 0)
        pause_vid(sess, NULL);
    else if (strncmp(cmd, "stop_video", 10) == 0)
        stop_vid(sess, NULL);
    else if (strncmp(cmd, "start_audio", 11) == 0)
        start_aud(sess, NULL);
    else if (strncmp(cmd, "stop_audio", 10) == 0)
        stop_aud(sess, NULL);
    else if (strncmp(cmd, "set_transport_type", 18) == 0)
        return  set_transport_type(sess, param , rsp_len);
    else if (strncmp(cmd, "set_gopsize", 11) == 0)
        set_gopsize(sess, param);
    else if (strncmp(cmd, "set_bitrate", 11) == 0)
        set_bitrate(sess, param);
    else if (strncmp(cmd, "set_framerate", 13) == 0)
        set_framerate(sess, param);
    else if (strncmp(cmd, "set_rotation_angle", 18) == 0)
        set_rotation_angle(sess, param);
    else if (strncmp(cmd, "set_output_ratio", 16) == 0)
        set_output_ratio(sess, param);
    else if (strncmp(cmd, "set_mirror_angle", 16) == 0)
        set_mirror_angle(sess, param);
    else if (strncmp(cmd, "set_compression", 15) == 0)
        set_compression(sess, param);
    else if (strncmp(cmd, "set_resolution", 14) == 0)
        set_resolution(sess, param);
    else if (strncmp(cmd, "get_transport_type", 18) == 0)
        resp = get_transport_type(sess, NULL);
    else if (strncmp(cmd, "get_gopsize", 11) == 0)
        resp = get_gopsize(sess, NULL);
    else if (strncmp(cmd, "get_bitrate", 11) == 0)
        resp = get_bitrate(sess, NULL);
    else if (strncmp(cmd, "get_framerate", 13) == 0)
        resp = get_framerate(sess, NULL);
    else if (strncmp(cmd, "get_rotation_angle", 18) == 0)
        resp = get_rotation_angle(sess, NULL);
    else if (strncmp(cmd, "get_output_ratio", 16) == 0)
        resp = get_output_ratio(sess, NULL);
    else if (strncmp(cmd, "get_mirror_angle", 16) == 0)
        resp = get_mirror_angle(sess, NULL);
    else if (strncmp(cmd, "get_compression", 15) == 0)
        resp = get_compression(sess, NULL);
    else if (strncmp(cmd, "get_resolution", 14) == 0)
        resp = get_resolution(sess, NULL);
    else if (strncmp(cmd, "update_video_conf", 17) == 0)
        update_video_conf(sess, NULL);
    else if (strncmp(cmd, "reset_video_conf", 16) == 0)
        reset_video_conf(sess, NULL);
    else if (strncmp(cmd, "restart_server", 14) == 0)
        restart_server(sess, NULL);
    else if (strncmp(cmd, "get_firmware_rev", 16) == 0)
        resp = get_firmware_revision(sess, NULL);
    else if (strncmp(cmd, "get_video_state", 15) == 0)
        resp = get_video_state(sess, NULL);
    else if (strncmp(cmd, "get_camera_name", 15) == 0)
        resp = get_camera_name(sess, NULL);
    else if (strncmp(cmd, "set_camera_name", 15) == 0)
        set_camera_name(sess, param);
    else if (strncmp(cmd, "reboot", 6) == 0)
        resp = reboot();
    else if (strncmp(cmd, "get_motion_state", 16) == 0)
        resp = get_motion_state(sess, param);
    else if (strncmp(cmd, "get_motion_detection", 20) == 0)
        resp = get_motion_detection(sess, NULL);
    else if (strncmp(cmd, "enable_motion_detection", 23) == 0)
        enable_motion_detection(sess, param);
    else if (strncmp(cmd, "disable_motion_detection", 24) == 0)
        disable_motion_detection(sess, param);
    else if (strncmp(cmd, "SetRs485BautRate", 16) == 0)
        SetRs485BautRate(param);
    else if (strncmp(cmd, "Rs485Cmd", 8) == 0)
        Rs485Cmd(param);
    else if (strncmp(cmd, "SetRecordStatue", 15) == 0)
        SetRecordStatue(param);
    else if (strncmp(cmd, "SetIpAddress", 12) == 0)
        SetIpAddress(param);
    else if (strncmp(cmd, "pb_set_status", 13) == 0)
        resp = cli_playback_set_status(sess, param);
    else if (strncmp(cmd, "set_video_fmt", 13) == 0)
        set_vide_format(param);
    else if (strncmp(cmd, "set_video_quality", 17) == 0)
        set_vide_quality(param);
    else if (strncmp(cmd, "save_parameters", 15) == 0)
        save_parameters(param);
    else if (strncmp(cmd, "restore_parameters", 18) == 0)
        resp = restore_parameters(param);
    else
        dbg("unrecognized command: %s\n", cmd);

    return resp;
}

static int CurrentUpdatedCount; //the id that has been sucessfully updated

#define TEMP_FILE_SIZE 10000

static char* UpdateSystem(char *cmd, int cmd_len, int* rsp_len)
{
    int ret = 0;
    int UpdateCount = 0;
    char* p = &cmd[13];
    unsigned char xor;
    int i;
    FILE* fp;
    int fw;
    char* tempfile = "/tmp/update";
    char* ret_buf;
    int data_size;
    int size_uboot, size_kernel, size_rootfs;
    char* file_buf;

    UpdateCount = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)  ;
    p += 4;
    if ((UpdateCount % 100) == 0 || UpdateCount == 0xffffffff)
        dbg("UpdateCount=%d\n", UpdateCount);

    if (UpdateCount == 0)
    {
        fp = fopen(tempfile, "w");
        fclose(fp);
        system("sync");
        CurrentUpdatedCount = -1;
        v2ipd_request_timeover_protect();
    }
    if (UpdateCount == CurrentUpdatedCount)
        ret = 0;
    else if (UpdateCount == CurrentUpdatedCount + 1)
    {
        v2ipd_request_timeover_protect();
        data_size = 995;
        do
        {
            xor = p[0];
            for (i = 1; i < data_size; i++)
            {
                xor ^= p[i];
            }
            if (xor != p[data_size])
            {
                ret = 1;
                break;
            }
            fp = fopen(tempfile, "a");
            if (fp == NULL)
            {
                ret = 2;    // err restart
                goto err_restart;
            }
            else
            {
                ret = fwrite(p, 1, data_size, fp);
                if (ret != data_size * 1)
                {
                    ret = 2; // err restart
                    fclose(fp);
                    goto err_restart;
                }
                else
                {
                    fclose(fp);
                    ret = 0;
                    CurrentUpdatedCount++;
                }
            }
        }
        while (0);
    }
    else if (UpdateCount == 0xffffffff)      // last package
    {
        v2ipd_stop_timeover_protect();
        data_size = cmd_len - 13 - 4 - 1;
        do
        {
            xor = p[0];
            for (i = 1; i < data_size; i++)
            {
                xor ^= p[i];
            }
            if (xor != p[data_size])
            {
                ret = 0x300;
                break;
            }
            fp = fopen(tempfile, "a");
            if (fp == NULL)
            {
                ret = 0x200;    // err restart
                break;
            }
            else
            {
                ret = fwrite(p, 1, data_size, fp);
                if (ret != data_size)
                {
                    ret = 0x201; // err restart
                    fclose(fp);
                    break;
                }
                else
                {
                    fclose(fp);
                    ret = 0;
                }
            }
            if (ret != 0)
            {
                system("rm /tmp/update");
                break;
            }
            // now we are ready to update the system
            v2ipd_disable_write_nand();
            sleep(1);
            system("cp /sbin/reboot /tmp/reboot -f");
            system("sync");
            if (IsFileExist("/tmp/reboot") == 0)
            {
                dbg("cope reboot error\n");
                system("rm /tmp/update");
                ret = 0x101;
                break;
            }
            system("cp /nand-flush /tmp/nand-flush -f");
            system("sync");
            if (IsFileExist("/tmp/nand-flush") == 0)
            {
                dbg("cope nand-flush error\n");
                system("rm /tmp/update");
                ret = 0x102;
                break;
            }
            system("cp /reboot-delay.sh /tmp/reboot-delay.sh -f");
            system("sync");
            if (IsFileExist("/tmp/reboot-delay.sh") == 0)
            {
                dbg("cope reboot-delay error\n");
                system("rm /tmp/update");
                ret = 0x103;
                break;
            }
            file_buf = malloc(TEMP_FILE_SIZE);
            if (file_buf == NULL)
            {
                ret = 0x104;
                break;
            }
            fp = fopen(tempfile, "r");
            if (fp == NULL)
            {
                ret = 0x105;
                break;
            }
            fseek(fp, 0, SEEK_SET);
            fread(&size_uboot, 1, 4, fp);
            dbg("uboot size=%d\n", size_uboot);
            if (size_uboot)
            {
                fw = open("/dev/nand-boot", O_RDWR | O_SYNC);
                if (fw == -1)
                {
                    ret = 0x106;
                    break;
                }
                while (size_uboot > TEMP_FILE_SIZE)
                {
                    fread(file_buf, 1, TEMP_FILE_SIZE , fp);
                    write(fw, file_buf, TEMP_FILE_SIZE);
                    size_uboot -= TEMP_FILE_SIZE;
                }
                if (size_uboot)
                {
                    fread(file_buf, 1, size_uboot , fp);
                    write(fw, file_buf, size_uboot);
                }
                close(fw);
                system("sync");
                system("/tmp/nand-flush /dev/nand-boot");
                system("sync");
            }
            fread(&size_kernel, 1, 4, fp);
            dbg("kernel size=%d\n", size_kernel);
            if (size_kernel)
            {
                fw = open("/dev/nand-kernel", O_RDWR | O_SYNC);
                if (fw == -1)
                {
                    ret = 0x107;
                    break;
                }
                while (size_kernel > TEMP_FILE_SIZE)
                {
                    fread(file_buf, 1, TEMP_FILE_SIZE , fp);
                    write(fw, file_buf, TEMP_FILE_SIZE);
                    size_kernel -= TEMP_FILE_SIZE;
                }
                if (size_kernel)
                {
                    fread(file_buf, 1, size_kernel , fp);
                    write(fw, file_buf, size_kernel);
                }
                close(fw);
                system("sync");
                system("/tmp/nand-flush /dev/nand-kernel");
                system("sync");
            }
            fread(&size_rootfs, 1, 4, fp);
            dbg("rootfs size=%d\n", size_rootfs);
            if (size_rootfs)
            {
                fw = open("/dev/nand-rootfs", O_RDWR | O_SYNC);
                if (fw == -1)
                {
                    ret = 0x108;
                    break;
                }
                while (size_rootfs > TEMP_FILE_SIZE)
                {
                    fread(file_buf, 1, TEMP_FILE_SIZE , fp);
                    write(fw, file_buf, TEMP_FILE_SIZE);
                    size_rootfs -= TEMP_FILE_SIZE;
                }
                if (size_rootfs)
                {
                    fread(file_buf, 1, size_rootfs , fp);
                    write(fw, file_buf, size_rootfs);
                }
                close(fw);
                system("sync");
                system("/tmp/nand-flush /dev/nand-rootfs");
                system("sync");
            }
            fclose(fp);
            free(file_buf);
            ret = 0;
        }
        while (0);
        CurrentUpdatedCount = 0xffffffff;
        if (ret == 0)
        {
        }
        else
        {
            system("rm /tmp/update");
        }
        v2ipd_reboot_system();
    }
    else if (UpdateCount == 0xfffffffe)
    {
        system("rm /tmp/update");
        system("sync");
        ret = 0;
        v2ipd_stop_timeover_protect();
    }
    else
    {
        ret = 2;
        v2ipd_stop_timeover_protect();
    }

    ret_buf = malloc(10);
    memset(ret_buf, 0, 10);
    p = &cmd[13];
    ret_buf[0] = p[0];
    ret_buf[1] = p[1];
    ret_buf[2] = p[2];
    ret_buf[3] = p[3];
    ret_buf[4] = ret;
    *rsp_len = 8;
    return ret_buf;

err_restart:
    v2ipd_reboot_system();
    ret_buf = malloc(10);
    memset(ret_buf, 0, 10);
    p = &cmd[13];
    ret_buf[0] = p[0];
    ret_buf[1] = p[1];
    ret_buf[2] = p[2];
    ret_buf[3] = p[3];
    ret_buf[4] = ret;
    *rsp_len = 8;
    return ret_buf;
}

char *do_cli_cmd_bin(void *sess, char *cmd, int cmd_len, int size, int *rsp_len)
{
    *rsp_len = 0;

    if (strncmp(cmd, "UpdateSystem", 12) == 0)
    {
        return UpdateSystem(cmd, cmd_len, rsp_len);
    }

    return NULL;
}

struct cli_sess_ctx *start_cli(void *arg)
{
    struct cli_sess_ctx * cli_ctx;

    cli_cmd_handler.cmds        = (char *)cli_cmds;
    cli_cmd_handler.handler     = do_cli_cmd;
    cli_cmd_handler.handler_bin = do_cli_cmd_bin;

    if ((cli_ctx = cli_init(arg)) != NULL)
    {
        if (cli_bind_cmds(cli_ctx, &cli_cmd_handler) < 0)
            return NULL;
    }
    else
        return NULL;

    return cli_ctx;
}

int extract_value(struct configstruct *allconfig, int elements, char *name, int is_string, void *dst)
{
    int i;
    char *strp;
    int * intp;
    for (i = 0; i < elements; i++)
    {
        if (strncmp(allconfig[i].name, name, strlen(name)) == 0)
        {
            if (is_string)
            {
                strp = (char *)dst;
                memcpy(strp, allconfig[i].value, 64);
            }
            else
            {
                intp = (int *)dst;
                *intp = atoi(allconfig[i].value);
            }
            return 0;
        }
    }
    return -1;
}

int set_value(struct configstruct *allconfig, int elements, char *name, int is_string, void *value)
{
    int i;
    for (i = 0; i < elements; i++)
    {
        if (strncmp(allconfig[i].name, name, strlen(name)) == 0)
        {
        	printf("find element\n");
            if (is_string)
            {
                memcpy(allconfig[i].value, value, 64);
            }
            else
            {
                memset(allconfig[i].value, 0, 64);
                sprintf(allconfig[i].value, "%d", *(int*)value);
            }
            return 0;
        }
    }
    return -1;
}

struct configstruct * extract_config_file(int* element_numbers)
{
	int lines;
    FILE*fd;
    char buf[512];
    struct configstruct *conf_p;
	
	printf("try to read config file\n");
    fd = fopen(RECORD_PAR_FILE, "r");
    if (fd == NULL)
    {
		printf("open video.cfg error\n");
		system("reboot");
	}
	conf_p = (struct configstruct *)calloc(100, sizeof(struct configstruct));
	if (!conf_p)
	{
		printf("unable to calloc 100 configstruct \n");
		system("reboot");
	}
	memset(conf_p, 0, 100 * sizeof(struct configstruct));
	lines = 0;
	memset(buf, 0, 512);
	while (fgets(buf, 512, fd) != NULL)
	{
		char *sp = buf;
		char *dp = conf_p[lines].name;
		while (*sp == ' ' || *sp == '\t')sp++;
		while (*sp != '=')
		{
			*dp = *sp;
			dp++;
			sp++;
		}
		sp++;
		while (*sp && (*sp == ' ' || *sp == '\t'))sp++;
		dp = conf_p[lines].value;
		while (*sp && *sp != '\n')
		{
			*dp = *sp;
			dp++;
			sp++;
		}
		printf("name==%s , value=%s\n", conf_p[lines].name, conf_p[lines].value);
		lines++;
		memset(buf, 0, 512);
	}

	*element_numbers = lines;
	
	fclose(fd);
	return conf_p;
}

int write_config_value(struct configstruct *allconfig, int elements)
{
    FILE*fp;
    int i;
    int len;
    char buf[512];
    fp = fopen(RECORD_PAR_FILE, "w");
    if (!fp)
    {
        printf("write configure file error , something wrong\n");
        return -1;
    }
    for (i = 0; i < elements; i++)
    {
        memset(buf, 0, 512);
        sprintf(buf, "%s=%s\n", allconfig[i].name, allconfig[i].value);
        len = strlen(buf);
        if (len != fwrite(buf, 1, len, fp))
        {
            printf("write config file error\n");
        }
    }
    fflush(fp);
    fclose(fp);
    return 0;
}


