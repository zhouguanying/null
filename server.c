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

pthread_mutex_t acceptlock;//if a listen socket is create,we will lock until we accpet
pthread_mutex_t global_ctx_lock;
struct sess_ctx *global_ctx_running_list=NULL; //each connection have a sesction
int 	     		     currconnections=0;

struct threadconfig threadcfg;  //we use this variable to control the the speed of record thread
int msqid= -1;
/*
int 			     globalsocket=-1; 
we have wanted to  use this socket to listen,
now we create a listen socket each set_transport_type 
then close it after we accpet.
not need the global listen socket now!*/

int daemon_msg_queue;
int v2ipd_shm_id;
vs_share_mem* v2ipd_share_mem;


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
	printf("free_system_session now\n");
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
	if (sess->video.sess != NULL)
		deinit_video(sess);
	if (sess->audio.sess != NULL)
		deinit_audio(sess);
	if (sess->s1 >= 0){
		close(sess->s1);
	}
	if (sess->s2 >= 0)
		close(sess->s2);
	if (sess->to != NULL)
		free(sess->to);
	if (sess->myaddr != NULL)
		free(sess->myaddr);
	if (sess->pipe_fd >= 0)
		close(sess->pipe_fd);
	if (sess->pipe_name != NULL) {
		unlink(sess->pipe_name);
		free(sess->pipe_name);
	}
	if (sess->file_fd >= 0)
		close(sess->file_fd);

#ifdef VIDEO_STATS
	if (sess->video.tv != NULL)
		free(sess->video.tv);
#endif /* VIDEO_STATS */
#ifdef AUDIO_STATS
	if (sess->audio.tv != NULL)
		free(sess->audio.tv);
#endif /* AUDIO_STATS */

#ifdef USE_CLI
	if (sess->cli_sess != NULL)
		cli_deinit(sess->cli_sess);
#endif /* USE_CLI */
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

	/* Create session context */
	sess = malloc(sizeof(*sess));
	if (sess == NULL)
		return NULL;
	memset(sess, 0, sizeof(*sess));

	/* Add name of session */
	sess->name = malloc(strlen(name) + 1); /* +1 for string termination */
	if (sess->name == NULL)
		goto error1;
	memset(sess->name, 0, strlen(name));
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
	int canuse=0;
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
	buff=malloc(vdin_camera->buf.bytesused+DHT_SIZE);
	if(!buff){
		printf("malloc buf error\n");
		pthread_mutex_unlock(&vdin_camera->tmpbufflock);
		*size=0;
		return 0;
	}
	memcpy(buff,vdin_camera->tmpbuffer,vdin_camera->buf.bytesused+DHT_SIZE);
	vdin_camera->hrb_tid[canuse]=self_tid;
	*size=vdin_camera->buf.bytesused+DHT_SIZE;
	pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	//printf("read tmpbuffer sucess size=%d\n",*size);
	return buff;
have_readed:
	pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	usleep(20000);
	goto try_again;
}
static char* get_data(int* size)
{
	 char* buf;
	 
	pthread_mutex_lock(&vdin_camera->tmpbufflock);
retry:
	if (uvcGrab (vdin_camera) < 0) {
		printf("Error grabbing\n");
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
	memcpy(buf, vdin_camera->tmpbuffer, vdin_camera->buf.bytesused + DHT_SIZE );
	*size = vdin_camera->buf.bytesused + DHT_SIZE;
	pthread_mutex_unlock(&vdin_camera->tmpbufflock);
	return buf;
}
#endif
/*
int rtp_thread(struct sess_ctx * sess)
{
	struct sess_ctx * tmp;
	int ret;
	 pthread_mutex_lock(&global_ctx_lock);
	sess->next=global_ctx_running_list;
	global_ctx_running_list=sess;
	currconnections++;
	pthread_mutex_unlock(&global_ctx_lock);
	ret= rtp_playback_connect(sess->from);
	if(ret<0){
	}else{
	}
}
*/

int start_video_monitor(struct sess_ctx* sess)
{
	socklen_t fromlen;
	int ret;
	struct sockaddr_in address;
	int socket;
	char* buffer;
	int size;
	int i;
	struct sess_ctx * tmp;
	struct timeval timeout;
	//int setframes=0;
	dbg("Starting video monitor server\n");
	if( !vdin_camera ){
		if(( vdin_camera = init_camera() ) == NULL ){
			printf("init camera error\n");
			pthread_mutex_unlock(&acceptlock);
			pthread_mutex_lock(&global_ctx_lock);
			if(g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
			pthread_mutex_unlock(&global_ctx_lock);
			pthread_mutex_lock(&sess->sesslock);
			sess->running=0;
			sess->ucount--;
			if(sess->ucount<=0){
				pthread_mutex_unlock(&sess->sesslock);
				free_system_session(sess);
			}else
				pthread_mutex_unlock(&sess->sesslock);
			return -1;
		}
	}
       pthread_mutex_lock(&global_ctx_lock);
	sess->next=global_ctx_running_list;
	global_ctx_running_list=sess;
	currconnections++;
	pthread_mutex_unlock(&global_ctx_lock);
	/* Set up signals */
	for (i = 1; i <= _NSIG; i++) {
		if (i == SIGIO || i == SIGINT)
			sigset(i, sig_handler);
		else
			sigignore(i);
	}

	/* Spin */
	pthread_mutex_lock(&sess->sesslock);
	sess->running = 1;
	sess->soft_reset = 0; /* Clear soft reset condition */
	pthread_mutex_unlock(&sess->sesslock);
	
	while(1) {
		pthread_mutex_lock(&sess->sesslock);
		if(!sess->running){
			pthread_mutex_unlock(&acceptlock);
			pthread_mutex_unlock(&sess->sesslock);
			goto exit;
		}
		pthread_mutex_unlock(&sess->sesslock);
		if (sess->is_tcp) {
			fromlen = sizeof(struct sockaddr_in);
			printf("ready to connect\n");
			//printf("my thread id is==%lu\n",pthread_self());
			//printf("sess->s1=%d\n",sess->s1);
			socket = accept(sess->s1,(struct sockaddr *) &address,&fromlen);
			if(socket<0){
				printf("accept erro!\n");
				close(sess->s1);
				sess->s1=-1;
				pthread_mutex_unlock(&acceptlock);
				goto exit;
			}
			if(socket >= 0) {
				close(sess->s1);
				sess->s1=-1;
				pthread_mutex_unlock(&acceptlock);
				pthread_mutex_lock(&global_ctx_lock);
				memcpy(&sess->from,&address,fromlen);
				pthread_mutex_unlock(&global_ctx_lock);
				printf("connection in, addr=0x%x, port=0x%d\n",address.sin_addr.s_addr, address.sin_port);
				pthread_mutex_lock(&sess->sesslock);
				sess->haveconnected=1;
				pthread_mutex_unlock(&sess->sesslock);
				 ret = playback_connect(address, socket);
				printf("%s: ret = %d\n", __func__, ret);
				//conncect for playback firstly. if it fails, connect for monitor.
				if(ret < 0){
					timeout.tv_sec  = 5;
					timeout.tv_usec = 0;
					 if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0){
					            printf("Error enabling socket rcv time out \n");
					  }
					/*
					if (pthread_create(&sess->srtid, NULL, (void *) test_sound_tcp_transport, sess) < 0) {
						printf("create sound thread error\n");
						goto exit;
					} 
					pthread_mutex_lock(&sess->sesslock);
					sess->ucount++;
					pthread_mutex_unlock(&sess->sesslock);
					*/
					while(1) {
						buffer = get_video_data(&size);
						//printf("get data: size=%d\n",size);
						int i = 0;
						while(size > 0 ) {
							if( size >= 1000 ) {
								ret = send(socket, buffer+i, 1000,0);
								size -= 1000;
								i += 1000;
							} else {
								ret = send(socket, buffer+i, size,0);
								size = 0;
							}
						}
						//printf("send data, ret=%d\n", ret);
						free(buffer);
						usleep(1);
						pthread_mutex_lock(&sess->sesslock);
						if(!sess->running){
							pthread_mutex_unlock(&sess->sesslock);
							goto exit;
						}
						pthread_mutex_unlock(&sess->sesslock);
						if( ret < 0 ) {
							printf("sent data error,the connection may currupt!\n");
							printf("ret==%d\n",ret);
							printf("something wrong kill myself now\n");
							goto exit;
						}
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
	/* Take down session */
	pthread_mutex_lock(&global_ctx_lock);
	tmp=global_ctx_running_list;
	if(tmp==NULL){
		dbg("running_list erro*****************************************************\n");
		if(g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
		pthread_mutex_unlock(&global_ctx_lock);
		pthread_mutex_lock(&sess->sesslock);
		sess->ucount--;
		sess->running=0;
		if(sess->ucount<=0){
			pthread_mutex_unlock(&sess->sesslock);
			free_system_session(sess);
		}else
			pthread_mutex_unlock(&sess->sesslock);
		return -1;
	}
	if(tmp==sess){
		global_ctx_running_list=global_ctx_running_list->next;
		goto done;
	}
	while(tmp->next!=NULL){
		if(tmp->next==sess){
			tmp->next=sess->next;
			goto done;
		}
		tmp=tmp->next;
	}
	dbg("running list erro**************************************************************\n");
	if(g_cli_ctx->arg==sess)
			g_cli_ctx->arg=NULL;
	pthread_mutex_unlock(&global_ctx_lock);
	pthread_mutex_lock(&sess->sesslock);
	sess->ucount--;
	sess->running=0;
	if(sess->ucount<=0){
		pthread_mutex_unlock(&sess->sesslock);
		free_system_session(sess);
	}else
		pthread_mutex_unlock(&sess->sesslock);
	return -1;
done:
	currconnections--;
	if(g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
	pthread_mutex_unlock(&global_ctx_lock);
	pthread_mutex_lock(&sess->sesslock);
	sess->ucount--;
	sess->running=0;
	if(sess->ucount<=0){
		pthread_mutex_unlock(&sess->sesslock);
		free_system_session(sess);
	}else
		pthread_mutex_unlock(&sess->sesslock);
	if(socket>=0){
		printf("thread exit close set data socket==%d\n",socket);
		close(socket);
	}
	if (sess->soft_reset)
		/*ret = kill_connection(sess)*/;
	else
		/*ret = vpu_ExitSession()*/;
	//we eixt now kill ourself
	printf ("\nend/exit returned=%d\n", ret);
	printf("my thread_id is==%lu\n",pthread_self());

	dbg("\nExitting server\n");
	return 0;
/*	
erro:
	free_system_session(sess);
	return -1;
	*/
}


static nand_record_file_header record_header;
static const int sensitivity_diff_size[4] = {10000,250,400,500};
int start_video_record(struct sess_ctx* sess)
{
	int ret;
	char* buffer;
	int size;
	int i;
	int size0=0;
	struct timeval starttime,endtime;
	struct timeval alive_old_time,alive_curr_time;
	vs_ctl_message msg;
	unsigned long long timeuse;
	unsigned long long usec_between_image=0,prev_usec_between_image=0;
	pthread_t mail_alarm_tid=0;
	char swidth[12];
	char sheight[12];
	char FrameRateUs[12];
	int width = 640;
	int height = 480;
	int change_resolution =0;
	int curr_resolution = 1;/*0:qvga; 1:vga; 2:720p*/
	int email_alarm;
	int record_slow_speed=0;
	int record_fast_speed=0;
	char record_slow_resolution[32];
	char record_fast_resolution[32];
	int framerate;
	int pictures_to_write = 0;
	int record_fast_duration=0; 
	char record_resolution[32];
	int sensitivity_index = threadcfg.record_sensitivity;;
	int record_mode = 0;

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

	/* Spin */
	if(sess){
		sess->running = 1;
		sess->soft_reset = 0; /* Clear soft reset condition */
	}


	i = 0;

	
	if(strncmp(threadcfg.resolution,"qvga",4) ==0){
		curr_resolution = 0;
		width = 320;
		height = 240;
	}
	else if(strncmp(threadcfg.resolution,"vga",3) ==0){
		curr_resolution =1;
		width = 640;
		height = 480;
	}
	else {
		curr_resolution =2;
		width = 1280;
		height = 720;
	}
	change_resolution = curr_resolution;

	
	if(threadcfg.email_alarm){
		init_mail_attatch_data_list(threadcfg.mailbox);
		if (pthread_create(&mail_alarm_tid, NULL, (void *) mail_alarm_thread, NULL) < 0) {
			printf("mail alarm thread create fail\n");
			mail_alarm_tid=0;
		} else
			printf("mail alarm thread create sucess\n");
	}


	pthread_mutex_lock(&threadcfg.threadcfglock);
	if(strncmp(threadcfg.record_mode,"no_record",strlen("no_record")) ==0)
		record_mode = 0;
	else if(strncmp(threadcfg.record_mode,"normal",strlen("normal")) ==0){
		record_mode =1;
		usec_between_image=(unsigned long long )1000000/threadcfg.xfps;
		memcpy(record_resolution,threadcfg.resolution,32);
	}
	else {
		record_mode = 2;
		record_fast_duration = threadcfg.record_fast_duration;
		record_slow_speed = threadcfg.record_slow_speed;
		record_fast_speed = threadcfg.record_fast_speed;
		usec_between_image = (unsigned long long )1000000/record_fast_speed;
		memcpy(record_slow_resolution,threadcfg.record_slow_resolution,32);
		memcpy(record_fast_resolution,threadcfg.record_fast_resolution,32);
	}
	framerate = threadcfg.xfps;
	email_alarm = threadcfg.email_alarm;
	sensitivity_index = threadcfg.record_sensitivity;
	pthread_mutex_unlock(&threadcfg.threadcfglock);
	
	
			
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


	gettimeofday(&starttime,NULL);
	while(1) {

		
		gettimeofday(&alive_curr_time,NULL);
		if(alive_curr_time.tv_sec -alive_old_time.tv_sec>=(DAEMON_REBOOT_TIME_MAX>>1)){
			ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
			if(ret == -1){
				dbg("send daemon message error\n");
				exit(0);
			}
			memcpy(&alive_old_time,&alive_curr_time,sizeof(struct timeval));
		}


		
		buffer = get_data(&size);
		if(record_mode == 2&&pictures_to_write>0&&abs(size-size0)>sensitivity_diff_size[sensitivity_index])
			pictures_to_write = record_fast_speed * record_fast_duration;

		prev_usec_between_image =usec_between_image;
		switch(record_mode){
			case 0:/*not record*/
				pictures_to_write= 0;
				break;
			case 1:/*normal*/
			{
				if(abs(size-size0)>sensitivity_diff_size[sensitivity_index]){
					usec_between_image = (unsigned long long )1000000/framerate;
					gettimeofday(&endtime,NULL);
					timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
					if(timeuse >= usec_between_image){
						pictures_to_write= 1;
						memcpy(&starttime,&endtime,sizeof(struct timeval));
					}else
						pictures_to_write = 0;
				}else{
					pictures_to_write= 0;
				}
				break;
			}
			case 2:/*inteligent*/
			{
				if(pictures_to_write<=0){
					if(abs(size-size0)>sensitivity_diff_size[sensitivity_index]){
						usec_between_image = (unsigned long long )1000000/record_fast_speed;
						memcpy(record_resolution,record_fast_resolution,32);
						pictures_to_write = record_fast_speed * record_fast_duration;
						memset(&starttime,0,sizeof(struct timeval)); /*write right now!*/
					}else{
						usec_between_image = (unsigned long long ) 1000000/record_slow_speed;
						memcpy(record_resolution,record_fast_resolution,32);
						pictures_to_write =1;
					}
					gettimeofday(&endtime,NULL);
					timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
					if(timeuse>=usec_between_image){
						memcpy(&starttime,&endtime,sizeof(struct timeval));
					}else{
						pictures_to_write = 0;
					}
				}else{ 
					gettimeofday(&endtime,NULL);
					timeuse=(unsigned long long)1000000 *abs ( endtime.tv_sec - starttime.tv_sec ) + endtime.tv_usec - starttime.tv_usec;
					if(timeuse>=usec_between_image){
						memcpy(&starttime,&endtime,sizeof(struct timeval));
					}else{
						size0 = size;
						free(buffer);
						usleep(1000);
						continue;
					}
				}
				break;
			}
			default:/*something wrong*/
				exit(0);
		}

		if(pictures_to_write<=0){
			size0 = size;
			free(buffer);
			usleep(1000);
			continue;
		}
		
		pictures_to_write --;

		
		if(email_alarm&&mail_alarm_tid){
			char *image=malloc(size);
			if(image){
				memcpy(image,buffer,size);
				add_image_to_mail_attatch_list_no_block( image,  size);
			}
		}

		
retry:
		ret = nand_write(buffer, size);
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
			sprintf(FrameRateUs,"%08llu",prev_usec_between_image);
			memcpy(&record_header.FrameWidth,&swidth,4);
			memcpy(&record_header.FrameHeight,&sheight,4);
			memcpy(&record_header.FrameRateUs,&FrameRateUs,8);
			nand_write_start_header(&record_header);
			goto retry;
		}
		else if( ret == VS_MESSAGE_NEED_END_HEADER ){
			nand_prepare_close_record_header(&record_header);
			nand_write_end_header(&record_header);
			goto retry;
		}
		else{
			printf("write nand error\n");
		}
		
		if(strncmp(record_resolution,"qvga",4) ==0){
			change_resolution = 0;
			width = 640;
			height = 480;
		}
		else if(strncmp(record_resolution,"vga",3) ==0){
			change_resolution =1;
			width = 320;
			height = 240;
		}
		else {
			change_resolution =2;
			width = 1280;
			height = 720;
		}
		if(curr_resolution!=change_resolution){
			vdin_camera->width = width;
			vdin_camera->height = height;
			memset (&vdin_camera->fmt, 0, sizeof (struct v4l2_format));
			vdin_camera->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vdin_camera->fmt.fmt.pix.width = vdin_camera->width;
			vdin_camera->fmt.fmt.pix.height = vdin_camera->height;
			vdin_camera->fmt.fmt.pix.pixelformat = vdin_camera->formatIn;
			vdin_camera->fmt.fmt.pix.field = V4L2_FIELD_ANY;
			ret = ioctl (vdin_camera->fd, VIDIOC_S_FMT, &vdin_camera->fmt);
			if (ret < 0) {
				fprintf (stderr, "Unable to set format: %d.\n", errno);
				exit(-1);
			}
			if ((vdin_camera->fmt.fmt.pix.width != vdin_camera->width) ||
				(vdin_camera->fmt.fmt.pix.height != vdin_camera->height)) {
				fprintf (stderr, " format asked unavailable get width %d height %d \n",
						 vdin_camera->fmt.fmt.pix.width, vdin_camera->fmt.fmt.pix.height);
				vdin_camera->width = vdin_camera->fmt.fmt.pix.width;
				vdin_camera->height = vdin_camera->fmt.fmt.pix.height;
				/* look the format is not part of the deal ??? */
				//vdin_camera->formatIn = vdin_camera->fmt.fmt.pix.pixelformat;
			}
			curr_resolution = change_resolution;
		}
		free(buffer);
	}

	dbg("Exitting record server");

	return 0;
}

