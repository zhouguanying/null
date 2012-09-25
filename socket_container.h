#ifndef __SOCKET_CONTAINER_H__
#define __SOCKET_CONTAINER_H__

#ifdef  __cplusplus
extern "C" {
#endif
#include <sys/time.h>
#include "server.h"

enum  SOCKET_TYPE_T  {TCP_SOCKET =0, UDT_SOCKET , UDP_SOCKET , NON_SOCKET};
enum  SOCKET_CAP_T {CAP_CMD =0 ,CAP_VIDEO ,CAP_AUDIO};
typedef enum SOCKET_TYPE_T  SOCKET_TYPE;
typedef enum SOCKET_CAP_T   SOCKET_CAP;

struct socket_container{
	struct timeval create_tv;
	unsigned long long who;
	int cmd_socket;
	int video_socket;
	int audio_socket;
	SOCKET_TYPE cmd_st;
	SOCKET_TYPE video_st;
	SOCKET_TYPE audio_st;
	struct socket_container * next;
	pthread_cond_t  *cready;
	char video_socket_is_lan;
	char close_all;
	char connected;
};

typedef struct cmd_socket{
	int socket;
	SOCKET_TYPE  type;
}cmd_socket_t;

void init_socket_container_list();
void close_socket(SOCKET_TYPE st , int socket);
void get_cmd_socket(cmd_socket_t*fds , int *nums);
void check_cmd_socket();
int scl_add_socket(unsigned long long who , int socket , SOCKET_CAP cap,SOCKET_TYPE st , char is_lan);
struct socket_container *get_socket_container(int cmdsocket , int waitsocket);
int  close_socket_container(struct socket_container *sc);
void clean_socket_container(unsigned long long who , int need_lock);

#ifdef  __cplusplus
}
#endif

#endif
