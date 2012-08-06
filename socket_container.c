#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socket_container.h"
#include "udttools.h"
#include "cli.h"

#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %d:%s: " fmt, __LINE__,__func__, ## args); \
    } while (0)

pthread_mutex_t   container_list_lock;
struct socket_container *socket_clist = NULL;

void init_socket_container_list()
{
	pthread_mutex_init(&container_list_lock , NULL);
	socket_clist = NULL;
}

void close_socket(SOCKET_TYPE st , int socket)
{
	switch (st){
		case TCP_SOCKET:
		case UDP_SOCKET:
			close(socket);
			break;
		case UDT_SOCKET:
			udt_close(socket);
			break;
		default:
			dbg("BUG::get unkonw socket type , something wrong\n");
	}
}

void wait_socket(struct socket_container *sc)
{
	struct timeval tv;
	struct timespec ts;
	if(sc->cready){
		dbg("BUG the condition have already be built something wrong\n");
		exit(0);
	}
	sc->cready = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if(!(sc->cready)){
		dbg("error malloc buffer for sc condition lock\n");
		system("reboot &");
		exit(0);
	}
	pthread_cond_init(sc->cready , NULL);
	gettimeofday(&tv  , NULL);
	ts.tv_sec = tv.tv_sec +20;
	ts.tv_nsec = tv.tv_usec *1000;
	pthread_cond_timedwait(sc->cready , &container_list_lock , &ts);
	pthread_cond_destroy(sc->cready);
	free(sc->cready);
	sc->cready = NULL;
}

void get_cmd_socket(int *fds , int *nums)
{
	struct socket_container *p;
	int i;
	i = 0;
	pthread_mutex_lock(&container_list_lock);
	p = socket_clist;
	while(p!=NULL){
		if(p->cmd_socket>=0){
			if(p->cmd_st !=UDT_SOCKET){
				dbg("the cmd socket is not udt socket?\n");
				exit(0);
			}
			fds[i] = p->cmd_socket;
			i++;
		}
		p = p->next;
	}
	pthread_mutex_unlock(&container_list_lock);
	*nums = i;
}
void check_cmd_socket()
{
	struct socket_container **sc;
	struct socket_container *p;
	struct timeval now;
	gettimeofday(&now , NULL);
	pthread_mutex_lock(&container_list_lock);
	sc = &socket_clist;
	while(*sc!=NULL){
		if((*sc)->cmd_socket >=0 ){
			if((*sc)->cmd_st !=UDT_SOCKET){
				dbg("the cmd socket is not udt socket?\n");
				exit(0);
			}
			if(udt_socket_ok((*sc)->cmd_socket)<0){
				dbg("socket error close it now\n");
				p=*sc;
				*sc = (*sc)->next;
				udt_close(p->cmd_socket);
				
				if(p->audio_socket>=0)
					close_socket(p->audio_st, p->audio_socket);
				if(p->video_socket>=0)
					close_socket(p->video_st, p->video_socket);
				free(p);
				continue;
			}
		}
		else if(now.tv_sec - (*sc)->create_tv.tv_sec >=60){
			p = *sc;
			*sc = (*sc)->next;
			if(p->audio_socket>=0)
				close_socket(p->audio_st , p->audio_socket);
			if(p->video_socket>=0)
				close_socket(p->video_st , p->video_socket);
			free(p);
			continue;
		}
		sc = &((*sc)->next);
	}
	pthread_mutex_unlock(&container_list_lock);
}

/*
int  new_socket_container(int who)
{
	struct socket_container*sc;
	sc = (struct socket_container *)malloc(sizeof(struct socket_container));
	if(!sc) return -1;
	gettimeofday(&(sc->create_tv) , NULL);
	sc ->who = who;
	sc->cmd_socket = -1;
	sc->video_socket = -1;
	sc ->audio_socket = -1;
	sc->cmd_st = NON_SOCKET;
	sc ->video_st = NON_SOCKET;
	sc->audio_st = NON_SOCKET;
	pthread_mutex_lock(&container_list_lock);
	sc->next = socket_clist;
	socket_clist = sc;
	pthread_mutex_unlock(&container_list_lock);
	return 0;
}
*/
int scl_add_socket(unsigned long long who , int socket , SOCKET_CAP cap,SOCKET_TYPE st)
{
	struct socket_container **scp;
	struct socket_container *p;
	struct timeval now;
	int finded , ret;
	if(socket <0)
		return -1;
	gettimeofday(&now , NULL);
	finded = 0;
	ret = -1;
	dbg("add socket  cap = %d , who = %llu\n",cap ,who);
	pthread_mutex_lock(&container_list_lock);
	scp = &socket_clist;
	while(*scp!=NULL){
		if((*scp)->who == who){
			switch(cap){
				case CAP_CMD:
					if((*scp)->cmd_socket>=0){
						dbg("the socket have already exist? something strange\n");
						goto FREE_CONTAINER;
					}
					if(st !=UDT_SOCKET){
						dbg("BUG:: cmd socket is not udt socket\n");
						exit(0);
					}
					(*scp)->cmd_socket = socket;
					(*scp)->cmd_st =st;
					break;
				case CAP_VIDEO:
					if((*scp)->video_socket>=0){
						dbg("the socket have already exist? something strange\n");
						goto FREE_CONTAINER;
					}
					(*scp)->video_socket = socket;
					(*scp)->video_st = st;
					break;
				case CAP_AUDIO:
					if((*scp)->audio_socket>=0 ){
						dbg("the socket have already exist? something strange\n");
						goto FREE_CONTAINER;
					}
					(*scp)->audio_socket = socket;
					(*scp)->audio_st = st;
					break;
				default:
					dbg("########add unkown socket to list###########\n");
					exit(0);
			}
			if((*scp)->cready&&(*scp)->video_socket>=0&&(*scp)->audio_socket>=0){
				dbg("some one wait for sockets tell it\n");
				pthread_cond_signal((*scp)->cready);
			}
			gettimeofday(&((*scp)->create_tv),NULL);
			finded ++;
			ret = 0;
		}
		else if( (now.tv_sec - (*scp)->create_tv.tv_sec >=60)&&
			((*scp)->cmd_socket<0||(*scp)->video_socket<0||(*scp)->audio_socket<0)){
FREE_CONTAINER:
			p=*scp;
			*scp = (*scp)->next;
			if(p->cmd_socket >=0)
				close_socket(p->cmd_st, p->cmd_socket);
			if(p->video_socket>=0)
				close_socket(p->video_st, p->video_socket);
			if(p->audio_socket>=0)
				close_socket(p->audio_st , p->audio_socket);
			free(p);
			continue;
		}
		scp = &((*scp)->next);
	}
	switch (finded){
		case 0:
			p = (struct socket_container *)malloc(sizeof(struct socket_container));
			if(!p) break;
			memset(p, 0 , sizeof(struct socket_container));
			gettimeofday(&(p->create_tv) , NULL);
			p ->who = who;
			p->cmd_socket = -1;
			p->video_socket = -1;
			p ->audio_socket = -1;
			p->cmd_st = NON_SOCKET;
			p ->video_st = NON_SOCKET;
			p->audio_st = NON_SOCKET;

			switch(cap){
				case CAP_CMD:
					p->cmd_socket = socket;
					p->cmd_st =st;
					break;
				case CAP_VIDEO:
					p->video_socket = socket;
					p->video_st = st;
					break;
				case CAP_AUDIO:
					p->audio_socket = socket;
					p->audio_st = st;
					break;
				default:
					dbg("add unkown socket to list\n");
					exit(0);
			}
			if(p){
				p->next = socket_clist;
				socket_clist = p;
				ret = 0;
			}else{
				ret = -1;
			}
			break;
		case 1:
			break;
		default:
			dbg("#########we found collision monitor id for safe we reboot now############\n");
			system("reboot &");
			exit(0);
	}
	pthread_mutex_unlock(&container_list_lock);
	if(ret <0){
		dbg("##############BUG::cannot add socket to list?###############\n");
		close_socket(st , socket);
	}
	return ret;
}

int  close_socket_container(struct socket_container *sc)
{
	struct socket_container **scc;
	int ret;
	if(!sc)return -1;
	ret = -1;
	pthread_mutex_lock(&container_list_lock);
	scc = &socket_clist;
	while(*scc!=NULL){
		if(*scc ==sc){
			*scc = (*scc)->next;
			if(sc->cmd_socket>=0)
				close_socket(sc->cmd_st , sc->cmd_socket);
			if(sc->video_socket>=0)
				close_socket(sc->video_st , sc->video_socket);
			if(sc->audio_socket>=0)
				close_socket(sc->audio_st , sc->audio_socket);
			free(sc);
			ret = 0;
			break;
		}
		scc = &((*scc)->next);
	}
	pthread_mutex_unlock(&container_list_lock);
	return ret;
}

struct socket_container *get_socket_container(int cmdsocket)
{
	
	struct socket_container *sc;
	pthread_mutex_lock(&container_list_lock);
	sc = socket_clist;
	while(sc!=NULL){
		if(sc->cmd_socket == cmdsocket){
			break;
		}
		sc = sc->next;
	}
	if(sc&&(sc->audio_socket<0||sc->video_socket<0)){
		dbg("the socket is not built all  now wait for it\n");
		wait_socket(sc);
	}
	pthread_mutex_unlock(&container_list_lock);
	return sc;
}




