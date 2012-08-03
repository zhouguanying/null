#ifndef _SERVER_H_
#define _SERVER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>

#include "includes.h"
#include "socket_container.h"

//#include "list.h"
//#include "vpu_server.h"

#define USE_CLI

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

struct sess_ctx {
	u8 *                    name;	
	 int 			    id;
        int                     s1;  //cmd socket  it is udt socket
        int                     s2;  //video socket it is tcp or udt socket
	 int 			     s3; //audio socket it is tcp or udt socket
        int                     s1_type;
	 // int 			    debugsocket;
        struct sockaddr_in *    myaddr;
        struct sockaddr_in *    to;
        struct sockaddr_un *    to_un;
        struct sockaddr_in      from;
	 pthread_t tid;
	 pthread_t srtid;
	 pthread_t swtid;
	 pthread_mutex_t		sesslock;
	 int ucount;
        struct runtime {
                char *              cfgfile;
                FILE *              fp;
                u32                 nbytes;
                struct tv_params *  tv;
                u32                 curr_frame_idx;
                void *              params; 
                void *              sess;
        } video, audio;
        int     daemon;
        int     connected;
        void *  rtp_sess;
        int     use_rtp;
        int     recording;
        int     playing;
        int     paused;
#ifdef USE_CLI
        struct cli_sess_ctx *   cli_sess;
#endif /* USE_CLI */
        struct sock_ctx *   sock_ctx;
        int     pipe_fd; 
        char *  pipe_name;
        int     is_tcp;
        int     is_rtp;
        int     is_udp;
        int     is_pipe;
        int     running;
        int     soft_reset;
        int     is_file;
        char *  file_name;
        int     file_fd;
        int     motion_detected;
	 int     haveconnected;
	struct sess_ctx * next;
	 struct socket_container *sc;
};

//save the config of server,read it from video.cfg
 struct threadconfig
 {
	pthread_mutex_t threadcfglock;
	char pswd[128];
	unsigned int 		cam_id;
	int 		sdcard_exist;
	char     	name[64];
	//char    	password[64];
	char 	server_addr[64];
	char 	monitor_mode[64];
	volatile int framerate;				//frame rate
	char compression[64]; 
	char resolution[64];  //vga qvga
	//volatile int width;
	//volatile int height;
	volatile int gop;
	volatile int rotation_angle;
	volatile int output_ratio;
	volatile int mirror_angle;
	volatile int bitrate;
	volatile int brightness;
	volatile int contrast;
	volatile int volume;
	volatile int saturation;
	volatile int gain;
	char     record_mode[64];
	char     record_resolution[64];
	char     original_resolution[64];
	//int 	    qvga_flag;
	volatile int record_sensitivity;
	volatile int record_normal_speed;
	volatile int record_normal_duration;
	volatile int record_slow_speed;
	//char	    record_slow_resolution[64];
	volatile int record_fast_speed;
	//char     record_fast_resolution[64];
	volatile int record_fast_duration;
	volatile int email_alarm;
	char     mailbox[64];
	volatile int sound_duplex;
	char     inet_mode[64];
	volatile int inet_udhcpc;
};
/* Housekeeping globals used termintate program */
extern struct sess_ctx *global_ctx; // EJA
extern struct sess_ctx *global_ctx_running_list;
extern int 			 currconnections;
//extern int 			 globalsocket;
extern pthread_mutex_t  global_ctx_lock;

extern struct threadconfig threadcfg;

//if some unrecoverable system error is occured, use it to reset whole system
extern void force_reset_v2ipd(void);

struct sess_ctx *new_system_session(char *name);
int free_system_session(struct sess_ctx *sess);
void  add_sess(struct sess_ctx *sess);
void  del_sess(struct sess_ctx *sess);
void take_sess_up(struct sess_ctx *sess);
void take_sess_down(struct sess_ctx *sess);


char * get_video_data(int *size);
int start_video_monitor(struct sess_ctx* sess);
int start_video_record(struct sess_ctx* sess);
int do_net_update(void *arg);
void init_g_sess_id_mask();
int get_sess_id();
void put_sess_id(int index);

void init_sleep_time();
void increase_video_thread_sleep_time();
void handle_video_thread();
void set_do_update();
int is_do_update();
void set_msg_do_update();
void prepare_do_update();

unsigned char checksum(unsigned char cksum, unsigned char *data, int size);
#ifdef __cplusplus
}
#endif

#endif


