#ifndef PLAYBACK_H_
#define PLAYBACK_H_
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
#include <fcntl.h>

#include "list.h"
#include "rtp.h"

#define PLAYBACK_NOT_ADDED -1

#define PLAYBACK_STATUS_OFFLINE 0
#define PLAYBACK_STATUS_RUNNING 2
#define PLAYBACK_STATUS_PAUSED 3
#define PLAYBACK_STATUS_SEEK 3
#define PLAYBACK_STATUS_EXIT 4
#ifndef playback_t
typedef struct
{
	struct list_head list;
	int status; //running, pause, exit.
	int file;
	int seek; //0-100
	struct sockaddr_in address;
	int socket;
	pthread_t thread_id;
	pthread_mutex_t lock;
	int dead;
	uint16_t rtpport;
	uint16_t destrtpport;
	uint16_t destrtcpport;
	uint32_t destaddr;
}playback_t;
#endif

//int playback_new(struct sockaddr_in address, int file);
int playback_new(struct sockaddr_in address, int file, int seek_percent);
//int playback_seekto(playback_t* pb, int percent);
int playback_connect(struct sockaddr_in address, int socket);
int playback_exit(struct sockaddr_in address);
int playback_seekto(struct sockaddr_in address, int percent);
void playback_remove_dead();
uint16_t get_playback_port();
//int set_keep_alive_data(uint32_t who,uint32_t monitor_ip,uint16_t monitor_pb_port);
int put_playback_port(uint16_t port);
//int add_to_keep_alive_list(struct pb_port_connet_management *new_kalive);
//struct pb_port_connet_management * remove_from_keep_alive_list(uint32_t addr);
//int touch_playback_port(uint16_t localport,uint32_t ip,uint16_t port);
//int keep_playback_port_alive_thread();
int start_playback_session(playback_t*pb);

#ifdef __cplusplus
}
#endif

#endif

