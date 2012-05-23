#include "inet_type.h"
#include "includes.h"
#include "sockets.h"
#include "cli.h"
#include "rtp.h"
#include "playback.h"
#include "utilities.h"
#include <pthread.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 

#define CAM_ID   1

#define LOCAL_ALIVE_PORT 3000
#define PACK_HEAD_SIZE     (sizeof(u8)+sizeof(uint16_t))
#define TIME_OUT			    10
#define READY_STRUCT_TIME_OUT    60

#define MAX_PORT_TYPE     6

pthread_mutex_t ready_list_lock;
struct mapping*ready_list=NULL;
int ready_count=0;


#define BASE_PORT                  5001
pthread_mutex_t local_port_m_lock;
char local_port_m[MAX_CONNECTIONS<<1];


in_addr_t server_addr;

void init_local_m_data()
{
	pthread_mutex_init(&ready_list_lock,NULL);
	pthread_mutex_init(&local_port_m_lock,NULL);
	memset(local_port_m,0,sizeof(local_port_m));
}

uint16_t get_local_port()
{
	int i;
	pthread_mutex_lock(&local_port_m_lock);
	for(i=0;i<(MAX_CONNECTIONS<<1);i++)
		if(local_port_m[i]==0)
			break;
		local_port_m[i]=1;
	pthread_mutex_unlock(&local_port_m_lock);
	printf("in get_playback_port i==%d,port==%d\n",i,BASE_PORT+(i<<1));
	return (BASE_PORT+(i<<1));
}
int put_local_port(uint16_t port)
{
	int i;
	if(port<BASE_PORT)
		return -1;
	i=(port-BASE_PORT)>>1;
	printf("in put playback port port==%d ,  i==%d\n",port,i);
	pthread_mutex_lock(&local_port_m_lock);
	local_port_m[i]=0;
	pthread_mutex_unlock(&local_port_m_lock);
	return 0;
}




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
	printf("in set_ready_struct connectd cli port ==%d\n",ntohs(cliport));
	while(p!=NULL){
		printf("address %p,cliport ==%d\n",p,ntohs(p->dst_port[NAT_CLI_PORT]));
		if(p->ip==ip&&p->dst_port[NAT_CLI_PORT]==cliport){
			p->connected=1;
			printf("ok found ready mapping struct add to session now\n");
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
	int i;
	struct mapping ** tmp;
	struct timeval  now;
__retry:
	pthread_mutex_lock(&ready_list_lock);
	tmp=&ready_list;
	gettimeofday(&now,NULL);
	while((*tmp)!=NULL){
		p=(*tmp);
		/*
		if(p->ip==ip&&p->dst_port[NAT_CLI_PORT]==cliport){
			if(p->sess!=NULL){
				pthread_mutex_lock(&p->sess->sesslock);
				p->sess->running = 0;
				pthread_mutex_unlock(&p->sess->sesslock);
				pthread_mutex_unlock(&ready_list_lock);
				pthread_join(p->sess->tid,NULL);
				goto __retry;
			}else{
				*tmp=(*tmp)->next;
				for(i=1;i<PORT_COUNT;i++)
					close(p->udpsock[i]);
				put_local_port(p->local_port[NAT_V_PORT]);
				free(p);
				ready_count--;
				continue;
			}
		}
		*/
		if(p->connected==0&&(abs(now.tv_sec-p->aged.tv_sec)>READY_STRUCT_TIME_OUT)){
			*tmp=(*tmp)->next;
			for(i=1;i<PORT_COUNT;i++)
				close(p->udpsock[i]);
			put_local_port(p->local_port[NAT_V_PORT]);
			pthread_mutex_destroy(&p->mapping_lock);
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
void del_from_ready_list(struct mapping *p)
{
	struct mapping ** tmp;
	pthread_mutex_lock(&ready_list_lock);
	tmp=&ready_list;
	while((*tmp)!=NULL&&p!=(*tmp))
		tmp=&((*tmp)->next);
	if((*tmp)!=NULL){
		*tmp=(*tmp)->next;
		ready_count--;
	}
	pthread_mutex_unlock(&ready_list_lock);
}
int remove_from_ready_list(uint32_t addr,uint16_t cliport)
{
	struct mapping *p;
	struct mapping ** tmp;
	pthread_mutex_lock(&ready_list_lock);
	tmp=&ready_list;
	while((*tmp)!=NULL&&((*tmp)->ip!=addr||(*tmp)->dst_port[NAT_CLI_PORT]!=cliport))
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
		put_local_port(p->local_port[NAT_V_PORT]);
		/*
		* do something;
		*/
		free(p);
		return 0;
	}
	return -1;
}

static inline int touch_port(int sockfd,uint32_t ip,uint16_t port)
{
	int i;
	int ret;
	struct sockaddr_in from,localaddr;
	socklen_t fromlen;
	  from.sin_family=AF_INET;
	  fromlen=sizeof(struct sockaddr_in);
	  from.sin_addr.s_addr=ip;
	  from.sin_port=port;
	  for(i = 0;i<3;i++){
		  ret = sendto(sockfd, "touch", strlen("touch"), 0,(struct sockaddr *) &from, fromlen);
		  printf("send touch to ip:%s  ; port:%d ; return %d\n",inet_ntoa(from.sin_addr),ntohs(from.sin_port),ret);
		  fromlen=sizeof(struct sockaddr_in);
		   if(getsockname(sockfd,(struct sockaddr*)&localaddr,&fromlen) <0){
				printf("cannot get sock name\n");
				return -1;
		  }
		   printf("used cli ip:%s , port: %d\n",inet_ntoa(localaddr.sin_addr),ntohs(localaddr.sin_port));
	  }
	  return 0;
}

#define BUF_SZ 512
static int build_nat_addr(int sockfd,int recvsock)
{	
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

	 from.sin_addr.s_addr = server_addr;
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
		       printf("build local_port socket:%d error\n",sockfd);
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
	  printf("build local_port socket:%d ok\n",sockfd);
	  return 0;
}

int build_local_sock(uint16_t port)
{
	int sockfd;
	int on;
	struct sockaddr_in *s;
	if ((sockfd = create_udp_socket()) < 0) {
       	 perror("Error creating socket\n");
        	return -1;
   	 }
	on = 1;
	 if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
              printf("Error enabling socket address reuse\n");
        }
  	  if ((s=bind_udp_socket(sockfd, port)) == NULL) {
     	   	 printf("****************************Error binding udp socket****************************");
      	  	 close(sockfd);
       	 return -1;
   	 }
	  free(s);
	  return sockfd; 
}

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
	int i;
	struct timeval oldtime,newtime;
//	struct sockaddr_in *saddr;
       struct sockaddr_in from;
	//cmd_t cmd,replay;
	struct udp_transfer recvaddr;
	struct udp_transfer *pdest=&recvaddr;
	struct mapping*p;
	struct timeval timeout;
	struct hostent *host;
	struct sockaddr_in *s;
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
  	  if ((s=bind_udp_socket(sockfd, LOCAL_ALIVE_PORT)) == NULL) {
     	   	 printf("****************************Error binding udp socket****************************");
      	  	 close(sockfd);
       	 return -1;
   	 }
	  free(s);
	  //pthread_mutex_lock(&threadcfg.threadcfglock);
	  if( (server_addr=inet_addr(threadcfg.server_addr))==INADDR_NONE)
    	{
	        if((host=gethostbyname(threadcfg.server_addr) )==NULL) 
	        {
	           perror("server address gethostbyname error");
	           exit(1);
	        }
	        memcpy( &server_addr,host->h_addr,host->h_length);
    	}
	// pthread_mutex_unlock(&threadcfg.threadcfglock);
		from.sin_addr.s_addr = server_addr;
		from.sin_family=AF_INET;
		 from.sin_port=htons(CAMERA_WATCH_PORT);
		 printf("convert server addr sucess ip:%s\n",inet_ntoa(from.sin_addr));
		 fromlen=sizeof(struct sockaddr_in); 
		 while(1){ 
		 	camid=threadcfg.cam_id;
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
	gettimeofday(&oldtime,NULL);
	printf("camera set alive ok\n");
	while(1){
		gettimeofday(&newtime,NULL);
		if(abs(newtime.tv_sec-oldtime.tv_sec)>10){
			 from.sin_addr.s_addr = server_addr;
			 from.sin_family=AF_INET;
			 from.sin_port=htons(CAMERA_WATCH_PORT);
			 fromlen=sizeof(struct sockaddr_in); 
			req[0]=0;
    			 sendto(sockfd, req, 1, 0,(struct sockaddr *) &from, fromlen); 
			memcpy(&oldtime,&newtime,sizeof(struct timeval));
		}
		req_len=recvfrom(sockfd, req, BUF_SZ, 0, (struct sockaddr *) &from, &fromlen);
		if(req_len<=0)
			continue;
		memcpy(&nsize,&req[1],sizeof(nsize));
		gettimeofday(&oldtime,NULL);
		size=ntohs(nsize); 
		if(req_len>0&&req_len==size+PACK_HEAD_SIZE){
			printf("get command\n");
			printf("req[0]==%d\n",req[0]);
			printf("size==%d\n",size);
			printf("udp_transfer size==%d\n",sizeof(struct udp_transfer));
			switch(req[0]){
				
				case NET_CAMERA_SEND_PORTS:
				{
					printf("NET_CAM_SEND_PORTS\n");
					if(size!=sizeof(struct udp_transfer))
						break;
					memcpy(pdest,&req[PACK_HEAD_SIZE],sizeof(struct udp_transfer));
					printf("before delete_timeout_mapping\n");
					printf("ready_count==%d\n",get_ready_count());
					printf("read_list==%p\n",ready_list);
					delete_timeout_mapping( pdest->ip,  pdest->port[NAT_CLI_PORT]);
					ret=get_ready_count();
					printf("after delete_timeout_mapping\n");
					printf("ready_count==%d\n",get_ready_count());
					printf("read_list==%p\n",ready_list);
					if(ret>=(MAX_CONNECTIONS<<2))
						break;
					p=(struct mapping *)malloc(sizeof(struct mapping));
					if(!p){
						printf("unable to malloc struct mapping\n");
						break;
					}
					p->local_port[NAT_CLI_PORT] = CLI_PORT;
					p->local_port[NAT_V_PORT]=get_local_port();
					p->local_port[NAT_A_PORT]=p->local_port[NAT_V_PORT]+1;
					p->udpsock[NAT_CLI_PORT] = g_cli_ctx->sock;
					p->udpsock[NAT_V_PORT] = build_local_sock(p->local_port[NAT_V_PORT]);
					if(p->udpsock[NAT_V_PORT]<0){
						put_local_port(p->local_port[NAT_V_PORT]);
						free(p);
						break;
					}
					p->udpsock[NAT_A_PORT] = build_local_sock(p->local_port[NAT_A_PORT]);
					if(p->udpsock[NAT_A_PORT]<0){
						close(p->udpsock[NAT_V_PORT]);
						put_local_port(p->local_port[NAT_V_PORT]);
						free(p);
						break;
					}
					for(i=0;i<PORT_COUNT;i++){
						if((ret=build_nat_addr(p->udpsock[i], sockfd))<0){
							close(p->udpsock[NAT_V_PORT]);
							close(p->udpsock[NAT_A_PORT]);
							put_local_port(p->local_port[NAT_V_PORT]);
							free(p);
							break;
						}
					}
					if(ret>=0){
						gettimeofday(&(p->aged),NULL);
						p->connected=0;
						p->ip = pdest->ip;
						p->dst_port[NAT_CLI_PORT] = pdest->port[NAT_CLI_PORT];
						p->dst_port[NAT_V_PORT] = pdest->port[NAT_V_PORT];
						p->dst_port[NAT_A_PORT] = pdest->port[NAT_A_PORT];
						p->ucount = 0;
						for(i=0;i<PORT_COUNT;i++)
							p->udtsock[i] =-1;

						p->sess=NULL;
						pthread_mutex_init(&p->mapping_lock,NULL);
						for(i=0;i<PORT_COUNT;i++)
							touch_port(p->udpsock[i], p->ip, p->dst_port[i]);
						add_to_ready_list( p);
						printf("after add to ready list\n");
						printf("ready_count==%d\n",get_ready_count());
						printf("read_list==%p\n",ready_list);

						struct in_addr inaddr;
						inaddr.s_addr=p->ip;
						printf("before conver ip==%x\n",pdest->ip);
						printf("\n##########################CAMERA ###############################\n");
						printf("connected in ip:%s\n",inet_ntoa(inaddr));
						printf("cli port:%d\n",ntohs(pdest->port[NAT_CLI_PORT]));
						printf("video port:%d\n",ntohs(pdest->port[NAT_V_PORT]));
						printf("audio port:%d\n",ntohs(pdest->port[NAT_A_PORT]));
						printf("\n#########################################################\n");
						
					}
					break;
				}
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
	init_local_m_data();
	if (pthread_create(&tid, NULL, (void *) transfer_thread,NULL) < 0) {
		return -1;
	} 
	return 0;
}










