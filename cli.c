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
#include <sys/statfs.h>

#include "includes.h"
//#include <defines.h>

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
#include "sound.h"
//#include "udp_transfer.h"
#include "video_cfg.h"
#include "v4l2uvc.h"
#include "amixer.h"
#include "udttools.h"
#include "socket_container.h"
//#include "monitor.h"

/* Debug */
#define ENCODER_DBG
#ifdef ENCODER_DBG
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif

struct cli_sess_ctx *g_cli_ctx = NULL; /* Single session only */

static struct cli_handler cli_cmd_handler;

static int cli_socket = -1; /*it must be udt socket*/

/*
struct  UDT_SELECT_SET{
	int array[MAX_CONNECTIONS];
	int fdnums;
	pthread_mutex_t setlock;
};

static struct UDT_SELECT_SET  fdset;

void init_udt_fdset()
{
	int i;
	for(i = 0 ; i<MAX_CONNECTIONS ; i++)
		fdset.array[i] = -1;
	fdset.fdnums = 0;
	pthread_mutex_init(&fdset.setlock , NULL);
}

int  udt_add_socket_to_set(int sockfd)
{
	int ret,i;
	if(sockfd <0)
		return -1;
	ret = -1;
	pthread_mutex_lock(&fdset.setlock);
	if(fdset.fdnums<MAX_CONNECTIONS){
		for(i = 0 ; i<MAX_CONNECTIONS; i++){
			if(fdset.array[i]<0){
				fdset.array[i] = sockfd;
				fdset.fdnums ++;
				ret = 0;
				break;
			}
		}
	}
	pthread_mutex_unlock(&fdset.setlock);
	return ret;
}

void udt_del_socket_from_set(int sockfd)
{
	int  i ;
	if(sockfd <0)
		return;
	pthread_mutex_lock(&fdset.setlock);
	if(fdset.fdnums>0){
		for(i = 0; i<MAX_CONNECTIONS ; i++){
			if(fdset.array[i] == sockfd){
				fdset.array[i] = -1;
				fdset.fdnums --;
				break;
			}
		}
	}
	pthread_mutex_unlock(&fdset.setlock);
}

void copy_udt_socket_set(int *array , int *nums)
{
	int i,j;
	pthread_mutex_lock(&fdset.setlock);
	for(i = 0, j=0;i<MAX_CONNECTIONS ; i++){
		if(fdset.array[i]>=0){
			array[j] = fdset.array[i];
			j++;
		}
	}
	pthread_mutex_unlock(&fdset.setlock);
	*nums = j;
}

void check_udt_fdset()
{
	int i;
	pthread_mutex_lock(&fdset.setlock);
	for(i = 0 ; i < MAX_CONNECTIONS ; i++){
		if(fdset.array[i] > 0){
			if(udt_socket_ok(fdset.array[i])<0){
				udt_close(fdset.array[i]);
				fdset.array[i] = -1;
				fdset.fdnums --;
			}
		}
	}
	pthread_mutex_unlock(&fdset.setlock);
}

*/
static struct cli_sess_ctx * new_session(void *arg)
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
	
	//dbg("0x%08X removed successfully", (u32) sess);
	return 0;
}

extern char *do_cli_cmd_bin(void *sess, char *cmd, int cmd_len, int size, int* rsp_len);
static int save_config_value(char *name , char *value)
{
	FILE*fp;
	char *buf;
	char *p;
	int pos;
	fp = fopen(RECORD_PAR_FILE , "r");
	if(!fp){
		dbg("open config file error\n");
		return -1;
	}
	buf = (char *)malloc(4096);
	if(!buf){
		dbg("malloc buf error\n");
		fclose(fp);
		return -1;
	}
	memset(buf,0,4096);
	pos = 0;
	while(fgets(buf+pos,4096-pos,fp)!=NULL){
		p = buf+pos;
		if(strncmp(p,name,strlen(name))==0){
			memset(p,0,strlen(p));
			sprintf(p,"%s=%s\n",name,value);
		}
		pos+=strlen(p);
	}
	fclose(fp);
	usleep(1000);
	fp = fopen(RECORD_PAR_FILE,"w");
	if(!fp){
		dbg("error open config file\n");
		free(buf);
		return -1;
	}
	fwrite(buf,1,pos,fp);
	fclose(fp);
	dbg("write config file ok\n");
	return 0;
}

char *set_transport_type_rsp(struct sess_ctx * sess ,int *size)
{
	char *buf;
	if(!sess)
		return NULL;
	buf = (char *)malloc(6);
	if(!buf)
		return NULL;
	if(sess->is_tcp)
	{
		sprintf(buf , "tcp");
	}
	else if(sess->is_rtp)
	{
		sprintf(buf , "rtp");
	}
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
                                 ssize_t req_len, u8 *unused, int* rsp_len,struct sockaddr_in from)
{
	#define N_ARGS 2    
    struct cli_handler *p;
    struct sess_ctx * tmp;
    char cmd[1024];
 //   char *delims = ":";
    char *argv[N_ARGS]; /* Command + param */
    int i;
    char* ptr;
    char * rsp;

	if( strncmp((char*)req, "UpdateSystem", 12) == 0 ){
		//sess->from = from;
		return do_cli_cmd_bin(NULL, (char*)req, req_len, (int)NULL, rsp_len);
	}

    if (sess->cmd == NULL) {
        dbg("Error parsing command table - handler not installed");
        return 0;
    }

    /* Lookup command handler */
    p = sess->cmd;

    /* Decompose commands string */
    i = 0;
    /* Save and strip extra chars */
    memset(cmd, 0, sizeof(cmd));

	// TODO:  so strange code, can it run ok before?
    if (req[req_len - 1] != '\0');{
	dbg("##################TODO#################\n");
        req_len -= 1;
    }
		
    memcpy(cmd, req, req_len);
#if 0	
    argv[i] = strtok(cmd, delims);
	while (argv[i] != NULL && i < N_ARGS-1)
		argv[++i] = strtok(NULL, delims);
#else
	argv[0] = argv[1] = 0;
	ptr = cmd;
	i = req_len;
	while(i--){
		if( *ptr != ' ' ){
			argv[0] = ptr;
			break;
		}
		ptr++;
	}
	while(i--){
		if( *ptr == ':' ){
			ptr++;
			argv[1] = ptr;
			break;
		}
		ptr++;
	}
#endif
    /* Pass command and param back to processer */
   printf("argv[0]==%s\n",argv[0]);
   printf("argv[1]==%s\n",argv[1]);
    if (p->handler != NULL){
		pthread_mutex_lock(&global_ctx_lock);
		sess->from = from;
		tmp=(struct sess_ctx*)sess->arg;
		if(tmp!=NULL&&tmp->from.sin_addr.s_addr==sess->from.sin_addr.s_addr&&tmp->from.sin_port ==sess->from.sin_port){
			printf("#######################found session in cache#########################\n");
			if (strncmp(argv[0], "set_transport_type", 18) == 0){
				if(tmp->running){
					pthread_mutex_unlock(&global_ctx_lock);
					dbg("set_transport_type session already running\n");
					return set_transport_type_rsp(tmp, rsp_len);
				}else{
					memset(&tmp->from,0,sizeof(struct sockaddr_in));
					goto NEW_SESSION;
				}
			}
			goto done;
		}
		tmp=global_ctx_running_list;
		while(tmp!=NULL){
			if(tmp->from.sin_addr.s_addr==sess->from.sin_addr.s_addr&&tmp->from.sin_port ==sess->from.sin_port){
				if (strncmp(argv[0], "set_transport_type", 18) == 0){
					if(tmp->running){
						pthread_mutex_unlock(&global_ctx_lock);
						dbg("set_transport_type session already running\n");
						return set_transport_type_rsp(tmp, rsp_len);
					}else{
						memset(&tmp->from,0,sizeof(struct sockaddr_in));
						goto NEW_SESSION;
					}
				}
				sess->arg=tmp;
				printf("###############ok find runnig sess,now go to do cmd#########################\n");
				goto done;
			}
			tmp=tmp->next;
		}
		 if (strncmp(argv[0], "set_transport_type", 18) == 0){
		 	if(currconnections>=MAX_CONNECTIONS){
				pthread_mutex_unlock(&global_ctx_lock);
				return  strdup("connected max ");//should return a string report max connections
			}
NEW_SESSION:
		//	printf("before new_system_session\n");
		 	tmp= new_system_session("ipcam");
			if(!tmp){
				pthread_mutex_unlock(&global_ctx_lock);
				printf("********************can't create session******************\n");
				return NULL;
			}
			//dbg("################sess->id = %d#################\n",tmp->id);
			memcpy(&tmp->from,&sess->from,sizeof(struct sockaddr_in));
			printf("#########################new_system_session###############################\n");
			sess->arg=tmp;
			//dbg("################sess->id = %d#################\n",tmp->id);
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
		 else if(strncmp(argv[0],"search_wifi",11)==0)
		 	goto done;
		 else if(strncmp(argv[0],"set_pswd",8)==0)
		 	goto done;
		 else if(strncmp(argv[0],"check_pswd",10)==0)
		 	goto done;
		 else if(strncmp(argv[0],"pswd_state",10)==0)
		 	goto done;
		 else if (strncmp(argv[0], "SetTime", 7) == 0)
              	goto done;
       	 else if (strncmp(argv[0], "GetTime", 7) == 0)
           		goto done;
		  else if (strncmp(argv[0], "DeleteAllFiles", 14) == 0)
                	goto done;
       	 else if (strncmp(argv[0], "DeleteFile", 10) == 0)
                	goto done;
		 pthread_mutex_unlock(&global_ctx_lock);
		 return NULL;
done:
		dbg("ready to do cli, cmd=%s\n", argv[0]);
		rsp= p->handler(sess->arg, argv[0], argv[1], i+1, rsp_len);
		pthread_mutex_unlock(&global_ctx_lock);
		return rsp;
	}

    dbg("error");
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
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
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
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
		system("reboot &");
		exit(0);
	}
}

static int do_cli(struct cli_sess_ctx *sess)
{
#define  CLI_BUF_SIZE  1024
	int uset[64];
	int fdnums;
	int sockfd;
	char req[CLI_BUF_SIZE];
	char *rsp;
	int rsp_len;
	struct timeval tv , now , last_alive_time;
	struct sockaddr_in from;
	int fromlen;
	int req_len;
	int s,ret;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	init_socket_container_list();
	do_cli_start();
	gettimeofday(&last_alive_time , NULL);
LOOP_START:
	for(;;){
		for(;;){
			get_cmd_socket(uset, &fdnums);
			if(fdnums >0)
				break;
			sleep(3);
			do_cli_alive();
			gettimeofday(&last_alive_time , NULL);
		}
		
		//dbg("get socket num = %d\n",fdnums);
		sockfd = udt_get_readable_socket(uset, fdnums,  &tv);
		if(sockfd <0){
			//dbg("select error\n");
			do_cli_alive();
			gettimeofday(&last_alive_time , NULL);
			check_cmd_socket();
			sleep(1);
			goto LOOP_START;
		}
		dbg("select ok now begin recv\n");
		
		//sockfd = uset[0];
		dbg("get ready sockfd = %d\n",sockfd);
		req_len = stun_recvmsg(sockfd ,req, CLI_BUF_SIZE , &from , &fromlen);
		if(req_len <0){
			dbg("udt select ok but cannot recv message? something wrong\n");
			do_cli_alive();
			gettimeofday(&last_alive_time , NULL);
			check_cmd_socket();
			sleep(1);
			goto LOOP_START;
		}
		dbg("recv ok\n");
		gettimeofday(&now , NULL);
		if(now.tv_sec - last_alive_time.tv_sec >=3){
			do_cli_alive();
			last_alive_time = now;
		}
		req[req_len] = 0;
		dbg("get cli cmd: %s ip ==%s  ,  port ==%d\n",req , inet_ntoa(from.sin_addr) , ntohs(from.sin_port));
		if(is_do_update()){
			dbg("is do update exit now\n");
			for(;;){
				do_cli_alive();
				sleep(3);
			}
		}
		rsp_len = 0;
		cli_socket = sockfd;
		rsp = handle_cli_request(sess, req, req_len, NULL, &rsp_len,from);
		dbg("rsp=%s , rsp_len =%d\n",rsp ,rsp_len);
		if (rsp != NULL) { /* command ok so send response */
		//dbg("sent rsp");
			fromlen = sizeof(struct sockaddr_in);
			if( rsp_len ){
				//1  it seems never occur
				//printf("enter seen never happen rsp_len==%d\n",rsp_len);
				s = 0;
				while(rsp_len>0){
					if(rsp_len>1000){
						ret =stun_sendmsg(sockfd,  rsp+s , 1000);
					}else{
						ret =stun_sendmsg(sockfd,  rsp+s , rsp_len);
					}
					if(ret <0){
						dbg("udt send  something wrong\n");
						break;
					}
					s+=ret;
					rsp_len-=ret;
				}
				//printf("sendto return ==%d dst ip==%s , port ==%d\n",ret , inet_ntoa(sess->from.sin_addr), ntohs(sess->from.sin_port));
			}
			else{
				rsp_len = strlen(rsp);
				s = 0;
				while(rsp_len>0){
					if(rsp_len>1000){
						ret =stun_sendmsg(sockfd,  rsp+s , 1000);
					}else{
						ret =stun_sendmsg(sockfd, rsp+s , rsp_len);
					}
					if(ret <0){
						dbg("udt send  something wrong\n");
						break;
					}
					s+=ret;
					rsp_len-=ret;
				}
			}
			free(rsp);
		}
		check_cmd_socket();
	}
	return 0;
}
/*
static int do_cli(struct cli_sess_ctx *sess)
{
#define BUF_SZ 1024    
    u8 req[BUF_SZ];
    char *rsp;
    socklen_t fromlen;
    struct sockaddr_in from;
     struct sockaddr_in cliaddr;
	struct timeval timeout;
	struct timeval old_snd_alive_time,currtime;
    ssize_t req_len;
	int rsp_len;
	int ret = 0;
	int s;
	int on;

    //dbg("Starting CLI sid(%08X)", (u32) sess);

    
    if ((sess->sock = create_udp_socket()) < 0) {
        perror("Error creating socket");
        return -1;
    }
	on = 1;
       if (setsockopt(sess->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
              printf("Error enabling sess->sock address reuse");
        }
	      timeout.tv_sec=3;
	  timeout.tv_usec=0;
	   if (setsockopt(sess->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
              printf("Error enabling socket rcv time out \n");
        }
    if ((sess->saddr = bind_udp_socket(sess->sock, sess->port)) == NULL) {
        printf("****************************Error binding udp socket****************************");
        close(sess->sock);
	 force_reset_v2ipd();
        return -1;
    }
    //dbg("sock %d bound to port %d\n", sess->sock, sess->port);    

    sess->running = 1;
    fromlen = sizeof(struct sockaddr_in);
   do_cli_start();
   gettimeofday(&old_snd_alive_time , NULL);
	
    while (sess->running) {
		dbg("try to get cli cmd\n");
		//memset(req,0,BUF_SZ);
try_get_cmd:
        req_len = recvfrom(sess->sock, req, sizeof(req), 0, (struct sockaddr *) &from, &fromlen);
		if(is_do_update()){
			dbg("is do update exit now\n");
			for(;;){
				do_cli_alive();
				sleep(3);
			}
		}
		gettimeofday(&currtime, NULL);
		if(abs(currtime.tv_sec - old_snd_alive_time.tv_sec)>=3){
			do_cli_alive();
			memcpy(&old_snd_alive_time , &currtime , sizeof(struct timeval));
		}
		if(req_len<=0)
			goto try_get_cmd;
		req[req_len] = 0;
		dbg("get cli cmd: %s ip ==%s  ,  port ==%d\n",req , inet_ntoa(from.sin_addr) , ntohs(from.sin_port));
        if (req_len <= 0) 
            dbg("socket error\n");
		else {
			rsp_len = 0;
			rsp = handle_cli_request(sess, req, req_len, NULL, &rsp_len,from);
			dbg("rsp=%s\n",rsp);
			if (rsp != NULL) { 
			//dbg("sent rsp");
				fromlen = sizeof(struct sockaddr_in);
				if( rsp_len ){
					//1  it seems never occur
					//printf("enter seen never happen rsp_len==%d\n",rsp_len);
					s = 0;
					while(rsp_len>0){
						if(rsp_len>1000){
							ret =sendto(sess->sock, rsp+s, 1000, 0,(struct sockaddr *) &sess->from, fromlen);
							s+=1000;
							rsp_len-=1000;
						}else{
							ret =sendto(sess->sock, rsp+s, rsp_len, 0,(struct sockaddr *) &sess->from, fromlen);
							rsp_len = 0;
						}
					}
					printf("sendto return ==%d dst ip==%s , port ==%d\n",ret , inet_ntoa(sess->from.sin_addr), ntohs(sess->from.sin_port));
				}
				else{
					rsp_len = strlen(rsp);
					s = 0;
					while(rsp_len>0){
						if(rsp_len>1000){
							ret =sendto(sess->sock, rsp+s, 1000, 0,(struct sockaddr *) &sess->from, fromlen);
							s+=1000;
							rsp_len-=1000;
						}else{
							ret =sendto(sess->sock, rsp+s, rsp_len, 0,(struct sockaddr *) &sess->from, fromlen);
							rsp_len = 0;
						}
					}
					//ret = sendto(sess->sock, rsp, strlen(rsp), 0,(struct sockaddr *) &sess->from, fromlen);
					printf("send to addr ip:%s ; port: %d\n",inet_ntoa(sess->from.sin_addr),ntohs(sess->from.sin_port));
					printf("sendto return == %d\n",ret);
					fromlen = sizeof(struct sockaddr_in);
					 if(getsockname(sess->sock,(struct sockaddr*)&cliaddr,&fromlen) <0){
						 printf("cannot get sock name\n");
						close(sess->sock);
						return -1;
					}
					 printf("used cli ip:%s , port: %d\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
				}
				free(rsp);
			}
		}
    }

	dbg("Exitting CLI");
    pthread_exit(NULL);

    return 0;
}
*/

/**
 * cli_init - init cli session
 * @arg: optional arg passed as argumment to handlers
 * Returns 0 on success or -1 on error
 */
static  int check_cli_pswd(char *arg , char **r)
{
	if(!threadcfg.pswd[0]){
		*r = strdup(PASSWORD_NOT_SET);
		return -1;
	}
	while(*arg&&*arg!=':')arg++;
	if(*arg!=':'){
		*r = strdup(PASSWORD_FAIL);
		return -1;
	}
	*arg = 0;
	arg++;
	if(strlen(arg)!=strlen(threadcfg.pswd)||strncmp(arg,threadcfg.pswd,strlen(arg))!=0){
		*r = strdup(PASSWORD_FAIL);
		return -1;
	}
	return 0;
}
struct cli_sess_ctx * cli_init(void *arg)
{
    struct cli_sess_ctx *sess;   

    /* Create session */
    if (g_cli_ctx != NULL || (sess = new_session(arg)) == NULL) return NULL;

    /* Start up main loop */
    if (pthread_create(&sess->tid, NULL, (void *) do_cli, sess) < 0) {
        free_session(sess);
        return NULL;
    } else        
        return sess;
}

/**
 * cli_deinit - deinit cli session
 * @sess: cli session context
 * Returns 0 on success or -1 on error
 */
int cli_deinit(struct cli_sess_ctx *sess)
{
    return free_session(sess);
}

/**
 * cli_bind_cmds - install command/handler structure
 * @sess: cli session context
 * @handlers: cli handlers
 * Returns 0 on success or -1 on error
 */
int cli_bind_cmds(struct cli_sess_ctx *sess, struct cli_handler *cmds)
{
    if (sess == NULL || cmds == NULL)
        return -1;
    else {
        sess->cmd = cmds;
        return 0;
    }
}

/**
 * set_dest_addr - sets destination ip address
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
int set_dest_addr(struct sess_ctx *sess, char *arg)
{
        if (sess == NULL || sess->to == NULL || arg == NULL) {
                dbg("error");
                return -1;
        } else {
                if (sess->is_udp || sess->is_rtp) {
                        inet_aton(arg, &sess->to->sin_addr);
                        dbg("ip=%s (ok)", arg);
                } else
                        dbg("command only used for udp or rtp sessions");
                return 0;
        }
}

/**
 * set_dest_port - sets destination port number
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
int set_dest_port(struct sess_ctx *sess, char *arg)
{
        u16 port;
    
        if (sess == NULL || sess->to == NULL || arg == NULL) {
                dbg("error");
                return -1;
        } else {
                if (sess->is_udp || sess->is_rtp) {
                        port = (u16) strtoul(arg, NULL, 10);    
                        sess->to->sin_port = htons(port);
                        dbg("port=%d (ok)", port);
                } else
                        dbg("command only used for udp or rtp sessions");
                return 0;
        }
}

/**
 * start_recording_video - start capturing video
 * @sess: session context
 * Returns 0 on success or -1 on error
 */
int start_recording_video(struct sess_ctx *sess)
{
#if 0
        vpu_StartSession(sess->video.params);
        if (!sess->is_tcp) 
                sess->connected = 1;
        return 0;
#else
		printf("should do something, system halt at 0\n");
		while(1);
#endif
}

/**
 * stop_recording_video - stop capturing video
 * @sess: session context
 * Returns 0 on success or -1 on error
 */
int stop_recording_video(struct sess_ctx *sess)
{
        dbg("called");
        sess->paused = 1;
        return 0;
}


/**
 * start_vid - start video recording session
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
int start_vid(struct sess_ctx *sess, char *arg)
{
        if (sess != NULL && !sess->playing) {
                /* tcp session starts when client connects */
                if (sess->is_tcp && !sess->connected) return 0;
        		//nand_open();
                if (start_recording_video(sess) < 0) {
                        dbg("error");
				   //nand_close();
                        return -1;
                } else {
//                        dbg("ok");
                        sess->playing = 1;
                        return 0;
                }
        } else if (sess->paused) {
                dbg("session is resuming");
                sess->paused = 0;
//                vpu_ResyncVideo(NULL);
                return 0;
        } else {
                dbg ("error");
                return -1;
        }
}

/**
 * pause_vid - pauses video recording session
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
int pause_vid(struct sess_ctx *sess, char *arg)
{
        if (sess != NULL && sess->playing) {
                dbg("ok");
			//nand_flush();
                return stop_recording_video(sess);
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * stop_vid - stops video recording session
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
int stop_vid(struct sess_ctx *sess, char *arg)
{
        if (sess != NULL) {
                dbg("ok");
			//nand_close();
                return raise(SIGPIPE);
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * set_transport_type - sets the session transport type (tcp, udp, rtp)
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */

static char *set_transport_type(struct sess_ctx*sess , char *arg , int *rsp_len)
{
	struct socket_container *sc;
	playback_t *pb;
	char *r;
	if (sess == NULL) {
            dbg("error\n");
            return NULL;
    }
	
    if(check_cli_pswd( arg, & r)!=0){
		  g_cli_ctx->arg=NULL;
		  free_system_session(sess);
		return r;
    	}
	//dbg("after check pswd\n");
	sc = get_socket_container(cli_socket);
	if(!sc){
		dbg("error not found socket container\n");
		g_cli_ctx->arg = NULL;
		free_system_session(sess);
		return NULL;
	}
	//dbg("after get socket container sc = %p , sc->video_socket = %d , sc->audio_socket = %d\n" , sc->video_socket , sc->audio_socket);
	if(sc->video_socket<0||sc->audio_socket<0){
		dbg("error the video socket or audio socket is not build\n");
		g_cli_ctx->arg = NULL;
		free_system_session(sess);
		close_socket_container(sc);
		return NULL;
	}
	//dbg("after check audio video socket\n");
	if(sc->audio_st !=sc->video_st){
		dbg("error the audio and video socket is not the same type?\n");
		g_cli_ctx->arg = NULL;
		free_system_session(sess);
		close_socket_container(sc);
		return NULL;
	}
	//dbg("after check 
	r = malloc(6);
	if(!r){
		dbg("error malloc set_transport_type rsp buf\n");
		g_cli_ctx->arg = NULL;
		free_system_session(sess);
		close_socket_container(sc);
		return NULL;
	}
	switch (sc->video_st){
		case TCP_SOCKET:
			sprintf(r,"tcp");
			sess->is_tcp = 1;
			break;
		case UDT_SOCKET:
			sprintf(r,"rtp");
			sess->is_rtp =1;
			break;
		default:
			dbg("not tcp socket nor udt error\n");
			exit(0);
	}
	sess->sc = sc;
	if(strncmp(arg , "update",6)==0){
		if(pthread_create(&sess->tid, NULL, (void *) do_net_update, sess) < 0){
			g_cli_ctx->arg=NULL;
			free_system_session(sess);
			close_socket_container(sc);
			free(r);
			return NULL;
		}
	}else{
		playback_remove_dead();
		pthread_mutex_lock(&list_lock);
		pb = playback_find(sess->from);
		pthread_mutex_unlock(&list_lock);
		if(pb&&(playback_get_status(pb) == PLAYBACK_STATUS_OFFLINE)){
			dbg("#########playback found###########\n");
			pb->sess = sess;
			if (pthread_create(&pb->thread_id, NULL, (void *) playback_thread, pb) < 0) {
				g_cli_ctx->arg=NULL;
				playback_set_dead( pb);
				free_system_session(sess);
				close_socket_container(sc);
				free(r);
				return NULL;
			}
		}else{
			dbg("#####now start video thread#########\n");
			if (pthread_create(&sess->tid, NULL, (void *) start_video_monitor, sess) < 0) {
				g_cli_ctx->arg=NULL;
				free_system_session(sess);
				close_socket_container(sc);
				free(r);
				return NULL;
			} 
		}
	}
	
	r[3] = (char)threadcfg.brightness;
	r[4] = (char)threadcfg.contrast;
	r[5] = (char)threadcfg.volume;
	*rsp_len = 6;
	return r;
	
}

/*
static char * set_transport_type(struct sess_ctx *sess, char *arg , int *rsp_len)
{
    int on;
    char *r;
  //  struct sess_ctx*tmp;
    if (sess == NULL || arg == NULL) {
            dbg("error\n");
            return NULL;
    }
    if(check_cli_pswd( arg, & r)!=0){
		  g_cli_ctx->arg=NULL;
		  free_system_session(sess);
		return r;
    	}
	//dbg("##############sess->id=%d################\n",sess->id);
    if (strlen(arg) == 3 && strncmp(arg, "tcp", 3) == 0) {
        
		if(sess->s1<0){
      		       if ((sess->s1 = create_tcp_socket()) < 0) {
                   		  printf("Error creating socket");
				  g_cli_ctx->arg=NULL;
				  free_system_session(sess);
                 	         return NULL;
            		}
		
        
            		on = 1;
          	       if (setsockopt(sess->s1, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
                    		 printf("Error enabling socket address reuse");
            		}

          	       if ((sess->myaddr = bind_tcp_socket(sess->s1, SERVER_PORT)) == NULL){
              		  printf("Error binding socket");
				 g_cli_ctx->arg=NULL;
				  free_system_session(sess);
                		  return NULL;
            		}

            		if (listen(sess->s1, MAX_CONNECTIONS) < 0){
               		 printf("Error listening for connection");
				 g_cli_ctx->arg=NULL;
				  free_system_session(sess);
                		return NULL;
            		}
            		dbg("created tcp socket\n");
		}else{
			printf("%s: already binded.\n", __func__);
			g_cli_ctx->arg=NULL;
			free_system_session(sess);
			return NULL;
		}
		
		sess->running = 1;
		sess->is_tcp = 1;
		sess->ucount=1;
		//dbg("##############sess->id=%d################\n",sess->id);
		if (pthread_create(&sess->tid, NULL, (void *) start_video_monitor, sess) < 0) {
			g_cli_ctx->arg=NULL;
			free_system_session(sess);
			return NULL;
		} 
		dbg("start_video_monitor run now\n");
		r = (char *)malloc(6);
		if(!r)return NULL;
		sprintf(r,"tcp");
		r[3] = (char)threadcfg.brightness;
		r[4] = (char)threadcfg.contrast;
		r[5] = (char)threadcfg.volume;
		*rsp_len = 6;
		return r;
	} 
	else if(strlen(arg) == 3 && strncmp(arg, "rtp", 3) == 0){
		printf("********************rtp type*****************\n");
		sess->running=1;
		sess->is_rtp=1;
		sess->ucount=0;
		printf("cli ip ==%s\n",inet_ntoa(sess->from.sin_addr));
		printf("cli port==%d\n",ntohs(sess->from.sin_port));
		if (pthread_create(&sess->tid, NULL, (void *) udt_sess_thread, sess) < 0) {
			if((struct sess_ctx*)g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
			printf("**********create udt_sess_thread error****************\n");
			free_system_session(sess);
			return NULL;
		} 
		dbg("create udt_sess_thread sucess\n");
		r = (char *)malloc(6);
		if(!r)return NULL;
		sprintf(r,"rtp");
		r[3] = (char)threadcfg.brightness;
		r[4] = (char)threadcfg.contrast;
		r[5] = (char)threadcfg.volume;
		*rsp_len = 6;
		return r;
	}else if(strlen(arg)==6&&strncmp(arg,"update",6) == 0){
		printf("###################do update################\n");
		sess->running = 1;
		sess->ucount = 0;
		if (pthread_create(&sess->tid, NULL, (void *) udp_do_update, sess) < 0) {
			if((struct sess_ctx*)g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
			printf("**********create udt_do_update error****************\n");
			free_system_session(sess);
			return NULL;
		} 
	}else if(strlen(arg)==9&&strncmp(arg,"updatetcp",9) == 0){
		printf("###################do update tcp################\n");
		if(sess->s1<0){
      		       if ((sess->s1 = create_tcp_socket()) < 0) {
                   		  printf("Error creating socket");
				  g_cli_ctx->arg=NULL;
				  free_system_session(sess);
                 	         return NULL;
            		}
		
           
            		on = 1;
          	       if (setsockopt(sess->s1, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
                    		 printf("Error enabling socket address reuse");
            		}

          	       if ((sess->myaddr = bind_tcp_socket(sess->s1, SERVER_PORT)) == NULL){
              		  printf("Error binding socket");
				 g_cli_ctx->arg=NULL;
				  free_system_session(sess);
                		  return NULL;
            		}

            		if (listen(sess->s1, MAX_CONNECTIONS) < 0){
               		 printf("Error listening for connection");
				 g_cli_ctx->arg=NULL;
				  free_system_session(sess);
                		return NULL;
            		}
            		dbg("created tcp socket\n");
		}else{
			printf("%s: already binded.\n", __func__);
			g_cli_ctx->arg=NULL;
			free_system_session(sess);
			return NULL;
		}
		
		sess->running = 1;
		//sess->is_tcp = 1;
		sess->ucount=1;
		//dbg("##############sess->id=%d################\n",sess->id);
		if (pthread_create(&sess->tid, NULL, (void *) tcp_do_update, sess) < 0) {
			g_cli_ctx->arg=NULL;
			free_system_session(sess);
			return NULL;
		} 
	}else{
		if((struct sess_ctx*)g_cli_ctx->arg==sess)
				g_cli_ctx->arg=NULL;
		free_system_session(sess);
		printf("******************************** set_transport_type , unknow parameter***********************\n");
		return NULL;
	}
	
	else if (strlen(arg) == 3 && strncmp(arg, "udp", 3) == 0) {
         //We do this so the CLI can set the destination 
        //  ip and port 
        if ((sess->s1 = create_udp_socket()) < 0) {
                perror("Error creating socket");
                return -1;
        }

        //Create base inet address - used later for 
         // setting destination 
        if ((sess->to = create_inet_addr(0)) == NULL) {
                perror("Error creating socket addr");
                close(sess->s1);
                return -1;
        }

        sess->is_udp = 1;
        //dbg("created udp socket\n");
	} 
	else if (strncmp(arg, "fifo", 4) == 0) { 
        // Create a named pipe - Note named pipes are 
         // always pre-pended with fifo 
        if (mkfifo(arg, 0666) == -1) {
                perror("error creating named pipe");
                return -1;
        }

       // Open the pipe for writing 
        if ((sess->pipe_fd = open(arg, O_WRONLY)) < 0) {
                dbg("error");
                perror("error opening named pipe - unlinking");
                unlink(arg);
                return -1;
        }
        sess->pipe_name = strdup(arg);

        sess->is_pipe = 1;
        //dbg("created named pipe at %s\n", sess->pipe_name);
	} 
	else { //Default to file storage 
        if ((sess->file_fd = open(arg, O_CREAT | O_RDWR | O_TRUNC,
                                        0666)) < 0) {
                dbg("error");
                perror("error opening file - exitting");
                return -1;
        }
        sess->file_name = strdup(arg);

        sess->is_file = 1;
        //dbg("opened file %s", sess->file_name);
    }
     
    //dbg("ok\n");
    return NULL;
}

*/

/**
 * get_transport_type - gets the session transport type (tcp, udp, rtp)
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error  
 */
static char *get_transport_type(struct sess_ctx *sess, char *arg)
{
        char *resp;
    
        if (sess == NULL) {
                dbg(" sess is null error\n");
                return NULL;
        }

        if (sess->is_tcp) {
                if ((resp = strdup("tcp")) == NULL)
                        return NULL;
        } else if (sess->is_udp) {
                if ((resp = strdup("udp")) == NULL)
                        return NULL;
        } else if (sess->is_rtp) {
                if ((resp = strdup("rtp")) == NULL)
                        return NULL;
	}else if (sess->is_pipe) {
                if ((resp = strdup("pipe")) == NULL)
                        return NULL;
        } else if (sess->is_file) { 
                if ((resp = strdup("file")) == NULL)
                        return NULL;
        }else {
                dbg("error\n");
                return NULL;
        }

        return resp;
}

/**
 * set_gopsize - sets Group of Picture size
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_gopsize(struct sess_ctx *sess, char *arg)
{
#if 1
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                p->gop.field_val = strtoul(arg, NULL, 10);
                p->gop.change_pending = 1;
                dbg("GOP==%d", p->gop.field_val);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
#endif
}

/**
 * get_gopsize - gets Group of Picture size
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error 
 */
static char *get_gopsize(struct sess_ctx *sess, char *arg)
{
#if 1
        struct video_config_params *p;
        char *resp;

        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = malloc(RESP_BUF_SZ)) == NULL)
                        return NULL;
                memset(resp, 0, RESP_BUF_SZ);
                sprintf(resp, "%d", p->gop.field_val);
                dbg("GOP==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
#endif
}

/**
 * set_bitrate - sets Sampling Bitrate
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_bitrate(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                p->bitrate.field_val = strtoul(arg, NULL, 10);
                p->bitrate.change_pending = 1;
                dbg("Bitrate==%d", p->bitrate.field_val);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_bitrate - gets bitrate used by compressor
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_bitrate(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p;
        char *resp;

        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = malloc(RESP_BUF_SZ)) == NULL)
                        return NULL;
                memset(resp, 0, RESP_BUF_SZ);
                sprintf(resp, "%d", p->bitrate.field_val);
                dbg("Bitrate==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_framerate - sets Video framerate
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_framerate(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                p->framerate.field_val = strtoul(arg, NULL, 10);
                p->framerate.change_pending = 1;
                dbg("Framerate==%d", p->framerate.field_val);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_framerate - gets Video framerate
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_framerate(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 
        char *resp;
    
        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = malloc(RESP_BUF_SZ)) == NULL)
                        return NULL;
                memset(resp, 0, RESP_BUF_SZ);
                sprintf(resp, "%d", p->framerate.field_val);
                dbg("Framerate==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_rotation_angle - sets Video rotation angle
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_rotation_angle(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                p->rotation_angle.field_val = strtoul(arg, NULL, 10);
                p->rotation_angle.change_pending = 1;
                dbg("Rotation_angle==%d", p->rotation_angle.field_val);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_rotation_angle - gets Video rotation angle
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_rotation_angle(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 
        char *resp;
    
        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = malloc(RESP_BUF_SZ)) == NULL)
                        return NULL;
                memset(resp, 0, RESP_BUF_SZ);
                sprintf(resp, "%d", p->rotation_angle.field_val);
                dbg("Rotation_angle==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_output_ratio - sets Video output ratio
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_output_ratio(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                p->output_ratio.field_val = strtoul(arg, NULL, 10);
                p->output_ratio.change_pending = 1;
                dbg("Output_ratio==%d", p->output_ratio.field_val);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_output_ratio - gets Video output ratio
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_output_ratio(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p;
        char *resp;
    
        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = malloc(RESP_BUF_SZ)) == NULL)
                        return NULL;
                memset(resp, 0, RESP_BUF_SZ);
                sprintf(resp, "%d", p->output_ratio.field_val);
                dbg("Output_ratio==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_mirror - sets Video output mirror
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_mirror_angle(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                p->mirror_angle.field_val = strtoul(arg, NULL, 10);
                p->mirror_angle.change_pending = 1;
                dbg("Mirror_angle==%d", p->mirror_angle.field_val);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_mirror_angle - gets Video mirror angle
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_mirror_angle(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p;
        char *resp;

        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = malloc(RESP_BUF_SZ)) == NULL)
                        return NULL;
                memset(resp, 0, RESP_BUF_SZ);
                sprintf(resp, "%d", p->mirror_angle.field_val);
                dbg("Mirror_angle==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_compression - sets Video encoder type
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_compression(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                free(p->compression.field_str);
                p->compression.field_str = strdup(arg);
                p->compression.change_pending = 1;
                dbg("Compression==%s", p->compression.field_str);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_compression - gets Video compression type
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_compression(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p;
        char *resp;

        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = strdup(p->compression.field_str)) == NULL)
                        return NULL;
                dbg("Compression==%s", resp);
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_resolution - sets Video encoder type
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int set_resolution(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                free(p->resolution.field_str);
                p->resolution.field_str = strdup(arg);
                p->resolution.change_pending = 1;
                dbg("Resolution==%s", p->resolution.field_str);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_resolution - gets Video encoder type
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_resolution(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p;
        char *resp;

        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = strdup(p->resolution.field_str)) == NULL)
                        return NULL;
                dbg("Resolution==%s", resp);        
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * set_camera_name - Sets camera name
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static int set_camera_name(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p; 

        if (sess != NULL && sess->video.params != NULL && arg != NULL) {
                p = sess->video.params;
                free(p->name.field_str);
                p->name.field_str = strdup(arg);
                p->name.change_pending = 1;
                dbg("Camera Name==%s", p->name.field_str);
                return 0;
        } else {
                dbg("error");
                return -1;
        }
}

/**
 * get_camera_name - Gets current camera name
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_camera_name(struct sess_ctx *sess, char *arg)
{
        struct video_config_params *p;
        char *resp;

        if (sess != NULL && sess->video.params != NULL) {
                p = sess->video.params;
                if ((resp = strdup(p->name.field_str)) == NULL)
                        return NULL;
                dbg("Camera Name==%s", resp);        
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * update_video_conf - updates video configuration file
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int update_video_conf(struct sess_ctx *sess, char *arg)
{
        if (sess != NULL && sess->video.params != NULL &&
                sess->video.params != NULL) {
                if (save_video_conf(sess->video.params, 
                                        sess->video.cfgfile) < 0)
                        dbg("error saving params to %s", 
                                        sess->video.cfgfile);
                else
                        dbg("params updated");
        }
        return 0;
}

/**
 * reset_video_conf - resets video configuration file to defaults
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int reset_video_conf(struct sess_ctx *sess, char *arg)
{
        /* Not supported */
        dbg("error");
        return -1;
}

/**
 * start_aud - starts audio recording session
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int start_aud(struct sess_ctx *sess, char *arg)
{
        /* Not supported */
        dbg("error");
        return -1;
}

/**
 * stop_aud - stops audio recording session
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int stop_aud(struct sess_ctx *sess, char *arg)
{
        /* Not supported */
        dbg("error");
        return -1;
}

/**
 * restart_server - performs soft restart on server
 * @sess: session context
 * @arg: optional argument
 * Returns 0 on success or -1 on error
 */
static int restart_server(struct sess_ctx *sess, char *arg)
{
	//struct sess_ctx * tmp;
	//int p;
	if(sess==NULL)
		return 0;
	//if(sess->is_rtp){
		
	//}
	
	printf("enter restar_server now begin\n");
	playback_exit(sess->from);
	pthread_mutex_lock(&sess->sesslock);
	sess->running=0;
	pthread_mutex_unlock(&sess->sesslock);
	printf("\nnow wait the monitor stop!\n ");
	//we donnot need to wait thread stop here the lock will do it for us
	//pthread_join(sess->tid,NULL); if we wait here we will lock global_ctx_lock forever!
	//printf("ok the thread stop now\n");
	return 0;
	/*
	if (sess != NULL) {
	        dbg("issued soft restart of server");
	//for playback and monitor, it is same: we will handle playback first.
	if(playback_exit(g_cli_ctx->from) < 0){
		monitor_exit(g_cli_ctx->from);
	}

	} else
	        dbg("error");

	return 0;
	*/
}

/**
 * get_firmware_revision - Gets current firmware revision number
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_firmware_revision(struct sess_ctx *sess, char *arg)
{
        char *resp;

        if (sess != NULL) {
                if ((resp = strdup(FIRMWARE_REVISION)) == NULL)
                        return NULL;
                dbg("Firmware revision==%s\n", resp);        
                return resp;
        } else {
                dbg("error\n");
                return NULL;
        }
}

/**
 * get_firmware_info - Gets current firmware build information
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_firmware_info(struct sess_ctx *sess, char *arg)
{
        char *resp;
	printf("ok enter firmware_info now\n");
        if (1/*sess != NULL*/) {
		//printf("enter if(1)\n");
		  /*
                if ((resp = strdup(FIRMWARE_BUILD)) == NULL)
                        return NULL;
	         dbg("Firmware build==%s", resp);  
	         */
	         resp = malloc(64);
		if(!resp)
			return NULL;
		memset(resp , 0 ,sizeof(resp));
		sprintf(resp,"%x",threadcfg.cam_id);
                return resp;
        } else {
                dbg("error\n");
                return NULL;
        }
}

/**
 * get_video_state - Gets current server state
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_video_state(struct sess_ctx *sess, char *arg)
{
        char *resp;
        char *state;

        if (sess != NULL) {
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
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * get_motion_state - Gets motion state
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_motion_state(struct sess_ctx *sess, char *arg)
{
        char *resp;
        char *state;

        if (sess != NULL) {
                if (sess->connected && sess->motion_detected)
                        state = "is_true";
                else
                        state = "is_false";

                if ((resp = strdup(state)) == NULL)
                        return NULL;
                dbg("motion state==%s", resp);        
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
}

/**
 * get_motion_detection - Gets motion enable/disable state
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static char *get_motion_detection(struct sess_ctx *sess, char *arg)
{
#if 0
        char *resp;
        char *state;

        if (sess != NULL) {
                if (vpu_GetMotionDetection(sess))
                        state = "is_enabled";
                else
                        state = "is_disabled";

                if ((resp = strdup(state)) == NULL)
                        return NULL;
                dbg("motion detection is %s", resp);        
                return resp;
        } else {
                dbg("error");
                return NULL;
        }
#else
	return NULL;
#endif
}

/**
 * enable_motion_detection - Enable motion detection
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static int enable_motion_detection(struct sess_ctx *sess, char *arg)
{
#if 0
        if (sess != NULL) {
                return vpu_EnableMotionDetection(sess);
        } else {
                dbg("error");
                return -1;
        }
#else
	return NULL;
#endif
}

/**
 * disable_motion_detection - Disable motion detection
 * @sess: session context
 * @arg: optional argument
 * Returns parameter string on success or NULL on error
 */
static int disable_motion_detection(struct sess_ctx *sess, char *arg)
{
	return NULL;
#if 0
        if (sess != NULL) {
                sess->motion_detected = 0;
                return vpu_DisableMotionDetection(sess);
        } else {
                dbg("error");
                return -1;
        }
#endif
}

static int SetRs485BautRate(char* arg)
{
	int speed;
	speed = atoi(arg);
//	printf("ready to set baudrate %d\n",speed);
	SetUartSpeed(speed);
	return 0;
}
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
static int Rs485Cmd(char* arg)
{
	int length;
	char* buffer;

	length = arg[0];
	buffer = &arg[1];
	//printf("get rs485cmd time %s\n",gettimestamp());
	//dbg("receive a cmd:%s\n", arg);
	UartWrite(buffer, length);
	return 0;
}
char * get_parse_scan_result( int *numssid ,char *wish_ssid);
char *search_wifi(char *arg)
{
	int numssid;
	char *buf;
	int length;
	char slength[5];
	char *p;
	int size;
	if (!arg)
		return NULL;
	if(*arg=='0'){
		buf = get_parse_scan_result(&numssid , NULL);
		if(!buf)
			return NULL;
		length = strlen(buf);
		memmove(buf+4 ,buf,length);
		memset(buf,0,4);
		sprintf(slength,"%4d",length);
		memcpy(buf,slength,4);
		return buf;
	}else{
		dbg("invalid argument\n");
	}
	return NULL;
}
int snd_soft_restart();
/*
int querryfs(char *fs , unsigned long long*maxsize,unsigned long long* freesize)
{
    struct statfs st; 
   *maxsize = 0;
   *freesize = 0;
  // printf("###############before statfs##############\n");
    if(statfs(fs,&st)<0){
        printf("error querry fs %s\n",fs);
        return -1; 
    }   
  //  printf("##############after statfs################\n");
    *maxsize = st.f_blocks*st.f_bsize;
    *freesize = st.f_bfree *st.f_bsize;
   //  printf("##############querryfs ok###############\n");
    return 0;
}
*/
extern char inet_eth_device[64];
extern char inet_wlan_device[64];
extern char inet_eth_gateway[64];
extern char inet_wlan_gateway[64];
int get_ip(char * device, char * ip, char * mask);
int get_gateway(char * device, char * gateway);
int get_dns(char * dns1, char * dns2);

static int fix_video_line(char *buf)
{
	char *p;
	char *v;
	char ip[32];
	char mask[32];
	char dns1[32];
	char dns2[32];
	p = buf;
	if(!buf||!*p)return -1;
	while(*p&&(*p==' '||*p=='\t'))p++;
	v= p;
	while(*v&&*v!='=')v++;
	if(!*v){
		*v='=';
	}
	v++;
	if(!*v||*v=='\n'){
		if(strncmp(p , CFG_WLAN_IP,strlen(CFG_WLAN_IP))==0){
			memset(ip,0,32);
			get_ip(inet_wlan_device , ip , mask);
			memcpy(v,ip,32);
		}else if(strncmp(p , CFG_WLAN_MASK,strlen(CFG_WLAN_MASK))==0){
			memset(mask,0,32);
			get_ip(inet_wlan_device , ip , mask);
			memcpy(v,mask,32);
		}else if(strncmp(p, CFG_WLAN_DNS1 , strlen(CFG_WLAN_DNS1)) == 0){
			memset(dns1,0,32);
			get_dns(dns1,dns2);
			memcpy(v,dns1,32);
		}else if(strncmp(p,CFG_WLAN_DNS2, strlen(CFG_WLAN_DNS2))==0){
			memset(dns2,0,32);
			get_dns(dns1,dns2);
			memcpy(v,dns2,32);
		}else if(strncmp(p,CFG_WLAN_GATEWAY , strlen(CFG_WLAN_GATEWAY))==0){
			memset(ip , 0 ,32);
			get_gateway(inet_wlan_device, ip);
			memcpy(v,ip,32);
			memcpy(inet_wlan_gateway ,ip ,32);
		}else if(strncmp(p , CFG_ETH_IP,strlen(CFG_ETH_IP))==0){
			memset(ip,0,32);
			get_ip(inet_eth_device , ip , mask);
			memcpy(v,ip,32);
		}else if(strncmp(p , CFG_ETH_MASK,strlen(CFG_ETH_MASK))==0){
			memset(mask,0,32);
			get_ip(inet_eth_device , ip , mask);
			memcpy(v,mask,32);
		}else if(strncmp(p, CFG_ETH_DNS1 , strlen(CFG_ETH_DNS1)) == 0){
			memset(dns1,0,32);
			get_dns(dns1,dns2);
			memcpy(v,dns1,32);
		}else if(strncmp(p,CFG_ETH_DNS2, strlen(CFG_ETH_DNS2))==0){
			memset(dns2,0,32);
			get_dns(dns1,dns2);
			memcpy(v,dns2,32);
		}else if(strncmp(p,CFG_ETH_GATEWAY , strlen(CFG_ETH_GATEWAY))==0){
			memset(ip , 0 ,32);
			get_gateway(inet_eth_device, ip);
			memcpy(v,ip,32);
			memcpy(inet_eth_gateway , ip , 32);
		}else{
			return -1;
		}
		buf[strlen(buf)] = '\n';
		return 0;
	}
	return -1;
}  

char * get_clean_video_cfg(int *size)
{
	FILE * fp;
	char *cfg_buf;
	
	fp =fopen(RECORD_PAR_FILE, "r");
	if(!fp){
		system("cp /video.cfg  /data/video.cfg");
		usleep(100000);
		fp =fopen(RECORD_PAR_FILE, "r");
		if(!fp)
			return NULL;
	}
	cfg_buf = (char *)malloc(2048);
	if(!cfg_buf){
		printf("malloc buf for config file error\n");
		fclose(fp);
		return NULL;
	}
	memset(cfg_buf , 0 , 2048);
	fseek(fp , 0 ,  SEEK_END);
	*size = ftell(fp);
	fseek(fp , 0 , SEEK_SET);
	if(fread(cfg_buf , *size , 1 , fp)!=1){
		printf("read configure file error\n");
		free(cfg_buf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	return cfg_buf;
}

/*
char * get_clean_video_cfg()
{
	char buf[256];
	FILE * fp;
	int length;
	char *cfg_buf;
	char *p;
	int write_back = 0;
	fp =fopen(RECORD_PAR_FILE, "r");
	if(!fp){
		system("cp /video.cfg  /data/video.cfg");
		usleep(100000);
		fp =fopen(RECORD_PAR_FILE, "r");
		if(!fp)
			return NULL;
	}
	cfg_buf = (char *)malloc(2048);
	if(!cfg_buf){
		printf("malloc buf for config file error\n");
		fclose(fp);
		return NULL;
	}
	memset(cfg_buf , 0 , 2048);
	length = 0;
	memset(buf , 0 ,256);
	while(fgets(buf ,256 , fp)!=NULL){
		//if(fix_video_line( buf)==0)
			//write_back = 1;
		memcpy(cfg_buf+length , buf ,strlen(buf));
		length +=strlen(buf);
		memset(buf , 0 ,256);
	}
	fclose(fp);
	if(write_back){
		usleep(50000);
		fp = fopen(RECORD_PAR_FILE, "w");
		fwrite(cfg_buf , length , 1,fp);
		fflush(fp);
		fclose(fp);
	}
	return cfg_buf;
}
*/
static char *SetPswd(char*arg)
{
	FILE *fp;
	char *p;
	struct stat st;
	char buf[512];
	int pswd_size;
	if(!arg){
		printf("##############SetPswd no argument#########\n");
		return strdup(PASSWORD_FAIL);
	}
	if(*arg==':'){
		if(stat(PASSWORD_FILE , &st)==0)
			return strdup(PASSWORD_FAIL);
		else{
			arg++;
			if(strncmp(arg,PASSWORD_PART_ARG,strlen(PASSWORD_PART_ARG))==0){
				fp = fopen(PASSWORD_FILE , "w");
				if(fp){
					if(fwrite(arg,1,strlen(arg),fp)==strlen(arg)){
						fclose(fp);
						memset(threadcfg.pswd , 0 ,sizeof(threadcfg.pswd));
						memcpy(threadcfg.pswd , arg , strlen(arg));
						printf("################set password ok %s ############\n",arg);
						return strdup(PASSWORD_OK);
					}
					else{
						fclose(fp);
						sprintf(buf,"rm %s",PASSWORD_FILE);
						system(buf);
						return strdup(PASSWORD_FAIL);
					}
				}
			}
			return strdup(PASSWORD_FAIL);
		}
	}
	if(strncmp(arg,PASSWORD_PART_ARG,strlen(PASSWORD_PART_ARG))!=0){
		printf("##############setpswd invalide argument##############\n");
		return strdup(PASSWORD_FAIL);
	}
	p = arg;
	while (*p&&*p!=':')p++;
	if(*p!=':')
		return strdup(PASSWORD_FAIL);
	*p = 0;
	p++;
	if(strncmp(p,PASSWORD_PART_ARG,strlen(PASSWORD_PART_ARG))!=0){
		printf("##############setpswd invalide argument##############\n");
		return strdup(PASSWORD_FAIL);
	}
	if(strlen(arg)!=strlen(threadcfg.pswd)||strncmp(arg,threadcfg.pswd,strlen(arg))!=0)
		return strdup(PASSWORD_FAIL);
	fp = fopen(PASSWORD_FILE , "w");
	if(fp){
		if(fwrite(p,1,strlen(p),fp)==strlen(p)){
			fclose(fp);
			memset(threadcfg.pswd ,0 ,sizeof(threadcfg.pswd));
			memcpy(threadcfg.pswd,p,strlen(p));
			printf("################set password ok %s ############\n",arg);
			return strdup(PASSWORD_OK);
		}
		else{
			fclose(fp);
			sprintf(buf,"rm %s",PASSWORD_FILE);
			system(buf);
			return strdup(PASSWORD_FAIL);
		}
	}
	return strdup(PASSWORD_FAIL);
}
static char *CheckPswd(char *arg){
	FILE *fp;
	struct stat st;
	char buf[512];
	int pswd_size;
	if(!arg){
		printf("##############SetPswd no argument#########\n");
		return strdup(PASSWORD_FAIL);
	}
	if(strncmp(arg,PASSWORD_PART_ARG,strlen(PASSWORD_PART_ARG))!=0){
		printf("##############setpswd invalide argument##############\n");
		return strdup(PASSWORD_FAIL);
	}
	if(stat(PASSWORD_FILE , &st)==0){
		fp = fopen(PASSWORD_FILE,"r");
		if(fp){
			memset(buf,0,512);
			pswd_size = fread(buf,1,512,fp);
			if(pswd_size>0){
				if(strlen(arg)==strlen(buf)&&strncmp(arg,buf,strlen(arg))==0){
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
	if(stat(PASSWORD_FILE , &st)==0)
		return strdup(PASSWORD_SET);
	else
		return strdup(PASSWORD_NOT_SET);
}

static char *cli_playback_set_status(struct sess_ctx *sess, char *arg)
{
	int status,value;
	char *rsp = NULL;
	char buf[32];
	if(!sess||!arg)
		return NULL;
	if(strncmp(arg,"SEEK",4)==0){
		arg+=4;
		if(*arg!=':')
			return NULL;
		arg++;
		memset(buf,0,32);
		memcpy(buf,arg,strlen(arg));
		if(sscanf(buf,"%d",&value)!=1)
			return NULL;
		cmd_playback_set_status(sess->from, PLAYBACK_STATUS_SEEK, &value);
		rsp = strdup("SEEK");
	}else if(strncmp(arg,"PAUSED",6)==0){
		cmd_playback_set_status(sess->from, PLAYBACK_STATUS_PAUSED, NULL);
		rsp = strdup("PAUSED");
	}else if(strncmp(arg,"RUNNING",7)==0){
		cmd_playback_set_status(sess->from, PLAYBACK_STATUS_RUNNING, NULL);
		rsp = strdup("RUNNING");
	}
	return rsp;
}
static char* GetConfig(char* arg , int *rsp_len)
{
	char ConfigType;
	//FILE* fd;
	int length = 0;
	char* ret = 0;
	char *p;
	char *s;
	//unsigned long long sd_maxsize;
	//unsigned long long sd_freesize;
	int size;
	int cfg_len;
	char slength[5];
	//struct stat st;
	if(!arg)
		return NULL;
	if(check_cli_pswd( arg, &p)!=0)
		return p;
	ConfigType = (int)*arg;
	/*
	arg++;
	if(*arg!=':'){
		dbg("##########the argument not have password \n");
		if(stat(PASSWORD_FILE,&st)!=0){
		}else{
			strdup("PSWD_FAIL")
		}
	}
	*/
	ret = malloc(4096);
	if(!ret) return NULL;
	memset(ret , 0 ,4096);
	*rsp_len = 4;
	p = ret+4;
	//printf("#############enter GetConfig####################\n");
	if( ConfigType == '1' ){
		sprintf(p,APP_VERSION);
		(*rsp_len)+=strlen(p);
		p+=strlen(p);
		sprintf(p,"cam_id=%x\n",threadcfg.cam_id);
		(*rsp_len)+=strlen(p);
		p+=strlen(p);
		s = get_clean_video_cfg(&cfg_len);
		//printf("##################ok get configure file##########\n");
		if(!s){
			length = 0;
		}else{
			memcpy(p , s , cfg_len);
			length = cfg_len;
			free(s);
		}
		(*rsp_len) +=length;
		p+=length;
		sprintf(p,"system_time=%s\n",gettimestamp());
		(*rsp_len)+=strlen(p);
		p+=strlen(p);
		//querryfs("/sdcard", &sd_maxsize, &sd_freesize);
		sprintf(p,"tfcard_maxsize=%d\n",get_sdcard_size());
		(*rsp_len)+=strlen(p);
		//p+=strlen(p);
		//sprintf(p,"tfcard_freesize=%u\n",(unsigned int)(sd_freesize >>20));
		//(*rsp_len)+=strlen(p);
		//p+=strlen(p);
		size = *rsp_len;
		
		sprintf(slength,"%4d",size - 4);
		memcpy(ret , slength , 4);
		/*
		p = ret;
		while(size>0){
			if(size>1000){
				length = 1000 - 4;
				sprintf(slength,"%4d",length);
				memcpy(p,slength,4);
				p+=1000;
				memmove(p+4,p,size - 1000);
				(*rsp_len)+=4;
				size+=4;
				size -=1000;
			}else{
				length = size-4;
				sprintf(slength , "%4d",length);
				memcpy(p,slength,4);
				size = 0;
			}	
		}
		*/
	}else{
		free(ret);
		printf("#####################GetConfig invalid parmeter#########\n");
		printf("ConfigType==%c\n",ConfigType);
		return NULL;
	}
	/*
	if( ConfigType == '1' ){
		fd = fopen(RECORD_PAR_FILE, "r");
		if( fd == 0 ){
			length = 0;
			ret = malloc(4+4);
			memset(ret,0,8);
			sprintf(ret,"%4d",length);
		}
		else{
			fseek(fd, 0, SEEK_END);
			length = ftell(fd);
			fseek(fd, 0, SEEK_SET);
			ret = malloc(length+4+4);
			memset(ret,0,length+4+4);
			sprintf(ret,"%4d",length);
			fread(&ret[4],length,1,fd);
			fclose(fd);
		}
	}
	if( ConfigType == '3' ){
		fd = fopen(MONITOR_PAR_FILE, "r");
		if( fd == 0 ){
			length = 0;
			ret = malloc(4+4);
			memset(ret,0,8);
			sprintf(ret,"%4d",length);
		}
		else{
			fseek(fd, 0, SEEK_END);
			length = ftell(fd);
			fseek(fd, 0, SEEK_SET);
			ret = malloc(length+4+4);
			memset(ret,0,length+4+4);
			sprintf(ret,"%4d",length);
			fread(&ret[4],length,1,fd);
			fclose(fd);
		}
	}
	*/
	//printf("#########################RET#####################\n");
	//printf("%s",ret);
	//printf("#########################END#####################\n");
	return ret;	
}
/*
int test_printf_getConfig()
{
	int id ='1';
	int rsp_len;
	char *buf;
	char *p;
	char data_len[5];
	data_len[4]=0;
	buf=GetConfig(&id, &rsp_len);
	if(!buf){
		printf("**************get Config error*******************\n");
		return 0;
	}
	printf("######################CONFIG RESULTS#########################\n");
	printf("strlen(buf)==%d rsp_len==%d\n",strlen(buf),rsp_len);
	p = buf;
	memcpy(data_len , p,4);
	p+=4;
	p+=atoi(data_len);
	p+=4;
	printf("second data_len==%d\n",strlen(p));
	printf("%s",buf);
	free(buf);
	printf("######################END#############################\n");
	return 0;
}
*/
int set_raw_config_value(char * buffer);
static char * SetConfig(char* arg)
{
	char ConfigType;
	struct sockaddr_in from;
	socklen_t fromlen;
	fd_set sockset;
	struct timeval timeout;
	char *buf;
	char *p;
	int data_len;
	char sdata_len[5];
	int recvlen= 0;
	int tryrecv = 10;
	int ret;
	int i;
	int size;

	if(!arg)
		return  NULL;
	/*
	if(check_cli_pswd( arg, &p)!=0)
		return p;
		*/
	ConfigType = (int)*arg;
	dbg("save the config file\n");
	if(ConfigType == '0'){
		buf = (char *)malloc(256);
		if(!buf){
			return NULL;
		}
		memset(buf, 0 , 256);
		sprintf(buf,"rm %s",PASSWORD_FILE);
		system(buf);
		memset(buf, 0 , 256);
		sprintf(buf,"cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
		system(buf);
		free(buf);
		dbg("reset the value to default\n");
		snd_soft_restart();
		return NULL;
	}
	if( ConfigType != '1' && ConfigType != '3' ){
		dbg("invalid agument\n");
		return NULL;
	}

	//v2ipd_share_mem->v2ipd_to_client0_msg = VS_MESSAGE_STOP_MONITOR;
	//usleep(500);
	if( ConfigType == '1' ){
		buf = (char *)malloc (4096);
		if(!buf){
			printf("error malloc buf for video.cfg\n");
			return NULL;
		}
		memset(buf,0,4096);
		ret = stun_recvmsg(cli_socket,  buf, 4096, NULL, NULL);
		if(ret <=0){
			dbg("error recv configure file\n");
			free(buf);
			return NULL;
		}
		memcpy(sdata_len ,buf, 4);
		sdata_len[5] = 0;
		for(i  = 0 ; i<5 ;i++){
			if(sdata_len[i]>='0'&&sdata_len[i]<='9')
				break;
		}
		if(i==5){
			dbg("error get configure file len\n");
			free(buf);
			return NULL;
		}
		p =sdata_len +i;
		if(sscanf(p , "%d",&data_len)!=1){
			dbg("error get configure file len\n");
			free(buf);
			return NULL;
		}
		size = ret -4;
		memmove(buf ,buf +4,size ); 
		memset(buf+size , 0 ,4);
		while(size <data_len){
			ret =  stun_recvmsg(cli_socket,  buf + size, 4096 - size, NULL, NULL);
			if(ret <=0){
				dbg("recv configure file error\n");
				free(buf);
				return NULL;
			}
			size += ret;
		}
		printf("####################recv configure file####################\n");
		printf("%s\n",buf);
		printf("####################################################\n");
		set_raw_config_value(buf);
		free(buf);
		snd_soft_restart();
		return NULL;
		
	}else{
		dbg("invalid argument\n");
		return -1;
	}
}

int set_system_time(char * time);
static void SetTime(char* arg)
{
	//char*p1;
	//char buffer[20];
	//char cmd[40];

	dbg("SetTime\n");
	//Char time[15]; //YYYYMMDDHHMMSS ----> "MMDDHHMMYYYY.SS"
	if(!arg)
		return;
	set_system_time(arg);
	/*
	p1 = arg;
	memset(cmd,0,40);
	memset(buffer,0,20);
	sprintf(buffer, "%02d%02d%02d%02d%04d.%02d",  dec_string_to_int(&p1[4], 2), dec_string_to_int(&p1[6], 2),dec_string_to_int(&p1[8], 2),dec_string_to_int(&p1[10], 2),
		dec_string_to_int(&p1[0], 4),dec_string_to_int(&p1[12], 2));
	sprintf(cmd,"/rtc -s %s", buffer);
	printf("SetTime: %s\n",cmd);
	system(cmd);
	system("/rtc -init");
	*/
	return;	
}

extern struct vdIn *vdin_camera;

static void set_brightness(char *arg)
{
	char buf[32];
	int value;
	if(!arg)
		return;
	memset(buf,0,32);
	memcpy(buf,arg,strlen(arg));
	if(sscanf(buf,"%d",&value)!=1){
		dbg("error brightness\n");
		return;
	}
	if(v4l2_contrl_brightness(vdin_camera,value)==0){
		threadcfg.brightness = value;
		//save_config_value(CFG_BRIGHTNESS , buf);
	}
}

static void set_contrast(char *arg)
{
	char buf[32];
	int value;
	if(!arg)
		return;
	memset(buf,0,32);
	memcpy(buf,arg,strlen(arg));
	if(sscanf(buf,"%d",&value)!=1){
		dbg("error contrast\n");
		return;
	}
	if(v4l2_contrl_contrast(vdin_camera,value)==0){
		threadcfg.contrast = value;
		//save_config_value(CFG_CONTRAST, buf);
	}
}

static void set_volume(char *arg)
{
	char buf[32];
	int value;
	if(!arg)
		return;
	memset(buf,0,32);
	memcpy(buf,arg,strlen(arg));
	if(sscanf(buf,"%d",&value)!=1){
		dbg("error volume\n");
		return;
	}
	if(alsa_set_volume(value)==0){
		threadcfg.volume = value;
		//save_config_value(CFG_VOLUME, buf);
	}
}
static char* GetTime(char* arg)
{
	char* buffer;
	time_t timep;
	struct tm * gtm;

//	dbg("GetTime\n");
	buffer = malloc(20);
	memset( buffer, 0, 20 );
	time (&timep);
	gtm = localtime(&timep);
	sprintf(buffer,"%04d%02d%02d%02d%02d%02d",gtm->tm_year+1900,gtm->tm_mon+1,gtm->tm_mday,gtm->tm_hour,gtm->tm_min,gtm->tm_sec);
//	dbg("time=%s\n",buffer);
	return buffer;	
}

//0000003a:0007d060:20000101000253-20000101000255
#define MAX_ITEMS_ONE_SEND 20
static char FileNameBuffer[MAX_ALLOWED_FILE_NUMBER*50];
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

//	dbg("get a id char=%s\n",arg);
	if(!arg)
		return NULL;
	id = dec_string_to_int(arg, 8);
	dbg("get a id=%d\n",id);
	if(id==0){
		i = 0;
		memset(FileNameBuffer, 0, sizeof(FileNameBuffer));
		nand_open_simple("/dev/nand-user");
		max_file_num = nand_get_max_file_num();
		dbg("max file num=%d\n", max_file_num);
		//after nand_open, the oldest sector is found, so we get it
		start_sector = 0;
		time = nand_get_file_time(start_sector);
		if( time != 0 && time != (char*)0xffffffff  ){	// 0xffffffff means a deleted file
			dbg("file at %d, time=%s\n", start_sector, time);
			memcpy(&FileNameBuffer[i*48], time, 47 );
			FileNameBuffer[i*48+47] = 0;
			i++;
		}
		else if( time == (char*)0xffffffff ){

		}
		else{
			dbg("the disk is empty\n");
			goto search_finish;
		}
		do{
			next_secotor = nand_get_next_file_start_sector(start_sector);
			if( next_secotor == -1 ){
				dbg("disk end\n");
				break;
			}
			time = nand_get_file_time(next_secotor);
			if( time == (char*)0xffffffff ){
				start_sector = next_secotor;
				continue;
			}
//			dbg("file at %d, time=%s\n", start_sector,time);
			memcpy(&FileNameBuffer[i*48], time, 47 );
			FileNameBuffer[i*48+47] = 0;
			i++;
			start_sector = next_secotor;
		}while(1);
search_finish:
//		dbg("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^%s\n",FileNameBuffer);
		nand_close_simple();	
		total_file_number = i;
	} 	

	buffer = malloc(MAX_ITEMS_ONE_SEND*50+8);
	memset(buffer,0,MAX_ITEMS_ONE_SEND*50+8);
	sprintf(buffer,"%08d", id);
	if( id >= 25 || id >= total_file_number ){
		return buffer;
	}
	if( id >= total_file_number / MAX_ITEMS_ONE_SEND ){
		max_count = total_file_number % MAX_ITEMS_ONE_SEND;
	}
	else{
		max_count = MAX_ITEMS_ONE_SEND;
	}
	for( i = 0; i < max_count; i++ ){	//4 每次传20个包
		sprintf( &buffer[8+i*48], "%s\n", &FileNameBuffer[id*48*20 + i*48]);
	}

	dbg("\n%s\n",&buffer[8]);
	return buffer;
}

static int IsFileExist(char* filename)
{
	int ret;
	struct stat file;
	ret = stat(filename,&file);
	if( ret == 0 ){
		ret = 1;
	}
	else{
		ret = 0;
	}
	return ret;
}

static int IsDhcpMode()
{
	int ret;
	struct stat file;
	ret = stat("/tmp/static",&file);
	if( ret == 0 ){
		ret = 0;
	}
	else{
		ret = 1;
	}
	return ret;
}

static char* version="V 1.43M";

static char* GetRecordStatue(char* arg)
{
	char* buffer;
	int size;
	int state=1;
	char* ip;
	int ret;

	size = nand_get_size("/dev/nand-user");
	size = size /(1024*1024/512);  // GET SIZE IN  M
	buffer = malloc(200);
	memset( buffer, 0, 200 );
//	state = vs_get_record_config();
	if( state != 2 && state != 3 ){
		state = 1;
	}
	ret = IsDhcpMode();
	if( ret == 0 ){
		ip = "s";
	}
	else{
		ip = "d";
	}
	
	sprintf(buffer,"%s\n%8d\n%8d\n%1d\n%s\n", version,size,0,state, ip);
//	dbg("record stat:%s\n", buffer );
	return buffer;
}

static void SetRecordStatue(char* arg)
{
	char mode;

//	dbg("get record statue\n");
	if( arg[0] == '2' ){
		mode = 2;
	}
	else if( arg[0] == '3' ){
		mode = 3;
	}
	else {
		mode = 1;
	}
//	v2ipd_share_mem->v2ipd_to_client0_msg = VS_MESSAGE_STOP_MONITOR;
	usleep(500);
	vs_set_record_config(mode);

	v2ipd_restart_all();
	return;	
}

static int ArrangIpAddress(char* ip)
{
	int i,j;
	char tmp[20];
	char p_dot, *p1;
	int valid = 0;
	memset(tmp,0,20);
	p1=ip;
	p_dot = 1;
	for( i = 0, j = 0; i < 15; p1++,i++ ){
		if( *p1 == '.' ){
			tmp[j++] = *p1;
			p_dot = 1;
			continue;
		}
		else if(p1[0] == '0' && p1[1] == '0' && p1[2] == '0' && p_dot == 1){
			tmp[j++] = '0';
			i+=2;
			p1+=2;
			continue;
		}
		else if(*p1 == '0' && p_dot == 1){
			continue;
		}
		else {
			tmp[j++] = *p1;
			p_dot = 0;
			valid = 1;
			continue;
		}
	}
	if( valid == 0 ){
		return -1;
	}
	memcpy(ip, tmp, strlen(tmp)+1);
	return 0;
}

static void SetIpAddress(char* arg)
{
	char buffer_ip[20];
	char buffer_mask[20];
	char buffer_gateway[20];
	char cmd[100];

	
	dbg("ip address=:%s\n", arg);
	if( IsDhcpMode() ){
		return;
	}

	v2ipd_share_mem->v2ipd_to_client0_msg = VS_MESSAGE_STOP_MONITOR;
	usleep(500);

	
	memset(buffer_ip, 0, 20);
	memset(buffer_mask, 0, 20);
	memset(buffer_gateway, 0, 20);
	memcpy(buffer_ip, arg, 15);
	memcpy(buffer_mask, &arg[15+1], 15);
	memcpy(buffer_gateway, &arg[15+1+15+1], 15);
	if( ArrangIpAddress(buffer_ip) == -1 ){
		return;
	}
	if( -1 == ArrangIpAddress(buffer_mask)){
		return;
	}
	if( -1 == ArrangIpAddress(buffer_gateway)){
		return;
	}
	memset(cmd,0,100);
	sprintf(cmd,"ifconfig eth0 %s netmask %s",buffer_ip, buffer_mask);
	dbg("%s\n",cmd);
	system(cmd);
	system("wait");
	sprintf(cmd,"route add default   gw  %s",buffer_gateway);
	dbg("%s\n",cmd);
	system(cmd);
	system("wait");

	if(strlen(arg) != 48 ){
		dbg("invalid ip length %d\n",strlen(arg));
		return;
	}
	int fd;
	char* buffer;
	fd = open("/dev/nand-data", O_RDWR|O_SYNC);
	if( fd < 0 ){
		perror("open nand-data");
		return;
	}
	buffer = malloc( 512*1024 );
	memset(buffer, 0, 512*1024);
	if( read(fd, buffer, 512*1024) != 512*1024 ){
		printf("read error\n");
		return;
	}
	memcpy(&buffer[0x4000], arg, strlen(arg));
//	dbg("write data file\n");
	close(fd);
	fd = open("/dev/nand-data", O_RDWR|O_SYNC);
	if( fd < 0 ){
		perror("open nand-data");
		return;
	}
//	dbg("write file\n");
	write( fd, buffer, 512*1024 );
	close(fd);
	system("sync");			
//	ioctl(fd, BLKFLSBUF, NULL );
	system("/nand-flush /dev/nand-data");
	system("sync");
	return;
}

static void ReplayRecord(struct sess_ctx *sess,char* arg)
{
	int file_id;
	int seek;
	if(!arg)
		return;
	file_id = hex_string_to_int(arg, 8);
	seek = hex_string_to_int(&arg[8], 8);
	/*
	dbg("ready to replay file at sectors =%d, seek percent=%d\n", file_id,v2ipd_share_mem->replay_file_seek_percent);
	v2ipd_share_mem->replay_file_start_sectors = file_id;
	v2ipd_share_mem->replay_file_seek_percent = hex_string_to_int(&arg[8], 8);
	v2ipd_share_mem->v2ipd_to_v2ipd_msg = VS_MESSAGE_START_REPLAY;
	*/
	printf("%s: address=0x%x, port=%d, seek=%d\n",
			__func__, g_cli_ctx->from.sin_addr.s_addr,
			ntohs(g_cli_ctx->from.sin_port), seek);
	playback_new(g_cli_ctx->from, file_id, seek);
	return;
}

static char* DeleteAllFiles(struct sess_ctx *sess,char* arg)
{
	char * ret;
	ret = malloc(20);
	ret = "DeleteAllFilesOK";
	//v2ipd_disable_write_nand();
	nand_clean();
	//v2ipd_reboot_system();
	return ret;
}

static char* DeleteFile(struct sess_ctx *sess,char* arg)
{
	char * ret;
	int file_id, file_end_id;

	ret = malloc(50);
	file_id = hex_string_to_int(arg, 8);
	file_end_id = hex_string_to_int(&arg[8], 8);
	if( strlen(arg) != 16 ){
		dbg("error delete file, arg invalid\n");
		sprintf(ret,"DeleteFile%08xERROR",file_id);
		return ret;
	}
	sprintf(ret,"DeleteFile%08xOK",file_id);
	//v2ipd_disable_write_nand();
	//sleep(2);
	nand_invalid_file(file_id,file_end_id);
	system("/nand-flush /dev/nand-user");
	//v2ipd_restart_all();
	return ret;
}

static char *reboot (void) 
{
        system("reboot");
        return NULL;
}

#if 1
static const char *cli_cmds = "null";
#else
static const char *cli_cmds = 
    "set_dest_addr      - Set the destination ip address for INET transport\n"
    "set_dest_port      - Set the destination port number for INET transport\n"
    "start_video        - Start video capture\n"
    "pause_video        - Pause video capture\n"
    "stop_video         - Stop video capture\n"
    "start_audio        - Start audio capture\n"
    "stop_audio         - Stop audio capture\n"
    "reboot             - Reboot the system\n"
    "set_transport_type - Sets data transport type, (tcp, udp, or pipe)\n"
    "set_gopsize        - Sets Group of Pictures size\n"
    "set_bitrate        - Sets Sampling Bitrate\n"
    "set_framerate      - Sets Video framerate\n"
    "set_rotation_angle - Sets Video rotation angle\n"
    "set_output_ratio   - Sets Video output rotation angle\n"
    "set_mirror_angle   - Sets Video Mirror angle\n"
    "set_compression    - Sets Video compression type\n"
    "set_resolution     - Sets Video resolution\n"
    "get_transport_type - Gets data transport type, (tcp, udp, or pipe)\n"
    "get_gopsize        - Gets Group of Pictures size\n"
    "get_bitrate        - Gets Sampling Bitrate\n"
    "get_framerate      - Gets Video framerate\n"
    "get_rotation_angle - Gets Video rotation angle\n"
    "get_output_ratio   - Gets Video output rotation angle\n"
    "get_mirror_angle   - Gets Video Mirror angle\n"
    "get_compression    - Gets Video compression type\n"
    "get_resolution     - Gets Video resolution\n"
    "update_video_conf  - Update new Video encoder configuration\n"
    "reset_video_conf   - Restores Video encoder configuration defaults\n"
    "restart_server     - Soft restart of server\n"
    "get_firmware_info  - Gets firmware build information\n"
    "get_firmware_rev   - Gets firmware revision number\n"
    "get_video_state    - Gets state of video server\n"
    "set_camera_name    - Sets the camera name\n"
    "get_camera_name    - Gets the camera name\n"
    "get_motion_state   - Get motion detection status\n"
    "get_motion_detection     - Get motion enable/disable state\n"
    "enable_motion_detection  - Enable motion detection\n"
    "disable_motion_detection - Disable motion detection\n";
#endif
/**
 * do_cli - handles CLI commands
 * @sess: session context
 * @cmd: command to execute
 * @param: optional parameter
 * Returns response string or NULL to ignore resonse
 */
static char *do_cli_cmd(void *sess, char *cmd, char *param, int size, int* rsp_len)
{
        char *resp = NULL;
        *rsp_len = 0;
		/*I think we should seperate two class commands
		* the first class we do not need to check the sessction ,because excute  this commands do not need a sessction!
		* the second class we must check the sesstion , if the sessction do not exist it must something wrong!
		* I am sorry that the man who the first one setup this program cannot foreseen the future extension of this program!
		* now I have to patch it in a clumsy way ^_^
		*/

		/*the first class */
		if(!cmd)
			return NULL;
     	 if (strncmp(cmd, "get_firmware_info", 17) == 0)
               return resp = get_firmware_info(sess, NULL);/*give the function a parameter that it don't use i don't konw why*/
	 else if (strncmp(cmd, "GetRecordStatue", 15) == 0)
               return resp = GetRecordStatue(param);
	 else if (strncmp(cmd, "GetNandRecordFile", 17) == 0)
               return resp = GetNandRecordFile(param);
	 else if (strncmp(cmd, "ReplayRecord", 12) == 0){
                ReplayRecord(sess, param);/*again this function don't need the first parameter*/
		  return resp;
	 }
	  else if (strncmp(cmd, "GetConfig", 9) == 0)
               return  resp = GetConfig(param ,rsp_len);
        else if (strncmp(cmd, "SetConfig", 9) == 0){
              resp =  SetConfig(param);
		  return resp;
        }
	 else if(strncmp(cmd,"search_wifi",11)==0)
	 	return search_wifi(param);
	 else if(strncmp(cmd,"set_pswd",8)==0){
	 	return SetPswd(param);
	 }else if(strncmp(cmd,"check_pswd",10)==0){
	 	return CheckPswd(param);
	 }else if(strncmp(cmd,"pswd_state",10)==0){
	 	return PswdState();
	 } else if (strncmp(cmd, "SetTime", 7) == 0){
                SetTime(param);
		  return NULL;
	 }
        else if (strncmp(cmd, "GetTime", 7) == 0)
             return   GetTime(param);
	 else if (strncmp(cmd, "DeleteAllFiles", 14) == 0)
                return DeleteAllFiles(sess, param);
        else if (strncmp(cmd, "DeleteFile", 10) == 0)
                return DeleteFile(sess, param);
	 /*the second class*/
        if (sess == NULL || cmd == NULL) return NULL;
		//dbg("%s\n",cmd);
	//dbg("####################sess->id = %d##################\n",((struct sess_ctx*)sess)->id);
	if(strncmp(cmd,"set_brightness",14)==0)
		set_brightness(param);
	else if(strncmp(cmd,"set_contrast",12)==0)
		set_contrast(param);
	else  if(strncmp(cmd,"set_volume",10)==0)
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
       // else if (strncmp(cmd, "get_firmware_info", 17) == 0)
               // resp = get_firmware_info(sess, NULL);
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
	else if(strncmp(cmd,"pb_set_status",13)==0)
		resp = cli_playback_set_status( sess, param);
        else
		dbg("cmd = %s**not supported\n", cmd);
    
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
	char* tempfile="/tmp/update";
	char* ret_buf;
	int data_size;
	int size_uboot, size_kernel, size_rootfs;
	char* file_buf;
	
	UpdateCount = p[0] |( p[1] << 8 ) |( p[2] << 16 ) |( p[3] << 24 )  ;
	p += 4;
	if( ( UpdateCount % 100 ) == 0 || UpdateCount == 0xffffffff ){
		dbg("UpdateCount=%d\n",UpdateCount);
	}
	
	if( UpdateCount == 0 ){
		fp = fopen(tempfile, "w");
		fclose(fp);
		system("sync");
		CurrentUpdatedCount = -1;
		v2ipd_request_timeover_protect();
	}
	if( UpdateCount == CurrentUpdatedCount ){
		dbg("host retry, UpdateCount=%d\n",UpdateCount);
		ret = 0;
//		v2ipd_request_timeover_protect();
	}
	else if(UpdateCount == CurrentUpdatedCount + 1){
		v2ipd_request_timeover_protect();
		data_size = 995;
		do{
			xor = p[0];
			for( i = 1; i < data_size; i++ ){
				xor ^= p[i];
			}
			if( xor != p[data_size] ){
				ret = 1;
				dbg("crc error, xor=%x, should =%x\n", xor, p[data_size]);
				break;
			}
			fp = fopen(tempfile, "a");
			if( fp == NULL ){
				ret = 2;	// err restart
				goto err_restart;
			}
			else{
				ret = fwrite(p, 1,data_size,fp);
				if( ret != data_size*1 ){
					ret = 2; // err restart
					fclose( fp );
					goto err_restart;
				}
				else{
					fclose( fp );
					ret = 0;
					CurrentUpdatedCount++;
				}
			}
		}while(0);
	}
	else if( UpdateCount == 0xffffffff ){	// last package
		v2ipd_stop_timeover_protect();
		dbg("CurrentUpdatedCount=%x\n",CurrentUpdatedCount);
		data_size = cmd_len - 13 - 4 - 1;
		do{
			xor = p[0];
			for( i = 1; i < data_size; i++ ){
				xor ^= p[i];
			}
			if( xor != p[data_size] ){
				ret = 0x300;
				break;
			}			
			fp = fopen(tempfile, "a");
			if( fp == NULL ){
				ret = 0x200;	// err restart
				break;
			}
			else{
				ret = fwrite(p, 1, data_size,fp);
				if( ret != data_size ){
					ret = 0x201; // err restart
					fclose(fp);
					break;
				}
				else{
					fclose( fp );
					ret = 0;
				}
			}
			if( ret != 0 ){
				system("rm /tmp/update");
				break;
			}
			// now we are ready to update the system
			v2ipd_disable_write_nand();
			sleep(1);
			system("cp /sbin/reboot /tmp/reboot -f");
			system("sync");
			if( IsFileExist("/tmp/reboot") == 0 ){
				dbg("cope reboot error\n");
				system("rm /tmp/update");
				ret = 0x101;
				break;
			}
			system("cp /nand-flush /tmp/nand-flush -f");
			system("sync");
			if( IsFileExist("/tmp/nand-flush") == 0 ){
				dbg("cope nand-flush error\n");
				system("rm /tmp/update");
				ret = 0x102;
				break;
			}
			system("cp /reboot-delay.sh /tmp/reboot-delay.sh -f");
			system("sync");
			if( IsFileExist("/tmp/reboot-delay.sh") == 0 ){
				dbg("cope reboot-delay error\n");
				system("rm /tmp/update");
				ret = 0x103;
				break;
			}
			file_buf = malloc( TEMP_FILE_SIZE );
			if( file_buf == NULL ){
				ret = 0x104;
				break;
			}
			fp = fopen(tempfile, "r");
			if( fp == NULL ){
				ret = 0x105;
				break;
			}
			fseek(fp, 0, SEEK_SET);
			fread(&size_uboot, 1, 4, fp );
			dbg("uboot size=%d\n",size_uboot);
			if( size_uboot ){
				fw = open("/dev/nand-boot", O_RDWR|O_SYNC);
				//fw = open("/mnt/nfs/nand-boot", O_RDWR|O_SYNC | O_CREAT );
				if( fw == -1 ){
					ret = 0x106;
					break;
				}
				while( size_uboot > TEMP_FILE_SIZE ){
					fread( file_buf, 1,TEMP_FILE_SIZE , fp);
					write( fw, file_buf, TEMP_FILE_SIZE );
					size_uboot -= TEMP_FILE_SIZE;
				}
				if( size_uboot ){
					fread( file_buf, 1,size_uboot , fp);
					write( fw, file_buf, size_uboot );
				}
				close(fw);
				system("sync");			
				system("/tmp/nand-flush /dev/nand-boot");
				system("sync");
			}
//			fseek(fp, size_uboot, SEEK_CUR);
			fread(&size_kernel, 1, 4, fp );
			dbg("kernel size=%d\n",size_kernel);
			if( size_kernel ){
				fw = open("/dev/nand-kernel", O_RDWR|O_SYNC);
				//fw = open("/mnt/nfs/nand-kernel", O_RDWR|O_SYNC |O_CREAT);
				if( fw == -1 ){
					ret = 0x107;
					break;
				}
				while( size_kernel > TEMP_FILE_SIZE ){
					fread( file_buf, 1,TEMP_FILE_SIZE , fp);
					write( fw, file_buf, TEMP_FILE_SIZE );
					size_kernel -= TEMP_FILE_SIZE;
				}
				if( size_kernel ){
					fread( file_buf, 1,size_kernel , fp);
					write( fw, file_buf, size_kernel );
				}
				close(fw);
				system("sync");			
				system("/tmp/nand-flush /dev/nand-kernel");
				system("sync");
			}
//			fseek(fp, size_kernel, SEEK_CUR);
			fread(&size_rootfs, 1, 4, fp );
			dbg("rootfs size=%d\n",size_rootfs);
			if( size_rootfs ){
				fw = open("/dev/nand-rootfs", O_RDWR|O_SYNC);
				//fw = open("/mnt/nfs/nand-rootfs", O_RDWR|O_SYNC |O_CREAT);
				if( fw == -1 ){
					ret = 0x108;
					break;
				}
				while( size_rootfs > TEMP_FILE_SIZE ){
					fread( file_buf, 1,TEMP_FILE_SIZE , fp);
					write( fw, file_buf, TEMP_FILE_SIZE );
					size_rootfs -= TEMP_FILE_SIZE;
				}
				if( size_rootfs ){
					fread( file_buf, 1,size_rootfs , fp);
					write( fw, file_buf, size_rootfs );
				}
				close(fw);
				system("sync");			
				system("/tmp/nand-flush /dev/nand-rootfs");
				system("sync");
			}
			fclose(fp);
			free(file_buf);
			ret = 0;
		}while(0);
		CurrentUpdatedCount = 0xffffffff;
		if( ret == 0 ){
		}
		else{
			system("rm /tmp/update");
		}
		v2ipd_reboot_system();
	}
	else if( UpdateCount == 0xfffffffe ){	// cancel current update process
		dbg("cancel update\n");
		system("rm /tmp/update");
		system("sync");
		ret = 0;
		v2ipd_stop_timeover_protect();
	}
	else{
		ret = 2;
		v2ipd_stop_timeover_protect();
	}

	ret_buf = malloc(10);
	memset(ret_buf,0,10);
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
	memset(ret_buf,0,10);
	p = &cmd[13];
	ret_buf[0] = p[0];
	ret_buf[1] = p[1];
	ret_buf[2] = p[2];
	ret_buf[3] = p[3];
	ret_buf[4] = ret;
	*rsp_len = 8;
	return ret_buf;	
}

char *do_cli_cmd_bin(void *sess, char *cmd, int cmd_len, int size, int* rsp_len)
{
	*rsp_len = 0;

	if( strncmp(cmd,"UpdateSystem",12) == 0 ){
		return UpdateSystem(cmd, cmd_len, rsp_len);
	}
    
	return NULL;
}

struct cli_sess_ctx * start_cli(void* arg)
{
	struct cli_sess_ctx * cli_ctx;
	cli_cmd_handler.cmds = (char *) cli_cmds;
	cli_cmd_handler.handler = do_cli_cmd; 
	cli_cmd_handler.handler_bin = do_cli_cmd_bin; 

	if ((cli_ctx = cli_init(arg)) != NULL) {
		if (cli_bind_cmds(cli_ctx, &cli_cmd_handler) < 0) {
			printf("error installing CLI handlers");
			return 0;
		}
	} else {
		printf("error initializing CLI");
		return 0;
	}
	return cli_ctx;
}


