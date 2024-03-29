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
#include "server.h"
    extern pthread_mutex_t list_lock;
#define PLAYBACK_NOT_ADDED -1

#define PLAYBACK_STATUS_OFFLINE 0
#define PLAYBACK_STATUS_RUNNING 1
#define PLAYBACK_STATUS_PAUSED 2
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
        struct sess_ctx *sess;
    } playback_t;
#endif

int playback_init();
int playback_new(struct sockaddr_in address, int file, int seek_percent);
playback_t* playback_find(struct sockaddr_in address);
int playback_connect(struct sockaddr_in address, int socket);
int playback_exit(struct sockaddr_in address);
int playback_seekto(struct sockaddr_in address, int percent);
void playback_remove_dead();
int playback_get_status(playback_t * pb);
int playback_set_status(playback_t * pb, int status);
int cmd_playback_set_status(struct sockaddr_in address ,  int status , void *value);
void* playback_thread(void * arg);
void playback_set_dead(playback_t* pb);

#ifdef __cplusplus
}
#endif

#endif

