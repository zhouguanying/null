#include "inet_type.h"
#include "includes.h"
#include "sockets.h"
#include "cli.h"
#include "rtp.h"
#include "playback.h"
#include "utilities.h"
#include <pthread.h>
#include <assert.h>

#define CAM_ID   1

#define LOCAL_ALIVE_PORT 3000
#define PACK_HEAD_SIZE     (sizeof(u8)+sizeof(uint16_t))
#define TIME_OUT			    10
#define READ_STRUCT_TIME_OUT    180

#define MAX_PORT_TYPE     6

pthread_mutex_t ready_list_lock;
struct mapping*ready_list=NULL;
int ready_count=0;

static  uint16_t local_port[6]={CLI_PORT,VIDEO_SESS_PORT,VIDEO_SESS_PORT+1,AUDIO_SESS_PORT,AUDIO_SESS_PORT+1,5006};
void add_to_ready_list(struct mapping*p)
{
	if(p==NULL)
		return;
	pthread_mutex_lock(&ready_list_lock);
	p->next=ready_list;
	ready_list=p;
	ready_count++;
	pthread_mutex_unlock(&ready_list_lock);
}
int set_ready_struct_connected(uint32_t ip,uint16_t cliport)
{
	struct mapping *p;
	int ret=-1;
	pthread_mutex_lock(&ready_list_lock);
	p=ready_list;
	while(p!=NULL){
		if(p->destaddrs.ip==ip&&p->destaddrs.port[CMD_CLI_PORT]==cliport){
			p->connected=1;
			videosess_add_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_V_RTP_PORT], p->destaddrs.port[CMD_V_RTCP_PORT]);
			audiosess_add_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_A_RTP_PORT], p->destaddrs.port[CMD_A_RTCP_PORT]);
			ret=0;
			break;
		}
		p=p->next;
	}
	pthread_mutex_unlock(&ready_list_lock);
	return ret;
}
int set_read_struct_pb_running(uint32_t ip,uint16_t cliport)
{
	struct mapping *p;
	int ret=-1;
	pthread_mutex_lock(&ready_list_lock);
	p=ready_list;
	while(p!=NULL){
		if(p->destaddrs.ip==ip&&p->destaddrs.port[CMD_CLI_PORT]==cliport){
			p->pb_running=1;
			ret=0;
			break;
		}
		p=p->next;
	}
	pthread_mutex_unlock(&ready_list_lock);
	return ret;
}
void delete_timeout_mapping(uint32_t ip,uint16_t cliport)
{
	struct mapping *p;
	struct mapping ** tmp;
	struct timeval  now;
	pthread_mutex_lock(&ready_list_lock);
	tmp=&ready_list;
	gettimeofday(&now,NULL);
	while((*tmp)!=NULL){
		p=(*tmp);
		if(p->destaddrs.ip==ip&&p->destaddrs.port[CMD_CLI_PORT]==cliport){
			*tmp=(*tmp)->next;
			put_playback_port(p->destaddrs.port[CMD_PB_PORT]);
			videosess_remove_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_V_RTP_PORT], p->destaddrs.port[CMD_V_RTCP_PORT]);
			audiosess_remove_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_A_RTP_PORT], p->destaddrs.port[CMD_A_RTCP_PORT]);
			free(p);
			ready_count--;
			continue;
		}
		if(p->connected==0&&(abs(now.tv_sec-p->aged.tv_sec)>READ_STRUCT_TIME_OUT)){
			*tmp=(*tmp)->next;
			put_playback_port(p->destaddrs.port[CMD_PB_PORT]);
			videosess_remove_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_V_RTP_PORT], p->destaddrs.port[CMD_V_RTCP_PORT]);
			audiosess_remove_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_A_RTP_PORT], p->destaddrs.port[CMD_A_RTCP_PORT]);
			free(p);
			ready_count--;
		}else
			tmp=&((*tmp)->next);
	}
	pthread_mutex_unlock(&ready_list_lock);
}
int get_ready_count()
{
	int ret;
	pthread_mutex_lock(&ready_list_lock);
	ret=ready_count;
	pthread_mutex_unlock(&ready_list_lock);
	return ret;
}
int remove_from_ready_list(uint32_t addr,uint16_t cliport)
{
	struct mapping *p;
	struct mapping ** tmp;
	pthread_mutex_lock(&ready_list_lock);
	tmp=&ready_list;
	while((*tmp)!=NULL&&((*tmp)->destaddrs.ip!=addr||(*tmp)->destaddrs.port[CMD_CLI_PORT]!=cliport))
		tmp=&((*tmp)->next);
	if((*tmp)==NULL)
		p=NULL;
	else{
		p=(*tmp);
		*tmp=(*tmp)->next;
		ready_count--;
	}
	pthread_mutex_unlock(&ready_list_lock);
	if(p){
		put_playback_port(p->destaddrs.port[CMD_PB_PORT]);
		videosess_remove_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_V_RTP_PORT], p->destaddrs.port[CMD_V_RTCP_PORT]);
		audiosess_remove_dstaddr(p->destaddrs.ip, p->destaddrs.port[CMD_A_RTP_PORT], p->destaddrs.port[CMD_A_RTCP_PORT]);
		free(p);
		return 0;
	}
	return -1;
}
struct udp_transfer *get_udp_transfer(uint32_t addr,uint16_t cliport)
{
	struct mapping *p;
	struct udp_transfer *ret=NULL;
	pthread_mutex_lock(&ready_list_lock);
	p=ready_list;
	while(p!=NULL&&(p->destaddrs.ip!=addr||p->destaddrs.port[CMD_CLI_PORT]!=cliport))
		p=p->next;
	if(p!=NULL){
		ret=malloc(sizeof(struct udp_transfer));
		if(ret)
			memcpy(ret,&(p->destaddrs),sizeof(struct udp_transfer));
	}
	pthread_mutex_unlock(&ready_list_lock);
	return ret;
}
void touch_cli_port(uint32_t ip, uint16_t port)
{
	int sockfd=g_cli_ctx->sock;
	socklen_t fromlen;
	struct sockaddr_in from;
	from.sin_family=AF_INET;
	fromlen=sizeof(struct sockaddr_in);
	from.sin_addr.s_addr=ip;
	from.sin_port=port;
	sendto(sockfd, "touch", strlen("touch"), 0,(struct sockaddr *) &from, fromlen); 
}
static inline int touch_port(uint16_t localport,uint32_t ip,uint16_t port)
{
	int sockfd;
	int on;
	struct sockaddr_in from;
	socklen_t fromlen;
	if ((sockfd = create_udp_socket()) < 0) {
       	 perror("Error creating socket\n");
        	return -1;
   	 }
	on = 1;
	 if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
              printf("Error enabling socket address reuse\n");
        }
  	  if ((bind_udp_socket(sockfd, localport)) == NULL) {
     	   	 printf("****************************Error binding udp socket****************************\n");
      	  	 close(sockfd);
       	 return -1;
   	 }
	  from.sin_family=AF_INET;
	  fromlen=sizeof(struct sockaddr_in);
	  from.sin_addr.s_addr=ip;
	  from.sin_port=port;
	  sendto(sockfd, "touch", strlen("touch"), 0,(struct sockaddr *) &from, fromlen);
	  close(sockfd);
	  return 0;
}
 int touch_v_rtp_port(uint32_t ip,uint16_t port)
{
	return touch_port(VIDEO_SESS_PORT,  ip,  port);
}
 int touch_v_rtcp_port(uint32_t ip,uint16_t port)
{
	return touch_port(VIDEO_SESS_PORT+1,  ip,  port);
}
 int touch_a_rtp_port(uint32_t ip,uint16_t port)
{
	return touch_port(AUDIO_SESS_PORT,  ip,  port);
}
 int touch_a_rtcp_port(uint32_t ip,uint16_t port)
{
	return touch_port(AUDIO_SESS_PORT+1,  ip,  port);
}

int touch_playback_port(uint16_t localport,uint32_t ip,uint16_t port)
{
	return touch_port( localport,  ip,  port);
}
#define BUF_SZ 512
static int build_nat_addr(int type,int recvsock)
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
		if(req_len>0&&req_len==size+PACK_HEAD_SIZE&&req[0]==(u8)NET_CAMERA_OK) {
			break;
		}
		usleep(1000);
	  }
	  close(sockfd);
	  printf("build local_port[%d] ok\n",type);
	  return 0;
}
int keep_playback_port_alive_thread()
{
	struct mapping *p;
	while(1){
		pthread_mutex_lock(&ready_list_lock);
		p=ready_list;
		while(p!=NULL){
			if(p->pb_running==0)
				touch_playback_port(p->local_pb_port, p->destaddrs.ip, p->destaddrs.port[CMD_PB_PORT]);
			p=p->next;
		}
		pthread_mutex_unlock(&ready_list_lock);
		sleep(60);
	}
	return 0;
}
/*this function don't run well in arm architecture
int buffer_setup(u8 cmd, void *data, int data_len ,
                               u8 *sbuf, int sbuf_len)
{
    if (sbuf_len < (data_len + 1 + 2)) 
    {   
        printf("error buffer is too small\n");
       // assert(0);
       return -1;
    }   

	printf("cmd = %d\n", cmd);
    sbuf[0] = cmd;
    *( (uint16_t *)(sbuf + 1) ) = htons(data_len); // this line is the main reason 

    if (data_len > 0 && data)
        memcpy(sbuf + 3, data, data_len);

    return data_len + 3;
}
*/
int transfer_thread()
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
    	int n;
	//struct timeval oldtime,newtime;
//	struct sockaddr_in *saddr;
       struct sockaddr_in from;
	//cmd_t cmd,replay;
	struct udp_transfer recvaddr;
	struct udp_transfer *pdest=&recvaddr;
	struct mapping*p;
	struct timeval timeout;
	/*
#define set_alive()   \
	do{ \
		socket_set_nonblcok(sockfd);\
		from.sin_family=AF_INET;\
		 from.sin_port=htons(CAMERA_WATCH_PORT);\
		 fromlen=sizeof(struct sockaddr_in); \
		 while(1){ \
		 	camid=htonl(CAM_ID);\
		 	size=sizeof(camid);\
	  	 	buffer_setup(NET_CAMERA_ID, &camid, size,  req, BUF_SZ);\
			 for(n=0;n<size+PACK_HEAD_SIZE;n++)\
			 	printf("%d",req[n]);\
			 printf("\n");\
    			sendto(sockfd, req, size+PACK_HEAD_SIZE, 0,(struct sockaddr *) &from, fromlen); \
			req_len=recvfrom(sockfd, req, BUF_SZ, 0, (struct sockaddr *) &from, &fromlen); \
			psize=(uint16_t*)&req[1];\
			size=ntohs(*psize); \
			if(req_len>0&&req_len==size+PACK_HEAD_SIZE&&req[0]==(u8)NET_CAMERA_OK) {\
				socket_reset_block(sockfd);\
				break; \
			}\
			printf("camera set alive recv error req_len==%d\n",req_len);\
			usleep(1000);\
		} \
	}while(0)
	*/
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
	 	printf("convert server ip sucess\n");
		from.sin_family=AF_INET;
		 from.sin_port=htons(CAMERA_WATCH_PORT);
		 fromlen=sizeof(struct sockaddr_in); 
		 while(1){ 
		 	camid=CAM_ID;
		 	size=sizeof(camid);
			req[0]=NET_CAMERA_ID;
			nsize=htons(size);
			memcpy(&req[1],&nsize,sizeof(nsize));
			ncamid=htonl(camid);
			memcpy(&req[3],&ncamid,sizeof(ncamid));
    			sendto(sockfd, req, size+PACK_HEAD_SIZE, 0,(struct sockaddr *) &from, fromlen); 
			req_len=recvfrom(sockfd, req, BUF_SZ, 0, (struct sockaddr *) &from, &fromlen); 
			memcpy(&nsize,&req[1],sizeof(nsize));
			size=ntohs(nsize); 
			if(req_len>0&&req_len==size+PACK_HEAD_SIZE&&req[0]==(u8)NET_CAMERA_OK) {
				break; 
			}
		//	printf("camera set alive recv error req_len==%d\n",req_len);
			usleep(1000);
		} 
	//gettimeofday(&oldtime,NULL);
	printf("camera set alive ok\n");
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
		if(req_len>0&&req_len==size+PACK_HEAD_SIZE){
			printf("get command\n");
			printf("req[0]==%d\n",req[0]);
			printf("size==%d\n",size);
			printf("udp_transfer size==%d\n",sizeof(struct udp_transfer));
			switch(req[0]){
				/*
				case server_cmd_get_id:{
					set_alive();
					gettimeofday(&oldtime,NULL);
					break;
				}
				*/
				case NET_CAMERA_SEND_PORTS:{
					printf("NET_CAM_SEND_PORTS\n");
					if(size!=sizeof(struct udp_transfer))
						break;
					memcpy(pdest,&req[PACK_HEAD_SIZE],sizeof(struct udp_transfer));
					printf("before delete_timeout_mapping\n");
					printf("ready_count==%d\n",get_ready_count());
					printf("read_list==%p\n",ready_list);
					delete_timeout_mapping( pdest->ip,  pdest->port[CMD_CLI_PORT]);
					ret=get_ready_count();
					printf("after delete_timeout_mapping\n");
					printf("ready_count==%d\n",get_ready_count());
					printf("read_list==%p\n",ready_list);
					if(ret>=MAX_CONNECTIONS)
						break;
					local_port[CMD_PB_PORT]=get_playback_port();
					int i;
					for(i=0;i<MAX_PORT_TYPE;i++){
						printf("try build public port type:%d\n",i);
						ret=build_nat_addr(i,sockfd);
						if(ret<0){
							printf("build public port error\n");
							put_playback_port(local_port[CMD_PB_PORT]);
							break;
						}
					}
					if(ret>=0){
						p=malloc(sizeof(struct mapping));
						if(!p){
							put_playback_port(local_port[CMD_PB_PORT]);
							break;
						}
						gettimeofday(&(p->aged),NULL);
						p->connected=0;
						p->pb_running=0;
						memcpy(&(p->destaddrs),pdest,sizeof(struct udp_transfer));
						p->local_pb_port=local_port[CMD_PB_PORT];
						touch_playback_port(local_port[CMD_PB_PORT], pdest->ip, pdest->port[CMD_PB_PORT]);
						touch_cli_port(pdest->ip,pdest->port[CMD_CLI_PORT]);
						touch_v_rtp_port(pdest->ip, pdest->port[CMD_V_RTP_PORT]);
						touch_v_rtcp_port(pdest->ip,  pdest->port[CMD_V_RTCP_PORT]);
						touch_a_rtp_port(pdest->ip, pdest->port[CMD_A_RTP_PORT]);
						touch_a_rtcp_port(pdest->ip, pdest->port[CMD_A_RTCP_PORT]);
						//videosess_add_dstaddr(pdest->ip, pdest->port[CMD_V_RTP_PORT], pdest->port[CMD_V_RTCP_PORT]);
						//audiosess_add_dstaddr(pdest->ip, pdest->port[CMD_A_RTP_PORT], pdest->port[CMD_A_RTCP_PORT]);
						add_to_ready_list( p);
						printf("after add to ready list\n");
						printf("ready_count==%d\n",get_ready_count());
						printf("read_list==%p\n",ready_list);
						struct in_addr inaddr;
						inaddr.s_addr=pdest->ip;
						printf("before conver ip==%x\n",pdest->ip);
						printf("\n##########################CAMERA ###############################\n");
						printf("connected in ip:%s\n",inet_ntoa(inaddr));
						printf("cli port:%d\n",ntohs(pdest->port[CMD_CLI_PORT]));
						printf("videosess rtp port:%d\n",ntohs(pdest->port[CMD_V_RTP_PORT]));
						printf("videosess rctp port:%d\n",ntohs(pdest->port[CMD_V_RTCP_PORT]));
						printf("audio rtp port:%d\n",ntohs(pdest->port[CMD_A_RTP_PORT]));
						printf("audio rctp port:%d\n",ntohs(pdest->port[CMD_A_RTCP_PORT]));
						printf("playback port:%d\n",ntohs(pdest->port[CMD_PB_PORT]));
						printf("\n#########################################################\n");
						
					}
					break;
				}
				/*
				case server_cmd_call_me:{
					req_len=recvfrom(sockfd, &getdest, sizeof(getdest), 0, (struct sockaddr *) &from, &fromlen); 
					if(req_len<=0||req_len!=sizeof(getdest))
						break;
					if(set_keep_alive_data(cmd.who, getdest.ip, getdest.port[CMD_PB_PORT])<0)
						break;
					struct mapping*m=malloc(sizeof(struct mapping));
					memcpy(&(m->destaddrs),&getdest,sizeof(struct udp_transfer));
					add_to_ready_list(m);
					videosess_add_dstaddr(getdest.ip, getdest.port[CMD_V_RTP_PORT], getdest.port[CMD_V_RTCP_PORT]);
					audiosess_add_dstaddr(getdest.ip, getdest.port[CMD_A_RTP_PORT], getdest.port[CMD_A_RTCP_PORT]);
					touch_cli_port(getdest.ip, getdest.port[CMD_CLI_PORT]);
					break;
				}
				*/
				default:
					break;
			}
		}
	}
   	return 0;
}
int start_udp_transfer()
{
	//int ret;
	//int i;
	pthread_t tid;
	pthread_mutex_init(&ready_list_lock,NULL);
	if (pthread_create(&tid, NULL, (void *) transfer_thread,NULL) < 0) {
		return -1;
	} 
	return 0;
}










