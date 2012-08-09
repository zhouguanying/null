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
#include <linux/fs.h>
#include <netdb.h>
#include <sys/ioctl.h>

#include "includes.h"
//#include <defines.h>
#include "mail_alarm.h"
#include "cli.h"
#include "sockets.h"
#include "rtp.h"
//#include "mpegts.h"
#include "config.h"
//#include "encoder.h"
#include "revision.h"
//#include "ipl.h"
#include "nand_file.h"
#include "vpu_server.h"
#include "server.h"
#include "playback.h"
#include "record_file.h"
#include "utilities.h"
//#include "monitor.h"
#include "v4l2uvc.h"
#include "sound.h"
#include "picture_info.h"

/* Process ID file name */
#define PID_FILE    "/var/run/v2ipd.pid"
#define LOG_FILE    "/tmp/v2ipd.log"
char *v2ipd_logfile = LOG_FILE; /* Fix me */

int nand_fd;

/* Externs */
extern void (*sigset(int sig, void (*disp)(int)))(int);
extern int sigignore(int sig);

/* Debug */
#define ENCODER_DBG
#ifdef ENCODER_DBG
extern char *v2ipd_logfile;
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif

#undef AUDIO_SUPPORTED
#undef AUDIO_STATS
#undef VIDEO_STATS
#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

/**
 * sess_ctx - session context data structure
 * @name: ASCII based session name
 * @ip: server ip address
 * @port: server port number
 * @s1: server socket number
 * @s2: connected client socket
 * @s1_type: socket type (STREAM or DATAGRAM)
 * @myaddr: socket address used for tcp connections
 * @to: remote ip address used to send DATAGRAMs
 * @from: remote ip address used to receive DATAGRAMs
 * @video: video configuration params
 * @audio: audio configuration params
 * @cfgfile: configuration file
 * @nbytes: total number of bytes transferred
 * @tv: time values
 * @curr_frame_idx: current frame index value
 * @params: configuration parameters
 * @sess: driver session context
 * @daemon: session is running in background mode
 * @connected: session connection status
 * @rtp_sess: rtp session context
 * @use_rtp: indicates that session is rtp based
 * @recording: indicates recording session is active
 * @paused: pauses session
 * @cli_sess: CLI session context
 * @pipe_fd: named pipe file descriptor
 * @pipe_name: named pipe path name
 * @is_tcp: session data sent over tcp
 * @is_rtp: session data sent over rtp - requires rtsp
 * @is_udp: session data sent over udp - requires mpeg2 ts
 * @is_pipe: session data sent over named pipe
 * @running session is running
 * @soft_reset: perform soft restart
 */
/* Housekeeping globals used termintate program */
struct sess_ctx *global_ctx; // EJA


/*why we need this lock because i found when i change the video or write file to disck 
* together one thread(start_video_monitor) start or exit , the video and the write function will be error
*
*/
static pthread_mutex_t     v_thread_sleep_t_lock;
static int v_thread_sleep_time ;
static unsigned char do_update_flag = 0;

pthread_mutex_t global_ctx_lock;
struct sess_ctx *global_ctx_running_list=NULL; //each connection have a sesction
int 	     		     currconnections=0;

struct threadconfig threadcfg;  //we use this variable to control the the speed of record thread
int msqid= -1;

/*
*each sess have a id , we use this id to manage the buffer of audio and video
*i think we will use it for more in future
*/

pthread_mutex_t g_sess_id_mask_lock;
char g_sess_id_mask[MAX_NUM_IDS];
/*
int 			     globalsocket=-1; 
we have wanted to  use this socket to listen,
now we create a listen socket each set_transport_type 
then close it after we accpet.
not need the global listen socket now!*/

int daemon_msg_queue;
int v2ipd_shm_id;
vs_share_mem* v2ipd_share_mem;

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
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
		system("reboot &");
		exit(0);
	}
	printf("##############send update msg ok exit now###############\n");
	exit(0);
}

void prepare_do_update()
{
	set_msg_do_update();
	//set_do_update();
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
	v_thread_sleep_time = (1000000/threadcfg.framerate)>>1;
}
void increase_video_thread_sleep_time()
{
	/*
	pthread_mutex_lock(&v_thread_sleep_t_lock);
	if((v_thread_sleep_time +100000)<=501000)
		v_thread_sleep_time +=100000;
	printf("####################increase sleep time %d###############\n",v_thread_sleep_time);
	pthread_mutex_unlock(&v_thread_sleep_t_lock);
	*/
}
void handle_video_thread()
{
/*
	int time;
	pthread_mutex_lock(&v_thread_sleep_t_lock);
	time = v_thread_sleep_time;
	if((v_thread_sleep_time-100)>=1000)
		v_thread_sleep_time -=100;
	pthread_mutex_unlock(&v_thread_sleep_t_lock);
	//printf("sleep the time is %d\n",time);
	usleep(time);
	*/
	usleep(v_thread_sleep_time);
}

void init_g_sess_id_mask()
{
	pthread_mutex_init(&g_sess_id_mask_lock,NULL);
	memset(g_sess_id_mask,0,sizeof(g_sess_id_mask));
	g_sess_id_mask[MAX_NUM_IDS-1] = 1; /*this is be used by record thread*/
}

int get_sess_id()
{
	//static int count = 0;
	int i;
	int ret;
	ret = -1;
	pthread_mutex_lock(&g_sess_id_mask_lock);
	for(i=0;i<MAX_NUM_IDS;i++){
		if(g_sess_id_mask[i]==0){
			g_sess_id_mask[i] = 1;
			ret  = i;
			//count ++;
			//printf("######################get id index = %d count==%d#####################\n",i , count);
			break;
		}
	}
	pthread_mutex_unlock(&g_sess_id_mask_lock);
	return ret;
}

void put_sess_id(int index)
{
	//static int count = 0;
	//count ++;
	//printf("###############put id index=%d count=%d##########################\n",index ,count);
	pthread_mutex_lock(&g_sess_id_mask_lock);
	g_sess_id_mask[index] = 0;
	pthread_mutex_unlock(&g_sess_id_mask_lock);
}

void v2ipd_restart_all()
{
	vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_RESTART_ALL;
	printf("%s\n", __func__);
	if( msgsnd( daemon_msg_queue, &msg, sizeof(msg)-sizeof(long), IPC_NOWAIT ) < 0 ) {
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
	if( msgsnd( daemon_msg_queue, &msg, sizeof(msg)-sizeof(long), IPC_NOWAIT ) < 0 ) {
		perror("msgsnd");
	}
	return;
}

void v2ipd_disable_write_nand()	// if disable_wirte_nand, means update is writing nand and will reboot the system right now
{
	vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_DISABLE_WIRTE_NAND;
	dbg("disable write nand at first\n");
	if( msgsnd( daemon_msg_queue, &msg, sizeof(msg)-sizeof(long), IPC_NOWAIT ) < 0 ) {
		perror("msgsnd");
	}
	return;
}

void v2ipd_request_timeover_protect()
{
	vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_REQUEST_TIMEOVER_PROTECT;
	if( msgsnd( daemon_msg_queue, &msg, sizeof(msg)-sizeof(long), IPC_NOWAIT ) < 0 ) {
		perror("msgsnd");
	}
	return;
}

void v2ipd_stop_timeover_protect()
{
	vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_STOP_TIMEOVER_PROTECT;
	if( msgsnd( daemon_msg_queue, &msg, sizeof(msg)-sizeof(long), IPC_NOWAIT ) < 0 ) {
		perror("msgsnd");
	}
	return;
}

static inline int kill_tcp_connection(struct sess_ctx *sess)
{
	//dbg("called\n");
	shutdown(sess->s2, 1); /* Notify client that we are done writing */
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
	// dbg("called\n");
#if 0
	return vpu_ExitSession();
#else
	printf("should do something, halt at 1\n");
	while(1);
#endif
	free_system_session(sess);

}

static inline int kill_connection(struct sess_ctx *sess)
{
	//kill_video(sess);
	struct sess_ctx * tmp;
	sess->connected = 0;
	sess->playing = 0;
	sess->paused = 0;
	sess->motion_detected = 0;

	if (sess->is_tcp) {
		//kill_tcp_connection(sess);
	} else if (sess->is_file) {
		if (sess->file_fd >= 0) {
			close(sess->file_fd);
			sess->file_fd = -1;
		}
		sess->is_file = 0;
	} else ;
	pthread_mutex_lock(&global_ctx_lock);
	tmp=global_ctx_running_list;
	if(tmp=sess){
		global_ctx_running_list=global_ctx_running_list->next;
		currconnections--;
	}else{
		while(tmp->next!=NULL){
			if(tmp->next==sess){
				tmp->next=sess->next;
				currconnections--;
				break;
			}
			tmp=tmp->next;
		}
	}
	pthread_mutex_unlock(&global_ctx_lock);
	// dbg("called");
	return kill_video(sess);
}

/**
 * init_video - create and init video driver
 * @config: video driver configuration file
 * Returns driver context 0 on success or NULL on error
 */
static inline void *init_video(char *config)
{
	return 0;
}

/**
 * deinit_video - deinit video driver
 * @sess: session context
 * Returns 0 on success or -1 on error
 */
static inline int deinit_video(struct sess_ctx *sess)
{
	return 0;
}

/**
 * add_video_handler - install video handler
 * @sess: session context
 * @handler: data handler
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static inline int add_video_handler(struct sess_ctx *sess, void *handler,
									void *arg)
{
#if 0
	return vpu_AddCallback(NULL, handler);
#else
	return 0;
#endif
}

/**
 * remove_video_handler - remove video handler
 * @sess: session context
 * @handler: data handler
 * Returns 0 on success or -1 on error
 */
static inline int remove_video_handler(struct sess_ctx *sess,
									   void *handler)
{
	return vpu_RemoveCallback(NULL, handler);
}


/**
 * init_audio - create and init audio driver
 * @config: audio driver configuration file
 * Returns driver context 0 on success or NULL on error
 */
static inline void *init_audio(char *config)
{
	dbg("driver not installed");
	return NULL;
}

/**
 * deinit_audio - deinit audio driver
 * @sess: session context
 * Returns 0 on success or -1 on error
 */
static int deinit_audio(struct sess_ctx *sess)
{
	dbg("driver not installed");
	return -1;
}

/**
 * free_session - cleans up session parameters.
 * @sess: session context
 * Returns 0 on success or -1 on error
 */
int free_system_session(struct sess_ctx *sess)
{
	if (sess == NULL) return -1;
	//dbg("##############sess->id=%d################\n",sess->id);
	printf("free_system_session now\n");
	if (sess->name != NULL)
		free(sess->name);
	if (sess->video.fp != NULL){
		printf("fclose(sess->video.fp)\n");
		fclose(sess->video.fp);
	}
	if (sess->audio.fp != NULL){
		fclose(sess->audio.fp);
		printf("fclose(sess->audio.fp)\n");
	}
	if (sess->video.params != NULL){
		printf("free_video_conf\n");
		free_video_conf(sess->video.params);
	}
	if (sess->audio.params != NULL){
		printf("free_audio_conf\n");
		free_audio_conf(sess->audio.params);
	}
	if (sess->audio.cfgfile != NULL){
		printf("free(sess->audio.cfgfile)\n");
		free(sess->audio.cfgfile);
	}
	if (sess->video.cfgfile != NULL){
		printf("free(sess->video.cfgfile)\n");
		free(sess->video.cfgfile);
	}
	if (sess->video.sess != NULL)
		deinit_video(sess);
	if (sess->audio.sess != NULL)
		deinit_audio(sess);
	if (sess->s1 >= 0){
		printf("close sess->s1 ==%d\n",sess->s1);
		close(sess->s1);
	}
	if (sess->s2 >= 0){
		printf("close sess->s2==%d\n",sess->s2);
		close(sess->s2);
	}
	if (sess->to != NULL)
		free(sess->to);
	if (sess->myaddr != NULL)
		free(sess->myaddr);
	if (sess->pipe_fd >= 0){
		printf("close sess->pipe_fd ==%d\n",sess->pipe_fd);
		close(sess->pipe_fd);
	}
	if (sess->pipe_name != NULL) {
		printf("free sess->pipe_name\n");
		unlink(sess->pipe_name);
		free(sess->pipe_name);
	}
	if (sess->file_fd >= 0){
		printf("close sess->file_fd ==%d\n",sess->file_fd);
		close(sess->file_fd);
	}

#ifdef VIDEO_STATS
	if (sess->video.tv != NULL)
		free(sess->video.tv);
#endif /* VIDEO_STATS */
#ifdef AUDIO_STATS
	if (sess->audio.tv != NULL)
		free(sess->audio.tv);
#endif /* AUDIO_STATS */

#ifdef USE_CLI
	if (sess->cli_sess != NULL){
		printf("close cli_deinit\n");
		cli_deinit(sess->cli_sess);
	}
#endif /* USE_CLI */
	if(sess->id>=0)
		put_sess_id(sess->id);
	pthread_mutex_destroy(&sess->sesslock);
	free(sess);

	//dbg("sid(%08X) removed successfully\n", (u32) sess);
	return 0;
}

/**
 * new_session - create and initialize session.
 * @name: ACSII based session name
 * Returns session context on success or NULL on error
 */
struct sess_ctx *new_system_session(char *name) {
	struct sess_ctx *sess = NULL;

	if (name == NULL) {
		perror("Error creating new session");
		return NULL;
	}
	/* create session context */
	sess = malloc(sizeof(*sess));
	if (sess == NULL)
		return NULL;
	memset(sess, 0, sizeof(struct sess_ctx));
	sess->id = get_sess_id();
	//printf("###############get sess->id = %d####################\n",sess->id);
	if(sess->id<0){
		printf("****************can't get session id*******************\n");
		free(sess);
		return NULL;
	}
	sess->s1 = -1;
	sess->s2 = -1;
	sess->pipe_fd =-1;
	sess->file_fd =-1;

	
	/* Add name of session */
	sess->name = malloc(strlen(name) + 1); /* +1 for string termination */
	if (sess->name == NULL)
		goto error1;
	memset(sess->name, 0, strlen(name)+1);
	memcpy(sess->name, name, strlen(name));

	/* Add timestamp stuff */
#ifdef VIDEO_STATS
	sess->video.tv = create_timeval(); /* usec timing */
#endif /* VIDEO_STATS */

#ifdef AUDIO_STATS
	sess->audio.tv = create_timeval();
#endif /* AUDIO_STATS */
      pthread_mutex_init(&sess->sesslock,NULL);

	return sess;

error1:
	put_sess_id(sess->id);
	free(sess);

	return NULL;
}

/**
 * send_payload - sends formatted payload through socket layer.
 * @sess: session id
 * @payload: payload data
 * @len: length of payload
 * Returns number of bytes sent on success or 0 on error
 */
int send_payload(struct sess_ctx *sess, u8 *payload, size_t len)
{
	//nand_write(payload, len);
	if (sess->paused) return 0;

	if (sess->is_pipe && sess->pipe_fd > 0)
		return write(sess->pipe_fd, payload, len);
	if (sess->is_file && sess->file_fd > 0) {
		return write(sess->file_fd, payload, len);
	} else if (sess->is_tcp && sess->connected && sess->s2 >= 0)
		return send(sess->s2, (void *) payload, len, 0);
	else if (sess->is_udp && sess->s1 >= 0)
		return sendto(sess->s1, (void *) payload, len, 0,
					  (struct sockaddr *) sess->to, sizeof(*sess->to));
	else {
		dbg("error");
		return 0;
	}
}

/**
 * process_video_payload - process video payload to forward to server
 * @sess: session id
 * @payload: payload data
 * @len: length of payload
 * Returns number of bytes sent on success or 0 on error
 */
static int stop_vid(struct sess_ctx *sess, char *arg);
int process_video_payload(struct sess_ctx *sess, u8 *payload, size_t len)
{
#if 0
	struct sess_ctx *s = global_ctx;
	/* Perform any data processing if required */
#ifdef VIDEO_STATS
	u32 delta = update_timeval(s->video.tv);
	s->video.curr_frame_idx++;
	dbg("time video between packets (%d) usecs", delta);
#endif /* VIDEO_STATS */

	/* This an FTF hack to trigger motion detection */
	if (sess == NULL && payload == NULL) {
		s->motion_detected = (len == IPL_MOTION) ? 1 : 0;
		//printf("motion detected %s\n", len == IPL_MOTION ? "yes" : "no");
		return 0;
	}

	/* This is an ESC demo fix only */
#define MAX_FILE_SZ (1024 * 1024 * 30) /* 30 MB */
	if (s->is_file && s->video.nbytes > MAX_FILE_SZ) {
		return stop_vid(s, NULL);
	} else {
		s->video.nbytes += len;
		return send_payload(s, payload, len);
	}
#endif
	return 0;
}

/**
 * process_audio_payload - process audio payload and forward to server
 * @sess: session id
 * @payload: payload data
 * @len: length of payload
 * Returns number of bytes sent on success or 0 on error
 */
#ifdef AUDIO_SUPPORTED
static int process_audio_payload(struct sess_ctx *sess, u8 *payload,
								 size_t len)
{
	/* Perform any data processing if required */
	struct sess_ctx *s = global_ctx;
#ifdef AUDIO_STATS
	u32 delta = update_timeval(sess->audio.tv);
	sess->audio.curr_frame_idx++;
	sess->audio.nbytes += len;
	dbg("time video audio packets (%d) usecs", delta);
#endif /* AUDIO_STATS */
	return send_payload(s, payload, len);
}
#endif /* SUPPORT_AUDIO */

/**
 * handle_session - handles data transmission
 * @sess: session context
 */
static void handle_session(struct sess_ctx *sess)
{
	usleep(1000); /* microseconds */
}

/**
 * daemonise - force program to run as a background process
 * @pid_file: file that contains PID (process id)
 */
static void daemonise(const char *pid_file)
{
	pid_t pid, sid;

	/* already a daemon */
	if (getppid() == 1) return;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		printf("fork error\n");
		exit(EXIT_FAILURE);
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		printf("setsid error\n");
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory.  This prevents
	 * the current directory from being locked; hence not
	 * being able to remove it. */
	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
	}

	if (pid_file != NULL) {
		FILE *f = fopen(PID_FILE, "w");
		if (f != NULL) {
//			fprintf(f, "%u\n", getpid()); /* Save pid to file */
			fclose(f);
		}
	}

	/* Redirect standard files to /dev/null */
//        freopen("/dev/null", "r", stdin);
//       freopen("/dev/null", "w", stdout);
//       freopen("/dev/null", "w", stderr);
}

/**
 * usage - displays various program options
 * @name: parameter name
 */
static void usage(char *name)
{
	printf("Usage: %s [-v <config>] [-a <config>] [-F] [-C]\n"
		   "\t-v    video configuration file\n"
		   "\t-a    audio configuration file\n"
		   "\t-C    start up cli\n"
		   "\t-F    do not daemonize\n", name);
}

void handle_sig(int signo)
{
	printf("%s, sig num:%d\n", __func__, signo);
}

/**
 * do_server - main server loop
 * @sess: session context
 */
static int do_server(struct sess_ctx *sess)
{
	socklen_t fromlen;
	int ret;
	struct sockaddr_in address;
	int socket;
	playback_t* pb;

	dbg("Starting V2IP server");

	/* Spin */
	sess->running = 1;
	sess->soft_reset = 0; /* Clear soft reset condition */

	playback_init();
//	monitor_init();
	//while (sess->running && !sess->soft_reset) {
	while(1) {
		if (sess->is_tcp) {
			fromlen = sizeof(struct sockaddr_in);
			socket = accept(
						 sess->s1,
						 (struct sockaddr *) &address,
						 &fromlen);
			printf("connection in, addr=0x%x, port=0x%d\n",
				   __func__, address.sin_addr.s_addr, address.sin_port);
			if(socket >= 0) {
				int ret = playback_connect(address, socket);
				printf("%s: ret = %d\n", __func__, ret);
				//conncect for playback firstly. if it fails, connect for monitor.
				if(ret < 0) {
//					monitor_new(socket, address);
					if(!sess->connected) {
						sess->connected = 1;
						if (start_vid(sess, NULL) < 0) {
							dbg("error starting video");
						}
					}
				}
			}
		}
		handle_session(sess);
	}

	/* Take down session */
	if (sess->soft_reset)
		ret = kill_connection(sess);
	else{
		//ret = vpu_ExitSession();
		ret = 0;
	}

	printf ("end/exit returned=%d\n", ret);

	dbg("Exitting server");

	return 0;
}

static inline int kill_server(struct sess_ctx *sess)
{
	dbg("called");
	sess->running = 0;
	sess->soft_reset = 0; /* This is a hard reset */
	return 0;
}

/**
 * sig_handler - sets flag to exit program
 * @signum: signal number
 */
static void sig_handler(int signum)
{
	struct sess_ctx *s = global_ctx;

	dbg("get a signal: %d\n", signum);

	if (signum == SIGINT || signum == SIGIO) {
		exit(-1);
//		kill_server(s);
	} else if (signum == SIGPIPE) {
		dbg("SIGPIPE\n");
		kill_connection(s);
	} else ;
}

//if some unrecoverable system error is occured, use it to reset whole system
void force_reset_v2ipd()
{
	global_ctx->soft_reset = 1;
}

/**
 * main - program entry
 * @argc: number of arguments
 * @argv: argument vectors
 * Returns 1 on successful program exit or 0 on error
 */
int monitor_main(int argc, char **argv)
{
	int val;
	int i;
	int err = 0;
	char *video_conf = NULL;
	char *audio_conf = NULL;
	int daemon = 1; /* Default */
	struct sigaction sa;
	struct sess_ctx *sess = NULL;
	static int sess_idx = 0;
	char name[80];
	int start_cli = 0;
	struct sched_param  mysched;
	char* argv_local[]= {
//		"v2ipd","-C","-v","/etc/v2ipd/video.cfg",
		"v2ipd","-C","-v","/tmp/video.cfg",
	};

	argc = 4;
	argv = argv_local;

	if (argc == 1) {
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	while (1) {
		if ((val = getopt(argc, argv, ":v:a:FC")) < 0)
			break; /* Check next arg */

		switch (val) {
		case 'v': /* video driver config */
			if (optarg != NULL)
				video_conf = strdup(optarg);
			break;
		case 'a': /* audio driver config */
			if (optarg != NULL)
				audio_conf = strdup(optarg);
			break;
		case 'F':
			daemon = 0;
			break;
		case 'C':
			start_cli = 1;
			break;
		case '?':
			if (isprint(optopt))
				printf("Unknown option -%c\n",
					   optopt);
			else
				printf("Unknown option %x\n",
					   optopt);
			usage(argv[0]);
			return -1;
		}
	}
//        dbg("v2ipd logfile:  %s, %s\n", __DATE__, __TIME__);
//	dbg("\n\n\nStarting V2IPd (Voice/Video over IP) client/server daemon");

	/* Make this a real time process */
	mysched.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
	if (sched_setscheduler(0, SCHED_FIFO, &mysched) == -1) {
		printf("%s: Error changing priority\n", __func__);
	}
//        printf("%s: New priority is %d\n", __func__, mysched.sched_priority);


	if (daemon) {
//                dbg("daemonising");
		daemonise(PID_FILE);
	}


	/* Set up signals */
	for (i = 1; i <= _NSIG; i++) {
		if (i == SIGIO || i == SIGINT)
			sigset(i, sig_handler);
		else
			sigignore(i);
	}

	/* Set up exit handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &sig_handler;
	sigaction(SIGINT | SIGPIPE, &sa, NULL);

	/* Set up session */
	memset(name, 0, sizeof(name));
	sprintf(name, "%s-%d", "Session", ++sess_idx);
	sess = new_system_session(name);
	if (sess == NULL) {
		printf("Error starting session - exitting\n");
		err = EXIT_FAILURE;
		goto done;
	}

	sess->daemon = daemon;
	global_ctx = sess;

//        printf("mypid %d\n", getpid());

#ifdef USE_CLI
	/* Start up the CLI */
#if 0
	if (start_cli) {
		cli_cmd_handler.cmds = (char *) cli_cmds;
		cli_cmd_handler.handler = do_cli_cmd;
		cli_cmd_handler.handler_bin = do_cli_cmd_bin;
		/* Create CLI session */
		if ((sess->cli_sess = cli_init(sess)) != NULL) {
			// dbg("created cli_sess %08X",
			//         (u32) sess->cli_sess);
			/* Install handlers */
			if (cli_bind_cmds(sess->cli_sess,
							  &cli_cmd_handler) < 0) {
				perror("error installing CLI handlers");
				err = EXIT_FAILURE;
				goto done;
			}
			//  dbg("handlers bound to CLI");
		} else {
			perror("error initializing CLI");
			err = EXIT_FAILURE;
			goto done;
		}
	}
#endif
#endif /* USE_CLI */

	PTZ();

	/* Add video ES handler */
	if (add_video_handler(sess, process_video_payload, NULL) < 0) {
		printf("%s error installing video callback - \
                                exitting\n", __func__);
		goto done;
	}

	if(( daemon_msg_queue = msgget( DAEMON_MESSAGE_QUEUE_KEY, 0 )) < 0 ) {
		perror("msgget: open");
		return -1;
	}

	if(( v2ipd_shm_id = shmget( V2IPD_SHARE_MEM_KEY, V2IPD_SHARE_MEM_SIZE, IPC_CREAT )) < 0 ) {
		perror("shmget in v2ipd");
		return -1;
	}
	if((v2ipd_share_mem = (vs_share_mem*)shmat(v2ipd_shm_id, 0, 0)) < ( vs_share_mem* ) 0 ) {
		perror("shmat in v2ipd");
		return -1;
	}

	/* Load config files */
	do {
		/* Load config files */
		if (audio_conf != NULL) {
			free_audio_conf(sess->audio.params);
			if (sess->audio.cfgfile == NULL)
				sess->audio.cfgfile = strdup(audio_conf);
			sess->video.params =
				parse_video_conf(sess->video.cfgfile);
		}

		if (video_conf != NULL) {
			/* Reload config */
			if (video_params_changed(sess->video.params))
				save_video_conf(sess->video.params,
								sess->video.cfgfile);

			free_video_conf(sess->video.params);
			if (sess->video.cfgfile == NULL)
				sess->video.cfgfile = strdup(video_conf);
			sess->video.params =
				parse_video_conf(sess->video.cfgfile);
		}

		/* Main loop - does not retun unless forced */
		err = do_server(sess);

		//      } while (sess->soft_reset);
	} while (!sess->soft_reset);

	if( sess->soft_reset ) {
		v2ipd_restart_all();
	}

done:
	shmdt( v2ipd_shm_id );
//	msgctl(msg_queue_id, IPC_RMID, NULL);
	free(video_conf);
	free(audio_conf);
	free_system_session(sess);
	dbg("Exitting program\n\n\n\n");

	exit(err);
}

#if 0
static char* test_jpeg_file="/720_480.jpg";
static char* get_data(int* size)
{
	int ret;
	int i;
	int fd;
	struct stat st;
	char* buf;

	fd = open(test_jpeg_file, O_RDONLY);
	if(fd==-1) {
		printf("can not open input file\n");
		return 0;
	}

	if (stat(test_jpeg_file, &st) != 0) {
		return 0;
	}
	buf = malloc(st.st_size);
	if( !buf ) {
		printf("malloc buf error\n");
		return 0;
	}
	read( fd, buf, st.st_size );
	close( fd );
	*size = st.st_size;
	return buf;
}
#else
struct vdIn * vdin_camera=NULL;
//we need tow functions to get data beacause we wo only allow one thread get data from kernal buffers that is function get_data
 char * get_video_data(int *size){
	int i;
	int canuse=-1;
	char *buff;
	pthread_t self_tid;
	self_tid=pthread_self();
try_again:
	pthread_mutex_lock(&vdin_camera->tmpbufflock);
	for(i=0;i<MAX_CONNECTIONS;i++){
		if(vdin_camera->hrb_tid[i]==0)
			canuse=i;
		else if(vdin_camera->hrb_tid[i]==self_tid){
			//printf("tmpbuffer have read last time!\n");
			//printf("hrb_tid[%d]=%lu,self_tid=%lu\n",i,vdin_camera->hrb_tid[i],self_tid);
			goto have_readed;
		}
	}
	buff=malloc(vdin_camera->buf.bytesused+DHT_SIZE + sizeof(picture_info_t));
	if(!buff){
		printf("malloc buf error\n");
		pthread_mutex_unlock(&vdin_camera->tmpbufflock);
		*size=0;
		return 0;
	}
	memcpy(buff,vdin_camera->tmpbuffer,vdin_camera->buf.bytesused+DHT_SIZE + sizeof(picture_info_t));
	vdin_camera->hrb_tid[canuse]=self_tid;
	*size=vdin_camera->buf.bytesused+DHT_SIZE + sizeof(picture_info_t);
	pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	//printf("read tmpbuffer sucess size=%d\n",*size);
	return buff;
have_readed:
	pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	usleep(20000);
	goto try_again;
}
static char* get_data(int* size,int *width,int *height)
{
	const char *videodevice = "/dev/video0";
	int format = V4L2_PIX_FMT_MJPEG;
	int grabmethod = 1;
	 char* buf;
	 int trygrab = 5;
	
	pthread_mutex_lock(&vdin_camera->tmpbufflock);
retry:
	if (uvcGrab (vdin_camera) < 0) {
		printf("Error grabbing\n");
		trygrab --;
		if(trygrab<=0){
			close_v4l2 (vdin_camera);
			if (init_videoIn(vdin_camera, (char *) videodevice, 640, 480, format, grabmethod) < 0) {
				printf("init camera device error\n");
				exit(0);
			}
		}
		usleep(100000);
		goto retry;
	}
	//pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	buf = malloc(vdin_camera->buf.bytesused + DHT_SIZE);
	if( !buf ) {
		printf("malloc buf error\n");
		*size=0;
		pthread_mutex_unlock(&vdin_camera->tmpbufflock);
		return 0;
	}
	//pthread_mutex_lock(&vdin_camera->tmpbufflock);
	memcpy(buf, vdin_camera->tmpbuffer + sizeof(picture_info_t), vdin_camera->buf.bytesused + DHT_SIZE );
	*size = vdin_camera->buf.bytesused + DHT_SIZE;
	*width = vdin_camera->width;
	*height = vdin_camera->height;
	pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	return buf;
}
#endif

/*智能监控模式
*如果第一个是通过内网进来的则切成VGA
*如果第一个是通过外网进来的则切成QVGA
*如果不是第一个进来的，则不能切换
*如果退出的是最后一个连接的会话则恢复原来分辨率
*/
void restart_v4l2(int width , int height );
void change_camera_status(struct sess_ctx *sess , int sess_in)
{
	if(strncmp(threadcfg.monitor_mode , "inteligent",10)!=0)
		return;
	if(sess_in){
		if(sess->is_tcp){			
			if(strncmp(threadcfg.record_resolution,"vga",3)!=0){
				memset(threadcfg.record_resolution , 0 ,sizeof(threadcfg.record_resolution));
				sprintf(threadcfg.record_resolution , "vga");
				memcpy(threadcfg.resolution ,threadcfg.record_resolution , sizeof(threadcfg.resolution));
				restart_v4l2(640, 480);
			}
		}else if(sess->is_rtp){
			if(strncmp(threadcfg.record_resolution,"qvga",4)!=0){
				memset(threadcfg.record_resolution , 0 ,sizeof(threadcfg.record_resolution));
				sprintf(threadcfg.record_resolution , "qvga");
				memcpy(threadcfg.resolution ,threadcfg.record_resolution , sizeof(threadcfg.resolution));
				restart_v4l2(320, 240);
			}
		}
	}else{
		if(strncmp(threadcfg.record_resolution,threadcfg.original_resolution,strlen(threadcfg.original_resolution))!=0){
			memcpy(threadcfg.record_resolution , threadcfg.original_resolution , sizeof(threadcfg.original_resolution));
			memcpy(threadcfg.resolution , threadcfg.original_resolution , sizeof(threadcfg.original_resolution));
			if(strncmp(threadcfg.original_resolution,"vga",3) ==0)
				restart_v4l2(640,480);
			else
				restart_v4l2(320,240);
		}
	}
}
 void  add_sess(struct sess_ctx *sess)
{
	pthread_mutex_lock(&global_ctx_lock);
	sess->next = global_ctx_running_list;
	global_ctx_running_list = sess;
	currconnections ++;
	if(currconnections == 1)
		change_camera_status( sess, 1);
	pthread_mutex_unlock(&global_ctx_lock);
}
void  del_sess(struct sess_ctx *sess)
{
	struct sess_ctx **p;
	pthread_mutex_lock(&global_ctx_lock);
	p = &global_ctx_running_list;
	while(*p!=NULL){
		if((*p)==sess){
			*p=(*p)->next;
			currconnections--;
			if(g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
			if(currconnections == 0)
				change_camera_status(sess, 0);
			break;
		}
		p=&((*p)->next);
	}
	pthread_mutex_unlock(&global_ctx_lock);
}

 void take_sess_up(struct sess_ctx *sess)
{
	pthread_mutex_lock(&sess->sesslock);
	sess->running = 1;
	sess->soft_reset = 0; /* Clear soft reset condition */
	pthread_mutex_unlock(&sess->sesslock);
}

 void take_sess_down(struct sess_ctx *sess)
{
	pthread_mutex_lock(&sess->sesslock);
	sess->ucount--;
	sess->running=0;
	if(sess->ucount<=0){
		pthread_mutex_unlock(&sess->sesslock);
		free_system_session(sess);
	}else
		pthread_mutex_unlock(&sess->sesslock);
}

#define UPDATE_FILE_HEAD_SIZE   11

 int do_net_update(void *arg){
 	struct sess_ctx *sess = (struct sess_ctx *)arg;
	socklen_t fromlen;
	int ret;
	int socket = -1;
	//char* buffer;
	//int size;
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

	add_sess( sess);
	take_sess_up( sess);
	if(is_do_update()){
		dbg("tcp do update some one is doing update\n");
		goto exit;
	}
	socket = sess->sc->video_socket;
	printf("###################do update###################\n");
	if(sess->is_tcp)
		r = recv(socket , buf,1024,0);
	else
		r = udt_recv(socket ,SOCK_STREAM ,buf ,1024,NULL,NULL);
	if(r!=1024){
		printf(" do update recv data too small\n");
		goto exit;
	}
	   p=buf;
	   cmd = *p;
	   p++;
	   memcpy(&__system_f_size , p , sizeof(__system_f_size));
	   p+=sizeof(__system_f_size);
	   memcpy(&__kernal_f_size , p,sizeof(__kernal_f_size));
	   p+=sizeof(__kernal_f_size);
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
	   switch(cmd)
	   {
	   	case 0:
			printf ("####################update system################\n");
			sfp = fopen(SYSTEM_UPDATE_FILE , "w");
			if(!sfp){
				printf("*******************open system update file error**********\n");
				goto exit;
			}
			system_f_size  = (1024-(p-buf));
			scrc = checksum(scrc, (unsigned char *)p, system_f_size);
			if(fwrite(p,1,system_f_size,sfp)!=system_f_size){
				printf("**********error write system.update************\n");
				goto exit;
			}
			while(system_f_size <__system_f_size){
				if(sess->is_tcp)
					r = recv(socket , buf,1024,0);
				else
					r = udt_recv(socket , SOCK_STREAM , buf , 1024 ,NULL , NULL);
				 if(r<=0){
				 	herror(" do update recvmsg error");
		 			 goto exit;
				 }
				system_f_size +=r;
				//printf("recv data=%d curr size =%d expected size = %d\n",r ,system_f_size , __system_f_size);
				scrc = checksum(scrc, (unsigned char *)buf, r);
				if((unsigned int )r!=fwrite(buf,1,r,sfp)){
					printf("**********error write system.update************\n");
					goto exit;
				}
			}
			if(system_f_size!=__system_f_size){
				printf("do system update error we expect file size %d but we recv %d\n",__system_f_size , system_f_size);
				goto exit;
			}
			if(scrc!=__scrc){
				printf("****************check sum error we expected %2x but the results is %2x**********\n",__scrc ,scrc);
				goto exit;
			}
			fclose(sfp);
			sfp = NULL;
			prepare_do_update();
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd1",SYSTEM_UPDATE_FILE);
			system(buf);
			system("reboot &");
			exit(0);
			break;
		case 1:
			printf("#####################update kernal################\n");
			kfp = fopen(KERNAL_UPDATE_FILE , "w");
			if(!kfp){
				printf("*******************open kernal update file error**********\n");
				goto exit;
			}
			kernal_f_size  = (1024-(p-buf));
			kcrc = checksum(kcrc, (unsigned char *)p, kernal_f_size);
			if(fwrite(p,1,kernal_f_size,kfp)!=kernal_f_size){
				printf("**********error write kernal.update************\n");
				goto exit;
			}
			while(kernal_f_size <__kernal_f_size){
				if(sess->is_tcp)
					r = recv(socket , buf,1024,0);
				else
					r = udt_recv(socket , SOCK_STREAM , buf , 1024 ,NULL , NULL);
				 if(r<=0){
				 	herror("tcp do update recvmsg error");
		 			 goto exit;
				 }
				kernal_f_size +=r;
				//printf("recv data=%d curr size =%d expected size = %d\n",r ,kernal_f_size , __kernal_f_size);
				kcrc = checksum(kcrc, (unsigned char *)buf , r);
				if((unsigned int)r!=fwrite(buf,1,r,kfp)){
					printf("**********error write kernal.update************\n");
					goto exit;
				}
			}
			if(kernal_f_size!=__kernal_f_size){
				printf("do kernal update error we expect file size %d but we recv %d\n",__kernal_f_size , kernal_f_size);
				goto exit;
			}
			if(kcrc!=__kcrc){
				printf("***************kernel sum error we expected %2x but the result is %2x************\n",__kcrc , kcrc);
				goto exit;
			}
			fclose(kfp);
			kfp = NULL;
			prepare_do_update();
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd0",KERNAL_UPDATE_FILE);
			system(buf);
			system("reboot &");
			exit(0);
			break;
		case 2:
			printf("###############update system and kernal################\n");
			sfp = fopen(SYSTEM_UPDATE_FILE , "w");
			if(!sfp){
				printf("*******************open system update file error**********\n");
				goto exit;
			}
			kfp = fopen(KERNAL_UPDATE_FILE , "w");
			if(!kfp){
				printf("*******************open kernal update file error**********\n");
				goto exit;
			}
			system_f_size  = (1024-(p-buf));
			scrc = checksum(scrc, (unsigned char *)p, system_f_size);
			if(fwrite(p,1,system_f_size,sfp)!=system_f_size){
				printf("**********error write system.update************\n");
				goto exit;
			}
			p=NULL;
			while(system_f_size <__system_f_size){
				if(sess->is_tcp)
					r = recv(socket , buf,1024,0);
				else
					r = udt_recv(socket , SOCK_STREAM , buf , 1024 ,NULL , NULL);
				 if(r<=0){
				 	herror("tcp do update recvmsg error");
		 			 goto exit;
				 }
				 if(__system_f_size - system_f_size<(unsigned int)r){
				 	scrc = checksum(scrc, (unsigned char *)buf, __system_f_size - system_f_size);
				 	if(fwrite(buf,1,__system_f_size - system_f_size , sfp)!=(__system_f_size - system_f_size)){
						printf("**********error write system.update************\n");
						goto exit;
				 	}
					p= buf;
					p+=(__system_f_size - system_f_size);
					system_f_size = __system_f_size;
					//printf("r = %d , system file size = %u we recv system update file ok\n",r , __system_f_size);
					break;
				 }
				system_f_size +=r;
				//printf("recv data=%d curr size =%u expected system file size = %u\n",r ,system_f_size , __system_f_size);
				scrc = checksum(scrc, (unsigned char *)buf, r);
				if((unsigned int )r!=fwrite(buf,1,r,sfp)){
					printf("**********error write system.update************\n");
					goto exit;
				}
			}
			if(system_f_size!=__system_f_size){
				printf("do system update error we expected file size %d but we recv %d\n",__system_f_size , system_f_size);
				goto exit;
			}
			kernal_f_size = 0;
			if(p){
				kernal_f_size =r-(p-buf);
				kcrc = checksum(kcrc, (unsigned char *)p, kernal_f_size);
				if(fwrite(p,1,kernal_f_size,kfp)!=kernal_f_size){
					printf("************error write kernal.updtea*************\n");
					goto exit;
				}
			}
			while(kernal_f_size <__kernal_f_size){
				if(sess->is_tcp)
					r = recv(socket , buf,1024,0);
				else
					r = udt_recv(socket , SOCK_STREAM , buf , 1024 ,NULL , NULL);
				 if(r<=0){
				 	herror("tcp do update recvmsg error:%s\n");
		 			 goto exit;
				 }
				kernal_f_size +=r;
				//printf("recv data=%d curr size =%d expected kernel file size = %d\n",r ,kernal_f_size , __kernal_f_size);
				kcrc = checksum(kcrc, (unsigned char *)buf , r);
				if((unsigned int)r!=fwrite(buf,1,r,kfp)){
					printf("**********error write kernal.update************\n");
					goto exit;
				}
			}
			if(kernal_f_size!=__kernal_f_size){
				printf("do kernal update error we expect file size %d but we recv %d\n",__kernal_f_size , kernal_f_size);
				goto exit;
			}
			if(scrc!=__scrc||kcrc!=__kcrc){
				printf("check sum error we expected system sum %2x  but the reslut is %2x we expected kernel sum %2x but the result is %2x\n",__scrc ,scrc,__kcrc , kcrc);
				goto exit;
			}
			fclose(sfp);
			fclose(kfp);
			
			prepare_do_update();
			printf("####################ok try update kernal###################\n");
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd0",KERNAL_UPDATE_FILE);
			system(buf);
			sleep(1);
			printf("####################update kernal ok try update system##############\n");
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd1",SYSTEM_UPDATE_FILE);
			system(buf);
			system("reboot &");
			exit(0);
			break;
		default:
			printf("*****************get unkown cmd*****************************\n");
			goto exit;
	   }
exit:
	if(reboot_flag){
		system("reboot &");
		exit(0);
	}
	if(sfp){
		system("reboot &");
		exit(0);
	}
	if(kfp){
		system("reboot &");
		exit(0);	
	}
	if(socket>=0){
		system("reboot &");
		exit(0);
	}
	del_sess(sess);
	take_sess_down( sess);
	return 0;
 }

/*
int tcp_do_update(void *arg){
 	struct sess_ctx *sess = (struct sess_ctx *)arg;
	socklen_t fromlen;
	int ret;
	struct sockaddr_in address;
	int socket = -1;
	//char* buffer;
	//int size;
	int tryaccpet = MAX_CONNECTIONS;
	struct timeval timeout;
	struct timeval selecttv;
	fd_set acceptfds;
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

	add_sess( sess);
	take_sess_up( sess);
	if(is_do_update()){
		dbg("tcp do update some one is doing update\n");
		goto exit;
	}
	selecttv.tv_sec = 5;
	selecttv.tv_usec = 0;
	printf("###############tcp do update ready to connect#####################\n");
__tryaccept:
	fromlen = sizeof(struct sockaddr_in);
	FD_ZERO(&acceptfds);
	FD_SET(sess->s1, &acceptfds);
	do{
		ret = select(sess->s1 + 1, &acceptfds, NULL, NULL, &selecttv);
	}while(ret == -1);
	if(ret == 0){
		tryaccpet --;
		if(tryaccpet<=0){
			close(sess->s1);
			sess->s1 = -1;
			goto exit;
		}
		goto __tryaccept;
	}
	
	socket = accept(sess->s1,(struct sockaddr *) &address,&fromlen);
	if(socket<0){
		printf("accept erro!\n");
		close(sess->s1);
		sess->s1=-1;
		goto exit;
	}
	if(sess->from.sin_addr.s_addr!=address.sin_addr.s_addr){
		close(socket);
		tryaccpet --;
		if(tryaccpet <=0){
			socket = -1;
			goto exit;
		}
		goto __tryaccept;
	}
	if(socket>=0){
		printf("##################connect ok###################\n");
		close(sess->s1);
		sess->s1 = -1;
		timeout.tv_sec  = 10;
		timeout.tv_usec = 0;
		 if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
		            printf("Error enabling socket rcv time out \n");
		  }
		r = recv(socket , buf,1024,0);
		if(r!=1024){
			printf("tcp do update recv data too small\n");
			goto exit;
		}
	   p=buf;
	   cmd = *p;
	   p++;
	   memcpy(&__system_f_size , p , sizeof(__system_f_size));
	   p+=sizeof(__system_f_size);
	   memcpy(&__kernal_f_size , p,sizeof(__kernal_f_size));
	   p+=sizeof(__kernal_f_size);
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
	   switch(cmd)
	   {
	   	case 0:
			printf ("####################update system################\n");
			sfp = fopen(SYSTEM_UPDATE_FILE , "w");
			if(!sfp){
				printf("*******************open system update file error**********\n");
				goto exit;
			}
			system_f_size  = (1024-(p-buf));
			scrc = checksum(scrc, (unsigned char *)p, system_f_size);
			if(fwrite(p,1,system_f_size,sfp)!=system_f_size){
				printf("**********error write system.update************\n");
				goto exit;
			}
			while(system_f_size <__system_f_size){
				r = recv(socket , buf,1024,0);
				 if(r<=0){
				 	herror("tcp do update recvmsg error");
		 			 goto exit;
				 }
				system_f_size +=r;
				//printf("recv data=%d curr size =%d expected size = %d\n",r ,system_f_size , __system_f_size);
				scrc = checksum(scrc, (unsigned char *)buf, r);
				if((unsigned int )r!=fwrite(buf,1,r,sfp)){
					printf("**********error write system.update************\n");
					goto exit;
				}
			}
			if(system_f_size!=__system_f_size){
				printf("do system update error we expect file size %d but we recv %d\n",__system_f_size , system_f_size);
				goto exit;
			}
			if(scrc!=__scrc){
				printf("****************check sum error we expected %2x but the results is %2x**********\n",__scrc ,scrc);
				goto exit;
			}
			fclose(sfp);
			sfp = NULL;
			prepare_do_update();
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd1",SYSTEM_UPDATE_FILE);
			system(buf);
			system("reboot &");
			exit(0);
			break;
		case 1:
			printf("#####################update kernal################\n");
			kfp = fopen(KERNAL_UPDATE_FILE , "w");
			if(!kfp){
				printf("*******************open kernal update file error**********\n");
				goto exit;
			}
			kernal_f_size  = (1024-(p-buf));
			kcrc = checksum(kcrc, (unsigned char *)p, kernal_f_size);
			if(fwrite(p,1,kernal_f_size,kfp)!=kernal_f_size){
				printf("**********error write kernal.update************\n");
				goto exit;
			}
			while(kernal_f_size <__kernal_f_size){
				r = recv(socket , buf,1024,0);
				 if(r<=0){
				 	herror("tcp do update recvmsg error");
		 			 goto exit;
				 }
				kernal_f_size +=r;
				//printf("recv data=%d curr size =%d expected size = %d\n",r ,kernal_f_size , __kernal_f_size);
				kcrc = checksum(kcrc, (unsigned char *)buf , r);
				if((unsigned int)r!=fwrite(buf,1,r,kfp)){
					printf("**********error write kernal.update************\n");
					goto exit;
				}
			}
			if(kernal_f_size!=__kernal_f_size){
				printf("do kernal update error we expect file size %d but we recv %d\n",__kernal_f_size , kernal_f_size);
				goto exit;
			}
			if(kcrc!=__kcrc){
				printf("***************kernel sum error we expected %2x but the result is %2x************\n",__kcrc , kcrc);
				goto exit;
			}
			fclose(kfp);
			kfp = NULL;
			prepare_do_update();
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd0",KERNAL_UPDATE_FILE);
			system(buf);
			system("reboot &");
			exit(0);
			break;
		case 2:
			printf("###############update system and kernal################\n");
			sfp = fopen(SYSTEM_UPDATE_FILE , "w");
			if(!sfp){
				printf("*******************open system update file error**********\n");
				goto exit;
			}
			kfp = fopen(KERNAL_UPDATE_FILE , "w");
			if(!kfp){
				printf("*******************open kernal update file error**********\n");
				goto exit;
			}
			system_f_size  = (1024-(p-buf));
			scrc = checksum(scrc, (unsigned char *)p, system_f_size);
			if(fwrite(p,1,system_f_size,sfp)!=system_f_size){
				printf("**********error write system.update************\n");
				goto exit;
			}
			p=NULL;
			while(system_f_size <__system_f_size){
				r = recv(socket , buf,1024,0);
				 if(r<=0){
				 	herror("tcp do update recvmsg error");
		 			 goto exit;
				 }
				 if(__system_f_size - system_f_size<(unsigned int)r){
				 	scrc = checksum(scrc, (unsigned char *)buf, __system_f_size - system_f_size);
				 	if(fwrite(buf,1,__system_f_size - system_f_size , sfp)!=(__system_f_size - system_f_size)){
						printf("**********error write system.update************\n");
						goto exit;
				 	}
					p= buf;
					p+=(__system_f_size - system_f_size);
					system_f_size = __system_f_size;
					//printf("r = %d , system file size = %u we recv system update file ok\n",r , __system_f_size);
					break;
				 }
				system_f_size +=r;
				//printf("recv data=%d curr size =%u expected system file size = %u\n",r ,system_f_size , __system_f_size);
				scrc = checksum(scrc, (unsigned char *)buf, r);
				if((unsigned int )r!=fwrite(buf,1,r,sfp)){
					printf("**********error write system.update************\n");
					goto exit;
				}
			}
			if(system_f_size!=__system_f_size){
				printf("do system update error we expected file size %d but we recv %d\n",__system_f_size , system_f_size);
				goto exit;
			}
			kernal_f_size = 0;
			if(p){
				kernal_f_size =r-(p-buf);
				kcrc = checksum(kcrc, (unsigned char *)p, kernal_f_size);
				if(fwrite(p,1,kernal_f_size,kfp)!=kernal_f_size){
					printf("************error write kernal.updtea*************\n");
					goto exit;
				}
			}
			while(kernal_f_size <__kernal_f_size){
				r = recv(socket , buf,1024,0);
				 if(r<=0){
				 	herror("tcp do update recvmsg error:%s\n");
		 			 goto exit;
				 }
				kernal_f_size +=r;
				//printf("recv data=%d curr size =%d expected kernel file size = %d\n",r ,kernal_f_size , __kernal_f_size);
				kcrc = checksum(kcrc, (unsigned char *)buf , r);
				if((unsigned int)r!=fwrite(buf,1,r,kfp)){
					printf("**********error write kernal.update************\n");
					goto exit;
				}
			}
			if(kernal_f_size!=__kernal_f_size){
				printf("do kernal update error we expect file size %d but we recv %d\n",__kernal_f_size , kernal_f_size);
				goto exit;
			}
			if(scrc!=__scrc||kcrc!=__kcrc){
				printf("check sum error we expected system sum %2x  but the reslut is %2x we expected kernel sum %2x but the result is %2x\n",__scrc ,scrc,__kcrc , kcrc);
				goto exit;
			}
			fclose(sfp);
			fclose(kfp);
			
			prepare_do_update();
			printf("####################ok try update kernal###################\n");
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd0",KERNAL_UPDATE_FILE);
			system(buf);
			sleep(1);
			printf("####################update kernal ok try update system##############\n");
			memset(buf , 0 ,1024);
			sprintf(buf , "flashcp  %s  /dev/mtd1",SYSTEM_UPDATE_FILE);
			system(buf);
			system("reboot &");
			exit(0);
			break;
		default:
			printf("*****************get unkown cmd*****************************\n");
			goto exit;
	   }
	}
exit:
	if(reboot_flag){
		system("reboot &");
		exit(0);
	}
	if(sfp){
		system("reboot &");
		exit(0);
	}
	if(kfp){
		system("reboot &");
		exit(0);	
	}
	if(socket>=0){
		system("reboot &");
		exit(0);
	}
	del_sess(sess);
	take_sess_down( sess);
	return 0;
 }
*/

/*
int start_video_monitor(struct sess_ctx* sess)
{
//printf("*******************************Starting video monitor server*******************************\n");
	socklen_t fromlen;
	int ret;
	struct sockaddr_in address;
	int socket = -1;
	char* buffer;
	int size;
	int tryaccpet = MAX_CONNECTIONS;
	struct timeval timeout;
	struct timeval selecttv;
	fd_set acceptfds;
	//int setframes=0;

	if(is_do_update())
		goto exit;
	dbg("Starting video monitor server\n");
       add_sess( sess);
	take_sess_up( sess);
	//dbg("##############sess->id=%d################\n",sess->id);
	while(1) {
		pthread_mutex_lock(&sess->sesslock);
		if(!sess->running){
			pthread_mutex_unlock(&sess->sesslock);
			goto exit;
		}
		pthread_mutex_unlock(&sess->sesslock);

		
		if (sess->is_tcp) {
			selecttv.tv_sec = 5;
			selecttv.tv_usec = 0;
			printf("ready to connect\n");
__tryaccept:
			fromlen = sizeof(struct sockaddr_in);
			FD_ZERO(&acceptfds);
			FD_SET(sess->s1, &acceptfds);
			do{
				ret = select(sess->s1 + 1, &acceptfds, NULL, NULL, &selecttv);
			}while(ret == -1);
			if(ret == 0){
				tryaccpet --;
				if(tryaccpet<=0){
					close(sess->s1);
					sess->s1 = -1;
					goto exit;
				}
				goto __tryaccept;
			}
			
			socket = accept(sess->s1,(struct sockaddr *) &address,&fromlen);
			if(socket<0){
				printf("accept erro!\n");
				close(sess->s1);
				sess->s1=-1;
				goto exit;
			}
			if(sess->from.sin_addr.s_addr!=address.sin_addr.s_addr){
				close(socket);
				tryaccpet --;
				if(tryaccpet <=0){
					socket = -1;
					close(sess->s1);
					sess->s1 = -1;
					goto exit;
				}
				goto __tryaccept;
			}
			if(socket >= 0) {
				close(sess->s1);
				sess->s1=-1;

				
				//pthread_mutex_lock(&global_ctx_lock);
				//memcpy(&sess->from,&address,fromlen);
				//pthread_mutex_unlock(&global_ctx_lock);
				
				
				printf("connection in, addr=0x%x, port=%d\n",address.sin_addr.s_addr, ntohs(address.sin_port));
				printf("from.sin_addr=0x%x , from.sin_port=%d\n",sess->from.sin_addr.s_addr,ntohs(sess->from.sin_port));
				
				
				//pthread_mutex_lock(&sess->sesslock);
				//sess->haveconnected=1;
				//pthread_mutex_unlock(&sess->sesslock);
				
				 ret = playback_connect(sess->from, socket);

				 
				printf("%s: test  playback ret = %d\n", __func__, ret);
				//conncect for playback firstly. if it fails, connect for monitor.
				if(ret < 0){
					timeout.tv_sec  = 10;
					timeout.tv_usec = 0;
					 if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0){
					            printf("Error enabling socket rcv time out \n");
					  }
					if(threadcfg.sound_duplex){
						if (pthread_create(&sess->srtid, NULL, (void *) test_sound_tcp_transport, sess) < 0) {
							printf("create sound thread error\n");
							goto exit;
						} 
						printf("create sound thread sucess  id = %lu\n",sess->srtid);
						pthread_mutex_lock(&sess->sesslock);
						sess->ucount++;
						pthread_mutex_unlock(&sess->sesslock);
					}
					while(1) {
						if(is_do_update())
							goto exit;
						buffer = get_video_data(&size);
						int i = 0;
						while(size > 0 ) {
							if( size >= 1000 ) {
								ret = send(socket, buffer+i, 1000,0);
								if( ret <= 0 ) {
									printf("sent data error,the connection may currupt!\n");
									printf("ret==%d\n",ret);
									printf("something wrong kill myself now\n");
									goto exit;
								}
								//dbg("ret = %d\n",ret);
								size -= ret;
								i += ret;
							} else {
								ret = send(socket, buffer+i, size,0);
								if( ret <= 0 ) {
									printf("sent data error,the connection may currupt!\n");
									printf("ret==%d\n",ret);
									printf("something wrong kill myself now\n");
									goto exit;
								}
								//dbg("ret = %d\n",ret);
								size -= ret;
								i += ret;
							}
						}
						free(buffer);
						pthread_mutex_lock(&sess->sesslock);
						if(!sess->running){
							pthread_mutex_unlock(&sess->sesslock);
							goto exit;
						}
						pthread_mutex_unlock(&sess->sesslock);
						handle_video_thread();
					}				
				}else{
					socket=-1;
					goto exit;
				}
			}
		}
		//it seen never happen so not need to lock
		if(!sess->running)
			goto exit;
		handle_session(sess);
	}
exit:
	 //Take down session 
	pthread_mutex_lock(&sess->sesslock);
	sess->running = 0;
	pthread_mutex_unlock(&sess->sesslock);
	if(sess->srtid>0){
		printf("pthread_join id = %lu\n",sess->srtid);
		pthread_join(sess->srtid,NULL);
	}
	del_sess(sess);
	take_sess_down( sess);
	
	if(socket>=0){
		close(socket);
	}
	dbg("\nExitting server\n");
	return 0;
}
*/

int start_video_monitor(struct sess_ctx* sess)
{
	socklen_t fromlen;
	int ret;
	struct sockaddr_in address;
	int socket = -1;
	char* buffer;
	int size;
	pthread_t tid;
	int attempts;
	//int setframes=0;

	if(is_do_update())
		goto exit;
	dbg("Starting video monitor server\n");
       add_sess( sess);
	take_sess_up( sess);
	sess->ucount = 1;
	socket = sess->sc->video_socket;
	
	if(threadcfg.sound_duplex){
		if (pthread_create(&tid, NULL, (void *) start_audio_monitor, sess) < 0) {
			goto exit;
		} 
	}
	attempts = 0;
	for(;;){
		buffer = get_video_data(&size);
		int i = 0;
		while(size > 0 ) {
			if(!sess->running){
				dbg("the sess have been closed exit now\n");
				free(buffer);
				goto exit;
			}
			if( size >= 1000 ) {
				if(sess->is_tcp)
					ret = send(socket, buffer+i, 1000,0);
				else
					ret = udt_send(socket , SOCK_STREAM , buffer+i , 1000);
				
			} else {
				if(sess->is_tcp)
					ret = send(socket, buffer+i, size,0);
				else
					ret = udt_send(socket , SOCK_STREAM , buffer+i , size);
			}
			
			if(ret <= 0 ) {
				attempts ++;
				if(attempts <=10){
					dbg("attempts send data now = %d\n",attempts);
					continue;
				}
				printf("sent data error,the connection may currupt!\n");
				printf("ret==%d\n",ret);
				free(buffer);
				goto exit;
			}
			//printf("sess->is_tcp = %d , sent message =%d\n",sess->is_tcp , ret);
			attempts = 0;
			size -= ret;
			i += ret;
		}
		free(buffer);
		handle_video_thread();
	}
	
exit:
	/* Take down session */
	del_sess(sess);
	take_sess_down( sess);
	
	dbg("\nExitting server\n");
	return 0;
}


/*record function set*/

#define RECORD_STATE_STOP 	 	0
#define RECORD_STATE_SLOW	 	1
#define RECORD_STATE_FAST		2
#define RECORD_STATE_NORMAL	3
extern struct __syn_sound_buf syn_buf;
static nand_record_file_header record_header;
static nand_record_file_internal_header video_internal_header={
		{0,0,0,1,0xc},
			{0,1,0,0},
};
static nand_record_file_internal_header audio_internal_header={
		{0,0,0,1,0xc},
			{0,2,0,0},
};
static const int sensitivity_diff_size[4] = {10000,300,350,450};

char pause_record = 0;
char force_close_file = 0;

static inline char * gettimestamp()
{
	static char timestamp[15];
	time_t t;
	struct tm *curtm;
	 if(time(&t)==-1){
        	 printf("get time error\n");
         	 exit(0);
   	  }
	 curtm=localtime(&t);
	  sprintf(timestamp,"%04d%02d%02d%02d%02d%02d",curtm->tm_year+1900,curtm->tm_mon+1,
                curtm->tm_mday,curtm->tm_hour,curtm->tm_min,curtm->tm_sec);
	 return timestamp;
}

#define RECORD_SOUND

#ifdef RECORD_SOUND
static inline void write_syn_sound(int *need_video_internal_head)
{
	static unsigned int i = 0;
	char *buf;
	int ret;
	int size;
	if(!threadcfg.sdcard_exist)
		return ;
	*need_video_internal_head=0;
	buf = new_get_sound_data(MAX_NUM_IDS-1, & size);
	if(buf){
		i++;
		if(!(i%60))
			dbg("get sound data size == %d\n",size);		
		ret = nand_write(&audio_internal_header, sizeof(audio_internal_header));
		if( ret == 0 ){
			ret = nand_write(buf,size);
			if(ret ==0)
				*need_video_internal_head=1;
		}
		//audio_internal_header.flag[0]=0;
		free(buf);
	}else{
		dbg("sound data not prepare\n");
	}
}
#endif

static  void vs_msg_debug()
{
	int ret;
	vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_DEBUG;
	msg.msg[1] = 0;
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		printf("send daemon message error\n");
		system("reboot &");
		exit(0);
	}
	printf(__FILE__":%s:CAMERA DEBUG\n",__func__);
	exit(0);
}

int snd_soft_restart();

int start_video_record(struct sess_ctx* sess)
{
	//const char *videodevice = "/dev/video0";
	//int format = V4L2_PIX_FMT_MJPEG;
	//int grabmethod = 1;
	int ret;
	char* buffer;
	int size;
	int i;
	int size0=0;
	char *timestamp;
	//struct timeval stop_time;
	//struct timeval open_time;
	//unsigned long long un_cpture_time;
	struct timeval starttime,endtime;
#ifdef RECORD_SOUND
	struct timeval prev_write_sound_time;
#endif
	struct timeval alive_old_time;
	struct timeval mail_last_time;
	struct timeval index_table_15sec_last_time;
	vs_ctl_message msg , rmsg;
	unsigned long long timeuse;
	unsigned long long usec_between_image=0;
	int frameratechange=0;
	pthread_t mail_alarm_tid=0;
	char swidth[12];
	char sheight[12];
	char FrameRateUs[12];
	//int type;
	//int num_pic_to_ignore = 150;
	int prev_width;
	int prev_height;
	int width ;
	int height;
	//int change_resolution =0;
	//int curr_resolution = 1;/*0:qvga; 1:vga; 2:720p*/
	//int big_to_small = 1;
	int email_alarm;
	int record_slow_speed=0;
	int record_fast_speed=0;
	//char record_slow_resolution[32];
	//char record_fast_resolution[32];
	int record_normal_speed;
	int record_normal_duration=3;
	int record_last_state = 0;
	int timestamp_change=0;
	int pictures_to_write = 0;
	int record_fast_duration=0; 
	//char record_resolution[32];
	int sensitivity_index = threadcfg.record_sensitivity;;
	int record_mode = 0;
	int need_write_internal_head=0;
	//int write_internal_head=0;
	char *attr_table;
	char *time_15sec_table;
	char * attr_pos ;
	char * record_15sec_pos ;
	unsigned int *attr_table_size;
	unsigned int *record_15sec_table_size;
	index_table_item_t	  table_item;
	//char *audio_internal_header_p;
	//char *video_internal_header_p;

	//unsigned int * value;

	dbg("Starting video record\n");
	
	if( !vdin_camera ){
		if(( vdin_camera = (struct vdIn *)init_camera() ) == NULL ){
			printf("init camera error\n");
			return -1;
		}
	}
	/* Set up signals */
	for (i = 1; i <= _NSIG; i++) {
		if (i == SIGIO || i == SIGINT)
			sigset(i, sig_handler);
		else
			sigignore(i);
	}
	
	if(threadcfg.sdcard_exist){
		attr_table = (char *)malloc(INDEX_TABLE_SIZE);
		time_15sec_table = (char *)malloc(INDEX_TABLE_SIZE);
		
		if(!attr_table||!time_15sec_table){
			printf("error malloc buffer for index table\n");
			exit(0);
		}
		memset(attr_table ,0 ,INDEX_TABLE_SIZE);
		memset(time_15sec_table , 0 ,INDEX_TABLE_SIZE);
		attr_table_size = (unsigned int *)attr_table;
		record_15sec_table_size = attr_table_size+1;
		attr_pos =(char *)(record_15sec_table_size+1);
		record_15sec_pos = time_15sec_table;
	}
	
	i = 0;
	width = vdin_camera->width;
	height = vdin_camera->height;
	prev_height =height = vdin_camera->height;
	prev_width = width =vdin_camera->width;
	printf("width ==%d , height == %d\n",width , height);

	
	if(threadcfg.email_alarm){
		init_mail_attatch_data_list(threadcfg.mailbox);
		if (pthread_create(&mail_alarm_tid, NULL, (void *) mail_alarm_thread, NULL) < 0) {
			printf("mail alarm thread create fail\n");
			mail_alarm_tid=0;
		} else
			printf("mail alarm thread create sucess\n");
	}


	//pthread_mutex_lock(&threadcfg.threadcfglock);
	usec_between_image = 0;
	if(strncmp(threadcfg.record_mode,"no_record",strlen("no_record")) ==0)
		record_mode = 0;
	else if(strncmp(threadcfg.record_mode,"inteligent",strlen("inteligent")) ==0){
		printf("record_mode ==inteligent\n");
		record_mode = 2;
		record_fast_duration = threadcfg.record_fast_duration;
		record_slow_speed = threadcfg.record_slow_speed;
		record_fast_speed = threadcfg.record_fast_speed;
		usec_between_image = (unsigned long long )1000000/record_fast_speed;
		//memcpy(record_slow_resolution,threadcfg.record_slow_resolution,32);
		//memcpy(record_fast_resolution,threadcfg.record_fast_resolution,32);
	}
	else {
		printf("record_mode==normal\n");
		record_mode =3;
		record_normal_duration = threadcfg.record_normal_duration;
		usec_between_image=(unsigned long long )1000000/threadcfg.record_normal_speed;
		//memcpy(record_resolution,threadcfg.resolution,32);
	}
	record_normal_speed = threadcfg.record_normal_speed;
	email_alarm = threadcfg.email_alarm;
	sensitivity_index = threadcfg.record_sensitivity;
	printf("record_sensitivity == %d\n",sensitivity_diff_size[sensitivity_index]);
	//pthread_mutex_unlock(&threadcfg.threadcfglock);
	record_last_state = RECORD_STATE_FAST;

	
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_START_RECORDING;
	msg.msg[1] = 0;
	gettimeofday(&alive_old_time,NULL);
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
		exit(0);
	}
	
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_RECORD_ALIVE;
	msg.msg[1] = 0;

	
	sprintf(FrameRateUs,"%08llu",(unsigned long long)20000);
	memcpy(audio_internal_header.FrameRateUs,FrameRateUs,sizeof(audio_internal_header.FrameRateUs));
	memset(audio_internal_header.FrameWidth,0,sizeof(audio_internal_header.FrameWidth));
	memset(audio_internal_header.FrameHeight,0,sizeof(audio_internal_header.FrameHeight));
	
	sprintf(swidth,"%04d",width);
	sprintf(sheight,"%04d",height);
	sprintf(FrameRateUs,"%08llu",usec_between_image);
	memcpy(video_internal_header.FrameRateUs,FrameRateUs,sizeof(video_internal_header.FrameRateUs));
	memcpy(video_internal_header.FrameHeight,sheight,sizeof(video_internal_header.FrameHeight));
	memcpy(video_internal_header.FrameWidth,swidth,sizeof(video_internal_header.FrameWidth));

	
	timestamp = gettimestamp();
	memcpy(audio_internal_header.StartTimeStamp,timestamp,sizeof(audio_internal_header.StartTimeStamp));
	memcpy(video_internal_header.StartTimeStamp,timestamp,sizeof(video_internal_header.StartTimeStamp));
/*
	printf("audio_internal_header.head==");
	for(i=0;i<5;i++)
		printf("%2x ",audio_internal_header.head[i]);
	printf("\n");
	printf("audio_internal_hreader.flag==");
	for(i=0;i<4;i++)
		printf("%2x ",audio_internal_header.flag[i]);
	printf("\n");
	printf("audio_internal_header.timestamp==%s\n",audio_internal_header.StartTimeStamp);

	printf("video_internal_header.head==");
	for(i=0;i<5;i++)
		printf("%2x ",video_internal_header.head[i]);
	printf("\n");
	printf("video_internal_hreader.flag==");
	for(i=0;i<4;i++)
		printf("%2x ",video_internal_header.flag[i]);
	printf("\n");
	printf("video_internal_header.timestamp==%s\n",video_internal_header.StartTimeStamp);
	printf("video_internal_header.FrameWidth==%s\n",video_internal_header.FrameWidth);
	printf("video_internal_header.FrameHeight==%s\n",video_internal_header.FrameHeight);
	*/
	reset_syn_buf();

	if(threadcfg.sdcard_exist){
		table_item.location = 0;
		memcpy(record_15sec_pos,&table_item ,sizeof(index_table_item_t));
		(*record_15sec_table_size) +=sizeof(index_table_item_t);
		//dbg("write 15sec location  pos = %u , write in %p , 15sec table size = %u\n",table_item.location , record_15sec_pos,*record_15sec_table_size);
		record_15sec_pos +=sizeof(index_table_item_t);
	}
	
	memset(&mail_last_time , 0 ,sizeof(struct timeval));
	gettimeofday(&starttime,NULL);
#ifdef RECORD_SOUND
	//timestamp = gettimestamp();
	//memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
	audio_internal_header.flag[0]=SET_ALL_FLAG_CHANGE;
	memcpy(&prev_write_sound_time,&starttime,sizeof(struct timeval));
#endif
	video_internal_header.flag[0] = SET_ALL_FLAG_CHANGE;
	memcpy(&index_table_15sec_last_time , &starttime , sizeof(struct timeval));
	while(1) {
		if(is_do_update()){
			close_v4l2(vdin_camera);
			dbg("is do update return now\n");
			goto __out;
		}
		gettimeofday(&endtime,NULL);
		if(abs(endtime.tv_sec -alive_old_time.tv_sec)>=3){
			ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
			if(ret == -1){
				dbg("send daemon message error\n");
					if(is_do_update()){
						close_v4l2(vdin_camera);
						dbg("is do update return now\n");
						goto __out;
					}
				system("reboot &");
				exit(0);
			}
			memcpy(&alive_old_time,&endtime,sizeof(struct timeval));
		}

		
		//dbg("before get buffer\n");
		buffer = get_data(&size,&width,&height);
		//dbg("after get buffer\n");
		/*
		if(big_to_small >0&&num_pic_to_ignore>0){
			size0 = size;
			num_pic_to_ignore --;
		}
		*/
		if(threadcfg.sdcard_exist){
			gettimeofday(&endtime,NULL);
			if(abs(endtime.tv_sec - index_table_15sec_last_time.tv_sec)>=15){
				table_item.location =nand_get_position();
				if(*record_15sec_table_size + *attr_table_size +sizeof(index_table_item_t) + 8 <=INDEX_TABLE_SIZE){
					memcpy(record_15sec_pos,&table_item ,sizeof(index_table_item_t));
					(*record_15sec_table_size) +=sizeof(index_table_item_t);
					memcpy(&index_table_15sec_last_time ,&endtime,sizeof(struct timeval));
					//dbg("write 15sec location  pos = %u , write in %p , 15sec table size = %u\n",table_item.location , record_15sec_pos,*record_15sec_table_size);
					record_15sec_pos +=sizeof(index_table_item_t);
				}
			}
		}

		if(pictures_to_write>0&&abs(size-size0)>sensitivity_diff_size[sensitivity_index]){
			if(record_mode == 2)
				pictures_to_write = record_fast_speed * record_fast_duration;
			else if(record_mode ==1)
				pictures_to_write = record_normal_speed*record_normal_duration;
		}
		
		switch(record_mode){
			case 0:/*not record*/
				pictures_to_write= 0;
				break;
			case 1:/*normal*/
			{
				//printf("in normal before enter record_last_state==%d\n",record_last_state);
				//printf("in normal before enter  picture_to_write==%d\n",pictures_to_write);
				//printf("abs(size-size0)==%d\n",abs(size-size0));
				if(pictures_to_write<=0){
					if(abs(size-size0)>sensitivity_diff_size[sensitivity_index]){
						usec_between_image = (unsigned long long )1000000/record_normal_speed;
						pictures_to_write = record_normal_speed * record_normal_duration;
						gettimeofday(&starttime,NULL);
						if(record_last_state ==RECORD_STATE_STOP){
#ifdef RECORD_SOUND
							set_syn_sound_data_clean(MAX_NUM_IDS-1);
							timestamp = gettimestamp();
							memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
							memcpy(&prev_write_sound_time,&starttime,sizeof(struct timeval));
#endif
							record_last_state = RECORD_STATE_FAST;
							timestamp_change = 1;
						}
					}else{
						pictures_to_write= 0;
						if(record_last_state == RECORD_STATE_FAST){
#ifdef RECORD_SOUND
						write_syn_sound(&need_write_internal_head);
#endif
							record_last_state = RECORD_STATE_STOP;
						}
					}
				}else{
					if(record_normal_speed<25){
						gettimeofday(&endtime,NULL);
						timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
						if(abs(timeuse)>=usec_between_image){
							memcpy(&starttime,&endtime,sizeof(struct timeval));
						}else{
							size0 = size;
							free(buffer);
							//printf("inteligent time not come? usec_between_image ==%llu\n",usec_between_image);
							continue;
						}
					}
				}
				//printf("in normal after out record_last_state==%d\n",record_last_state);
				//printf("in normal after out picture_to_write==%d\n",pictures_to_write);
				break;
			}
			case 2:/*inteligent*/
			{
				if(pictures_to_write<=0){
					if(abs(size-size0)>sensitivity_diff_size[sensitivity_index]){
						//printf("inteligent something move\n");
						//memcpy(record_resolution,record_fast_resolution,32);
						pictures_to_write = record_fast_speed * record_fast_duration;
						if(record_last_state==RECORD_STATE_SLOW){
							record_last_state = RECORD_STATE_FAST;
							frameratechange = 1;
							usec_between_image = (unsigned long long )1000000/record_fast_speed;
							memset(&starttime,0,sizeof(struct timeval)); /*write it right now!*/
						}
					}else{
						if(record_last_state==RECORD_STATE_FAST){
							record_last_state = RECORD_STATE_SLOW;
							frameratechange = 1;
							usec_between_image = (unsigned long long ) 1000000/record_slow_speed;
						}
						//memcpy(record_resolution,record_slow_resolution,32);
						pictures_to_write =1;
					}
					gettimeofday(&endtime,NULL);
					timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
					if(abs(timeuse)>=usec_between_image){
						memcpy(&starttime,&endtime,sizeof(struct timeval));
					}else{
						pictures_to_write = 0;
					}
				}else{ 
					if(record_fast_speed<25){
						gettimeofday(&endtime,NULL);
						timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
						if(abs(timeuse)>=usec_between_image){
							memcpy(&starttime,&endtime,sizeof(struct timeval));
						}else{
							size0 = size;
							free(buffer);
							//printf("inteligent time not come? usec_between_image ==%llu\n",usec_between_image);
							continue;
						}
					}
				}
				break;
			}
			case 3:{/*this is the real normal*/
				usec_between_image = (unsigned long long ) 1000000 /record_normal_speed;
				record_last_state = RECORD_STATE_NORMAL;
				gettimeofday(&endtime , NULL);
				timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
				if(abs(timeuse)>=usec_between_image){
					pictures_to_write = 1;
					memcpy(&starttime , &endtime,sizeof(struct timeval));
				}else
					pictures_to_write = 0;
				//in normal mode the mail alarm must happen as the other mode
				if(email_alarm&&mail_alarm_tid&&abs(size-size0)>sensitivity_diff_size[sensitivity_index]&&
					prev_height == height &&prev_width == width&&size>8000){
					char *image=malloc(size);
					if(image){
						memcpy(image,buffer,size);
						add_image_to_mail_attatch_list_no_block( image,  size);
					}
				}
				break;
			}
			default:/*something wrong*/
				exit(0);
		}

		size0 = size;
		if(pictures_to_write<=0){
			free(buffer);
			usleep(1000);
			continue;
		}
		
		pictures_to_write --;

		//printf("pictures to write ==%d\n", pictures_to_write);
		if(email_alarm&&mail_alarm_tid&&pictures_to_write&&size>8000){
			gettimeofday(&endtime , NULL);
			if(abs(endtime.tv_sec - mail_last_time.tv_sec)>=1){
				char *image=malloc(size);
				if(image){
					memcpy(image,buffer,size);
					add_image_to_mail_attatch_list_no_block( image,  size);
				}
				memcpy(&mail_last_time , &endtime, sizeof(struct timeval));
			}
		}

		/*the need of a boss is change every sec , i need to change the code every time
		*may be change back to the original designed ...
		*/
		//video_internal_header_p = video_internal_header.StartTimeStamp;
		//audio_internal_header_p = audio_internal_header.StartTimeStamp;
		
		if(need_write_internal_head||timestamp_change||frameratechange||prev_width!=width||prev_height!=height){
			need_write_internal_head = 1;
			timestamp_change = 0;
			frameratechange = 0;
			prev_width = width;
			prev_height = height;
		}
		/*
		if(timestamp_change){
			//printf("timestamp_change\n");
			timestamp = gettimestamp();
			//printf("%s\n",timestamp);
			timestamp_change = 0;
			video_internal_header.flag[0]|=(1<<FLAG0_TS_CHANGED_BIT);
			audio_internal_header.flag[0]|=(1<<FLAG0_TS_CHANGED_BIT);
			memcpy(video_internal_header_p,timestamp,sizeof(video_internal_header.StartTimeStamp));
			memcpy(audio_internal_header_p,timestamp,sizeof(audio_internal_header.StartTimeStamp));
			video_internal_header_p+=sizeof(video_internal_header.StartTimeStamp);
			audio_internal_header_p+=sizeof(audio_internal_header.StartTimeStamp);
			need_write_internal_head = 1;
		}
		if(frameratechange){
			//printf("frmeratechange\n");
			frameratechange = 0;
			video_internal_header.flag[0]|=(1<<FLAG0_FR_CHANGED_BIT);
			sprintf(FrameRateUs,"%08llu",usec_between_image);
			memcpy(video_internal_header_p,FrameRateUs,sizeof(video_internal_header.FrameRateUs));
			video_internal_header_p += sizeof(video_internal_header.FrameRateUs);
			//printf("%s\n",FrameRateUs);
			need_write_internal_head = 1;
		}
		if(prev_width!=width){
			//printf("picture width change\n");
			prev_width = width;
			video_internal_header.flag[0]|=(1<<FLAG0_FW_CHANGED_BIT);
			sprintf(swidth,"%04d",width);
			memcpy(video_internal_header_p,swidth,sizeof(video_internal_header.FrameWidth));
			video_internal_header_p += sizeof(video_internal_header.FrameWidth);
			need_write_internal_head = 1;
		}
		if(prev_height!=height){
			//printf("picture height change\n");
			prev_height = height;
			video_internal_header.flag[0]|=(1<<FLAG0_FH_CHANGED_BIT);
			sprintf(sheight,"%04d",height);
			memcpy(video_internal_header_p,sheight,sizeof(video_internal_header.FrameHeight));
			video_internal_header_p += sizeof(video_internal_header.FrameHeight);
			need_write_internal_head = 1;
		}
		*/
		if(need_write_internal_head){
			//printf("write video_internal_header\n");
			
			if(threadcfg.sdcard_exist){	
				timestamp = gettimestamp();
				sprintf(swidth,"%04d",width);
				sprintf(sheight,"%04d",height);
				sprintf(FrameRateUs,"%08llu",usec_between_image);
				memcpy(video_internal_header.StartTimeStamp,timestamp,sizeof(video_internal_header.StartTimeStamp));
				memcpy(video_internal_header.FrameRateUs,FrameRateUs,sizeof(video_internal_header.FrameRateUs));
				memcpy(video_internal_header.FrameHeight,sheight,sizeof(video_internal_header.FrameHeight));
				memcpy(video_internal_header.FrameWidth,swidth,sizeof(video_internal_header.FrameWidth));
			
				table_item.location = nand_get_position();
				if(*record_15sec_table_size + *attr_table_size +sizeof(index_table_item_t) + 8 <=INDEX_TABLE_SIZE){
				 	if(nand_write(&video_internal_header,sizeof(video_internal_header))==0){
						memcpy(attr_pos,&table_item ,sizeof(index_table_item_t));
						(*attr_table_size)+=sizeof(index_table_item_t);
						//dbg("write attr location  pos = %u,write in %p , attr table size = %u struct size = %d\n",table_item.location , attr_pos, *attr_table_size , video_internal_header_p -(char *) &video_internal_header);
						attr_pos +=sizeof(index_table_item_t);
				 	}
				}
			}
			//video_internal_header.flag[0]=0;
			need_write_internal_head = 0;
		}
		//printf("write video picture size = %d\n" , size);
retry:
		if(threadcfg.sdcard_exist){
			ret = nand_write(buffer, size);
			if(force_close_file||msgrcv(msqid,&rmsg,sizeof(rmsg) - sizeof( long ), VS_MESSAGE_RECORD_ID , IPC_NOWAIT )>0){
				dbg("####################force close file#################\n");
				memcpy(attr_pos , time_15sec_table , *record_15sec_table_size);
				nand_write_index_table( attr_table);
				nand_prepare_close_record_header(&record_header);
				nand_write_end_header(&record_header);
				snd_soft_restart();
			}
			if( ret == 0 ){
				i++;
				usleep(1);
				if( i % 1000 == 0 ){
					dbg("write pictures: %d\n",i);
				}
	//			dbg("write to nand=%d\n",size);
			}
			else if( ret == VS_MESSAGE_NEED_START_HEADER ){
				nand_prepare_record_header(&record_header);
				sprintf(swidth,"%04d",width);
				sprintf(sheight,"%04d",height);
				sprintf(FrameRateUs,"%08llu",usec_between_image);
				memcpy(&record_header.FrameWidth,&swidth,4);
				memcpy(&record_header.FrameHeight,&sheight,4);
				memcpy(&record_header.FrameRateUs,&FrameRateUs,8);
				nand_write_start_header(&record_header);
				goto retry;
			}
			
			else if( ret == VS_MESSAGE_NEED_END_HEADER ){
				
				memcpy(attr_pos , time_15sec_table , *record_15sec_table_size);
				nand_write_index_table( attr_table);
				nand_prepare_close_record_header(&record_header);
				nand_write_end_header(&record_header);
				
				memset(attr_table ,0 ,INDEX_TABLE_SIZE);
				memset(time_15sec_table , 0 ,INDEX_TABLE_SIZE);
				attr_table_size = (unsigned int *)attr_table;
				record_15sec_table_size = attr_table_size+1;
				attr_pos =record_15sec_table_size+1;
				record_15sec_pos = time_15sec_table;
				gettimeofday(&index_table_15sec_last_time , NULL);
				
				table_item.location = 0;
				memcpy(record_15sec_pos,&table_item ,sizeof(index_table_item_t));
				(*record_15sec_table_size) +=sizeof(index_table_item_t);
				dbg("write 15sec location  pos = %u , write in %p , 15sec table size = %u\n",table_item.location , record_15sec_pos,*record_15sec_table_size);
				record_15sec_pos +=sizeof(index_table_item_t);
				//dbg("#######################nand file write index table###############\n");
#ifdef RECORD_SOUND
				set_syn_sound_data_clean(MAX_NUM_IDS-1);
				timestamp = gettimestamp();
				memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
				gettimeofday(&prev_write_sound_time , NULL);
#endif
				goto retry;
			}
			else{
				printf("#########################write nand error##########################\n");
				system("reboot &");
				exit(0);
				//vs_msg_debug();
			}
		}
		free(buffer);
#ifdef RECORD_SOUND
		gettimeofday(&endtime , NULL);
		timeuse = (endtime.tv_sec - prev_write_sound_time.tv_sec)*1000000UL +
			endtime.tv_usec - prev_write_sound_time.tv_usec;
		if(timeuse>=1000000ULL){
			write_syn_sound( &need_write_internal_head);
			memcpy(&prev_write_sound_time , &endtime  , sizeof(endtime));
			timestamp = gettimestamp();
			memcpy(audio_internal_header.StartTimeStamp , timestamp , sizeof(audio_internal_header.StartTimeStamp));
		}
#endif
		//check if it is time come to write sound data
	}

	dbg("Exitting record server");
__out:
	while(is_do_update())
	{
		ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
		sleep(3);
	}
	return 0;
}

