#include "udp_transfer.h"
#include "includes.h"
#include "sockets.h"
#include "cli.h"
#include "rtp.h"
#include "playback.h"
#include "utilities.h"
#include <pthread.h>

#define CAM_ID   1

#define LOCAL_ALIVE_PORT 3000
#define PACK_HEAD_SIZE     (sizeof(u8)+sizeof(uint16_t))
#define TIME_OUT			    10
#define READ_STRUCT_TIME_OUT    180

#define MAX_PORT_TYPE     6
static  uint16_t local_port[6]={CLI_PORT,VIDEO_SESS_PORT,VIDEO_SESS_PORT+1,AUDIO_SESS_PORT,AUDIO_SESS_PORT+1,5006};
#define BUF_SZ 512
static int  build_nat_addr(int type,int recvsock)
{
	int sockfd;
	int on;
	int ret;
	u8 req[BUF_SZ];
	int req_len;
	uint16_t size;
	uint16_t nsize;
	struct sockaddr_in from;
	//struct in_addr inaddr;
	socklen_t fromlen;
	//cmd_t cmd;
	struct timeval oldtime;
	struct timeval age;
	//struct public_port getport;
	if ((sockfd = create_udp_socket()) < 0) {
       	 perror("Error creating socket");
        	return -1;
   	}
	on = 1;
       if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
              printf("Error enabling socket address reuse");
        }
   	if ((bind_udp_socket(sockfd, local_port[type])) == NULL) {
     	   	 printf("****************************Error binding udp socket****************************");
      	  	 close(sockfd);
       	 return -1;
   	 }
	 ret=inet_pton(AF_INET,SERVER_IP,&(from.sin_addr.s_addr));
  	 if(ret<=0){
		 printf("create server ip error\n");
	 	 close(sockfd);
	  	 return -1;
    	 }
	  from.sin_family=AF_INET;
	  from.sin_port=htons(INTERACTIVE_PORT);
	   fromlen=sizeof(struct sockaddr_in); 
	   memset(req,0,sizeof(req));
	   req[0]=(u8)NET_CAMERA_PORT;
	   size=0;
	   sendto(sockfd, req, size+PACK_HEAD_SIZE, 0,(struct sockaddr *) &from, fromlen); 
	   gettimeofday(&oldtime,NULL);
	  while(1){
	  	gettimeofday(&age,NULL);
		if(abs(age.tv_sec-oldtime.tv_sec)>TIME_OUT){
			close(sockfd);
		       printf("build local_port[%d] error\n",type);
			return -1;
		}
		req_len=recvfrom(recvsock, req, BUF_SZ, 0, (struct sockaddr *) &from, &fromlen); 
		memcpy(&nsize,&req[1],sizeof(nsize));
		size=ntohs(nsize);
		if(req_len>0){
			int i;
			for (i = 0;i<req_len;i++)
				printf("%2x  ",req[i]);
			printf("\n");
		}
		if(req_len>0&&req_len==size+PACK_HEAD_SIZE&&req[0]==(u8)NET_CAMERA_OK) {
			break;
		}
		usleep(1000);
	  }
	  close(sockfd);
	  printf("build local_port[%d] ok\n",type);
	  return 0;
}
int monitor_try_connected_thread()
{
	int ret;
	int sockfd;
	u8 req[BUF_SZ];
	uint16_t size;
	uint16_t nsize;
	uint32_t  camid;
	uint32_t ncamid;
//   	char *rsp;
   	socklen_t fromlen;
  	ssize_t req_len;
//	int rsp_len;
	int on;
	//struct timeval oldtime,newtime;
//	struct sockaddr_in *saddr;
       struct sockaddr_in from;
	//cmd_t cmd,replay;
	struct udp_transfer recvaddr;
	struct udp_transfer *pdest=&recvaddr;
	struct timeval  timeout;
	//struct mapping*p;

#define set_alive()   \
	do{ \
		from.sin_family=AF_INET;\
		 from.sin_port=htons(CLIENT_WATCH_PORT);\
		 fromlen=sizeof(struct sockaddr_in); \
		  printf("from.sin_port==%d\n",from.sin_port);\
		 while(1){ \
			 memset(req,0,BUF_SZ);\
	  	 	 req[0]=(u8)NET_CAMERA_ID;	\
			 size=sizeof(uint32_t);\
			 nsize=htons(size);\
			 memcpy(&req[1],&nsize,sizeof(nsize)); \
			 camid=CAM_ID; \
			 ncamid=htonl(camid); \
			 memcpy(&req[3],&ncamid,sizeof(ncamid));\
    			sendto(sockfd, req, size+PACK_HEAD_SIZE, 0,(struct sockaddr *) &from, fromlen); \
			req_len=recvfrom(sockfd, req, BUF_SZ, 0, (struct sockaddr *) &from, &fromlen); \
			if(req_len>0){\
				int i;\
				for (i = 0;i<req_len;i++)\
					printf("%2x  ",req[i]);\
				printf("\n");\
			}\
			memcpy(&nsize,&req[1],sizeof(nsize));\
			size=ntohs(nsize); \
			if(req_len>0&&req_len==size+PACK_HEAD_SIZE&&req[0]==(u8)NET_CAMERA_OK) {\
				break; \
			}\
			printf("monitor set alive recv error req_len==%d\n",req_len);\
			usleep(1000);\
		} \
	}while(0)
   	if ((sockfd = create_udp_socket()) < 0) {
       	 perror("Error creating socket\n");
        	return -1;
   	 }
	on = 1;
	 if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
              printf("Error enabling socket address reuse\n");
        }
	  if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0){
              printf("Error enabling socket keep alive \n");
        }
	  timeout.tv_sec=3;
	  timeout.tv_usec=0;
	   if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
              printf("Error enabling socket rcv time out \n");
        }
  	  if ((bind_udp_socket(sockfd, LOCAL_ALIVE_PORT)) == NULL) {
     	   	 printf("****************************Error binding udp socket****************************");
      	  	 close(sockfd);
       	 return -1;
   	 }
   	 ret=inet_pton(AF_INET,SERVER_IP,&(from.sin_addr.s_addr));
  	 if(ret<=0){
		 printf("create server ip error\n");
	 	 close(sockfd);
	  	 return -1;
    	 }
	 printf("conver server ip ok\n");
	//gettimeofday(&oldtime,NULL);
__retry:
	set_alive();
	printf("monitor set alive ok\n");
	while(1){
		/*
		gettimeofday(&newtime,NULL);
		if(newtime.tv_sec-oldtime.tv_sec>180){
			 set_alive();
			memcpy(&oldtime,&newtime,sizeof(struct timeval));
		}
		*/
		req_len=recvfrom(sockfd, req, BUF_SZ, 0, (struct sockaddr *) &from, &fromlen);
		if(req_len<=0)
			continue;
		memcpy(&nsize,&req[1],sizeof(nsize));
		size=ntohs(nsize); 
		printf("monitor recv data,len==%d,size==%d\n",req_len,size);
		if(req_len>0){
			int i;
			for (i = 0;i<req_len;i++)
				printf("%2x  ",req[i]);
			printf("\n");
		}
		if(req_len>0&&req_len==size+PACK_HEAD_SIZE){
			switch(req[0]){
				/*
				case server_cmd_get_id:{
					set_alive();
					gettimeofday(&oldtime,NULL);
					break;
				}
				*/
				printf("get command %d\n",req[0]);
				case NET_CAMERA_SEND_PORTS:{
					int i;
					for(i=0;i<MAX_PORT_TYPE;i++){
						printf("try build public port type:%d\n",i);
						ret=build_nat_addr(i,sockfd);
						if(ret<0){
							printf("build public port error\n");
							goto __retry;
						}
					}
					break;
				}
				case NET_CAMERA_PEER_PORTS:{
					if(size!=sizeof(struct udp_transfer))
						goto __retry;
					memcpy(pdest,&req[PACK_HEAD_SIZE],sizeof(struct udp_transfer));
					touch_playback_port(local_port[CMD_PB_PORT], pdest->ip, pdest->port[CMD_PB_PORT]);
					touch_cli_port(pdest->ip,pdest->port[CMD_CLI_PORT]);
					videosess_add_dstaddr(pdest->ip, pdest->port[CMD_V_RTP_PORT], pdest->port[CMD_V_RTCP_PORT]);
					audiosess_add_dstaddr(pdest->ip, pdest->port[CMD_A_RTP_PORT], pdest->port[CMD_A_RTCP_PORT]);
					struct in_addr inaddr;
					inaddr.s_addr=pdest->ip;
					printf("\n#########################MONITOR ################################\n");
					printf("connected in ip:%s\n",inet_ntoa(inaddr));
					printf("cli port:%d\n",ntohs(pdest->port[CMD_CLI_PORT]));
					printf("videosess rtp port:%d\n",ntohs(pdest->port[CMD_V_RTP_PORT]));
					printf("videosess rctp port:%d\n",ntohs(pdest->port[CMD_V_RTCP_PORT]));
					printf("audio rtp port:%d\n",ntohs(pdest->port[CMD_A_RTP_PORT]));
					printf("audio rctp port:%d\n",ntohs(pdest->port[CMD_A_RTCP_PORT]));
					printf("playback port:%d\n",ntohs(pdest->port[CMD_PB_PORT]));
					printf("\n#########################################################\n");
					goto __done;
				}
				default:
					break;
			}
		}
		else
			goto __retry;
	}
__done:
	printf("monitor connected sucess!\n");
   	return 0;
}