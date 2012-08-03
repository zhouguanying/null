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
};

void init_socket_container_list();
void close_socket(SOCKET_TYPE st , int socket);
void get_cmd_socket(int *fds , int *nums);
void check_cmd_socket();
int scl_add_socket(unsigned long long  who , int socket , SOCKET_CAP cap,SOCKET_TYPE st);
struct socket_container *get_socket_container(int cmdsocket);

#ifdef  __cplusplus
}
#endif

#endif
