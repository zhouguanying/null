#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <setjmp.h>
#include <errno.h>
#include <net/if.h>
#include <sys/msg.h>

#include "server.h"
#include "vpu_server.h"

#define PACKET_SIZE 4096
#define MAX_WAIT_TIME 5
#define MAX_NO_PACKETS 3


char curr_device[32] = "wlan0";
char __gate_way[32];

static int enable_wlan0 =0;
static char device[32] = "eth0";
static char sendpacket[PACKET_SIZE];
static char recvpacket[PACKET_SIZE];
static int sockfd;
static int datalen=56;
static int nsend=0,nreceived=0;
static struct sockaddr_in dest_addr;
static pid_t pid;
static struct sockaddr_in from;
static struct timeval tvrecv;

void statistics(int signo);
unsigned short cal_chksum(unsigned short *addr,int len);
int pack(int pack_no);
int send_packet();
int recv_packet();
int unpack(char *buf,int len);
void tv_sub(struct timeval *out,struct timeval *in);

void statistics(int signo)
{
   printf("\n--------------------PING statistics-------------------\n");
   printf("%d packets transmitted, %d received , %%%d lost\n",nsend,nreceived,(int)((float)((nsend-nreceived)/nsend))*100);
   close(sockfd);
   exit(1);
}
/*
int device_cmp()
{
	return (strncmp(curr_device,device,strlen(curr_device)));
}
*/
/*校验和算法*/
unsigned short cal_chksum(unsigned short *addr,int len)
{
   int nleft=len;
   int sum=0;
   unsigned short *w=addr;
   unsigned short answer=0;

   /*把ICMP报头二进制数据以2字节为单位累加起来*/
   while(nleft>1)
   {
     sum+=*w++;
     nleft-=2;
   }
   /*若ICMP报头为奇数个字节，会剩下最后一字节。把最后一个字节视为一个2字节数据的高字节，
   这个2字节数据的低字节为0，继续累加*/
   if( nleft==1)
   {
       *(unsigned char *)(&answer)=*(unsigned char *)w;
       sum+=answer;
   }
   sum=(sum>>16)+(sum&0xffff);
   sum+=(sum>>16);
   answer=~sum;
   return answer;
}
/*设置ICMP报头*/
int pack(int pack_no)
{
    int packsize;
    struct icmp *icmp;
    struct timeval *tval;
    icmp=(struct icmp*)sendpacket;
    icmp->icmp_type=ICMP_ECHO;
    icmp->icmp_code=0;
    icmp->icmp_cksum=0;
    icmp->icmp_seq=pack_no;
    icmp->icmp_id=pid;
    packsize=8+datalen;
    tval= (struct timeval *)icmp->icmp_data;
    gettimeofday(tval,NULL); /*记录发送时间*/
    icmp->icmp_cksum=cal_chksum( (unsigned short *)icmp,packsize); /*校验算法*/
    return packsize;
}


/*发送三个ICMP报文*/
int send_packet()
{
    int packetsize;
      nsend++;
      packetsize=pack(nsend); /*设置ICMP报头*/
      if(sendto(sockfd,sendpacket,packetsize,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr))<0 )
      {
        perror("sendto error");
        return -1;
      }
      return 0;
}

/*接收所有ICMP报文*/
int recv_packet()
{
     int n;
     socklen_t fromlen;
     extern int errno;
     //signal(SIGALRM,statistics);
     fromlen=sizeof(from);
     //  alarm(MAX_WAIT_TIME);
    __retry:
       if( (n=recvfrom(sockfd,recvpacket,sizeof(recvpacket),0,(struct sockaddr *)&from,&fromlen)) <0)
       {
           if(errno==EINTR) goto __retry;
           perror("recvfrom error");
           return -1;
       }
       gettimeofday(&tvrecv,NULL); /*记录接收时间*/
       if(unpack(recvpacket,n)==-1) {
	   	printf("unpack tell error\n");
	   	return -1;
       }
       nreceived++;
	return 0;
}
/*剥去ICMP报头*/
int unpack(char *buf,int len)
{
    int iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;
    double rtt;
    ip=(struct ip *)buf;
    iphdrlen=ip->ip_hl<<2; /*求ip报头长度,即ip报头的长度标志乘4*/
    icmp=(struct icmp *)(buf+iphdrlen); /*越过ip报头,指向ICMP报头*/
    len-=iphdrlen; /*ICMP报头及ICMP数据报的总长度*/
    if(len<8) /*小于ICMP报头长度则不合理*/
    {
      printf("ICMP packets\'s length is less than 8\n");
      return -1;
    }
    /*确保所接收的是我所发的的ICMP的回应*/
    if( (icmp->icmp_type==ICMP_ECHOREPLY) && (icmp->icmp_id==pid) )
    {
        tvsend=(struct timeval *)icmp->icmp_data;
        tv_sub(&tvrecv,tvsend); /*接收和发送的时间差*/
        rtt=tvrecv.tv_sec*1000+tvrecv.tv_usec/1000; /*以毫秒为单位计算rtt*/
        /*显示相关信息*/
        printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3f ms\n",
        len,inet_ntoa(from.sin_addr),icmp->icmp_seq,ip->ip_ttl,rtt);
    }
    else return -1;
	return 0;
}
int check_net(char *ping_addr,char __device[32])
{
    struct hostent *host;
    unsigned long inaddr=0l;
    struct sockaddr_in addr;
    socklen_t addr_len;
    int size=50*1024;
    struct timeval timeout;
    struct ifreq ifr;
    char *argv[2];
    int n = 10;
    argv[1] = ping_addr;
    memcpy(device,__device,32);
    /*生成使用ICMP的原始套接字,这种套接字只有root才能生成*/
    if( (sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
    {
        perror("socket error");
        exit(1);
    }
    timeout.tv_sec = 5;
    timeout.tv_usec  = 0;
    /* 回收root权限,设置当前用户权限*/
   // setuid(getuid());
    /*扩大套接字接收缓冲区到50K这样做主要为了减小接收缓冲区溢出的
    的可能性,若无意中ping一个广播地址或多播地址,将会引来大量应答*/
     memset(&ifr, 0, sizeof(ifr));
     strncpy(ifr.ifr_name, device, IFNAMSIZ-1);
   if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr)) == -1){
   	printf("unable to bind device %s\n",device);
	close(sockfd);
	exit(0);
   }
   addr_len = sizeof(struct sockaddr);
   if(getsockname(sockfd,(struct sockaddr*)&addr,&addr_len) <0){
   	printf("cannot get sock name\n");
	close(sockfd);
	return -1;
   }
   printf("bind to device %s , ip %s\n",device,inet_ntoa(addr.sin_addr));
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
            printf("Error enabling socket rcv time out \n");
    }
    setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size) );
    bzero(&dest_addr,sizeof(dest_addr));
    dest_addr.sin_family=AF_INET;

    /*判断是主机名还是ip地址*/
    if( (inaddr=inet_addr(argv[1]))==INADDR_NONE)
    {
        if((host=gethostbyname(argv[1]) )==NULL) /*是主机名*/
        {
           perror("gethostbyname error");
           exit(1);
        }
        memcpy( (char *)&dest_addr.sin_addr,host->h_addr,host->h_length);
    }
    else /*是ip地址*/
    memcpy( (char *)&dest_addr.sin_addr.s_addr,(char *)&inaddr,sizeof(in_addr_t));
    /*获取main的进程id,用于设置ICMP的标志符*/
    pid=getpid();
    printf("PING %s(%s): %d bytes data in ICMP packets.\n",argv[1],
    inet_ntoa(dest_addr.sin_addr),datalen);
   // signal(SIGINT,statistics);
   for(n =0;n<5;n++){
   	send_packet();
	if(recv_packet() ==0){
		close(sockfd);
		return 0;
	}
   }
   close(sockfd);
   return -1;
}
/*
int main(int argc,char *argv[])
{
    struct hostent *host;
    unsigned long inaddr=0l;
   struct sockaddr_in addr;
    socklen_t addr_len;
    int waittime=MAX_WAIT_TIME;
    int size=50*1024;
    struct timeval timeout;
    struct ifreq ifr;

    if(argc<2)
    {
       printf("usage:%s hostname/IP address\n",argv[0]);
       exit(1);
    }

    if( (sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
    {
        perror("socket error");
        exit(1);
    }
    timeout.tv_sec = 5;
    timeout.tv_usec  = 0;
    setuid(getuid());
   
     memset(&ifr, 0, sizeof(ifr));
     strncpy(ifr.ifr_name, device, IFNAMSIZ-1);
   if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr)) == -1){
   	printf("unable to bind device %s\n",device);
	return -1;
   }
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0){
		printf("unable to bind addr\n");
    }
    memset(&addr,0,sizeof(addr));
   addr_len = sizeof(struct sockaddr);
   if(getsockname(sockfd,(struct sockaddr*)&addr,&addr_len) <0){
   	printf("cannot get sock name\n");
	return -1;
   }
   printf("bind to device %s , ip %s\n",device,inet_ntoa(addr.sin_addr));
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
            printf("Error enabling socket rcv time out \n");
    }
    setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size) );
    bzero(&dest_addr,sizeof(dest_addr));
    dest_addr.sin_family=AF_INET;

    
    if( (inaddr=inet_addr(argv[1]))==INADDR_NONE)
    {
        if((host=gethostbyname(argv[1]) )==NULL) 
        {
           perror("gethostbyname error");
           exit(1);
        }
        memcpy( (char *)&dest_addr.sin_addr,host->h_addr,host->h_length);
    }
    else 
    memcpy( (char *)&dest_addr.sin_addr.s_addr,(char *)&inaddr,sizeof(in_addr_t));
    
    pid=getpid();
    printf("PING %s(%s): %d bytes data in ICMP packets.\n",argv[1],
    inet_ntoa(dest_addr.sin_addr),datalen);
    signal(SIGINT,statistics);
    while(1){
		
	   if( send_packet()<0){
	   	nsend --;
		printf("send icmp packet error\n");
		continue;
	   }
	    recv_packet(); 
	   sleep(1);
    }
    statistics(SIGALRM); 
    return 0;

}
*/
/*两个timeval结构相减*/
void tv_sub(struct timeval *out,struct timeval *in)
{
     if( (out->tv_usec-=in->tv_usec)<0)
     {
         --out->tv_sec;
         out->tv_usec+=1000000;
     }
     out->tv_sec-=in->tv_sec;
}
extern int msqid;
int snd_soft_restart()
{
	int ret;
      vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_SOFTRESET;
	msg.msg[1] = 0;
__retry:
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		printf("send daemon message error\n");
		goto __retry;
	}
	printf("set soft reset\n");
	exit(0);
}

int check_net_thread()
{
	char ping_addr[32];
	char trydevice[32];
	int crrconected;
	int eth0_wan;
	int eth0_lan;
	int wlan0_wan;
	int wlan0_lan;
	while(1){
		eth0_lan = -1;
		eth0_wan =-1;
		wlan0_lan = -1;
		wlan0_wan=-1;
		
		/*check eth0 wan*/
		memset(ping_addr,0,32);
		memset(trydevice,0,32);
		sprintf(ping_addr,"www.baidu.com");
		sprintf(trydevice,"eth0");
		eth0_wan = check_net( ping_addr, trydevice);
		if(eth0_wan == 0)
			goto __check_ok;

		/*check wlan0 wan*/
		if(enable_wlan0){
			memset(ping_addr,0,32);
			memset(trydevice,0,32);
			sprintf(ping_addr,"www.baidu.com");
			sprintf(trydevice,"wlan0");
			wlan0_wan = check_net( ping_addr, trydevice);
			if(wlan0_wan == 0)
				goto __check_ok;
		}
		//check eth0 lan
		memset(ping_addr,0,32);
		memset(trydevice,0,32);
		sprintf(trydevice,"eth0");
		memcpy(ping_addr,__gate_way,32);
		eth0_lan = check_net( ping_addr, trydevice);
		if(eth0_lan == 0)
			goto __check_ok;

		//check wlan0 lan
		if(enable_wlan0){
			memset(ping_addr,0,32);
			memset(trydevice,0,32);
			sprintf(trydevice,"wlan0");
			memcpy(ping_addr,__gate_way,32);
			wlan0_lan = check_net( ping_addr, trydevice);
		}
__check_ok:
		printf("check return ok \n");
		if(eth0_wan ==0 ){
			printf("enter eth0_wan ==0\n");
			if(strncmp(curr_device,"eth0",4)!=0){
				pthread_mutex_lock(&global_ctx_lock);
				crrconected =currconnections;
				pthread_mutex_unlock(&global_ctx_lock);
				if(crrconected<=0)
					snd_soft_restart();
				else
					goto __done;
			}else
				goto __done;
		}
		if(wlan0_wan ==0){
			printf("enter wlan0_wan ==0\n");
			if(strncmp(curr_device,"wlan0",5)!=0){
				pthread_mutex_lock(&global_ctx_lock);
				crrconected =currconnections;
				pthread_mutex_unlock(&global_ctx_lock);
				if(crrconected<=0)
					snd_soft_restart();
				else
					goto __done;
			}else
				goto __done;
		}
		if(eth0_lan ==0){
			printf("enter eth0_lan == 0\n");
			if(strncmp(curr_device,"eth0",4)!=0){
				pthread_mutex_lock(&global_ctx_lock);
				crrconected =currconnections;
				pthread_mutex_unlock(&global_ctx_lock);
				if(crrconected<=0)
					snd_soft_restart();
				else
					goto __done;
			}else
				goto __done;
		}
		if(wlan0_lan == 0){
			printf("enter wlan0_lan == 0\n");
			if(strncmp(curr_device,"wlan0",5)!=0){
				pthread_mutex_lock(&global_ctx_lock);
				crrconected =currconnections;
				pthread_mutex_unlock(&global_ctx_lock);
				if(crrconected<=0)
					snd_soft_restart();
				else
					goto __done;
			}else
				goto __done;
		}
		/*all cannot connected ......*/
		printf("all disconnected  now reboot\n");
		system("reboot");
__done:
	sleep(10);
	}
	return 0;
}

int built_net(int check_wlan0)
{
	char ping_addr[32];
	char trydevice[32];
	int eth0_wan;
	int eth0_lan;
	int wlan0_wan;
	int wlan0_lan;
	eth0_lan = -1;
	eth0_wan = -1;
	wlan0_lan = -1;
	wlan0_wan = -1;

	/*check eth0 wan*/

	       enable_wlan0 = check_wlan0;
	
		memset(ping_addr,0,32);
		memset(trydevice,0,32);
		sprintf(ping_addr,"www.baidu.com");
		sprintf(trydevice,"eth0");
		eth0_wan = check_net( ping_addr, trydevice);
		if(eth0_wan == 0)
			goto __check_ok;

		/*check wlan0 wan*/
		if(check_wlan0){
			memset(ping_addr,0,32);
			memset(trydevice,0,32);
			sprintf(ping_addr,"www.baidu.com");
			sprintf(trydevice,"wlan0");
			wlan0_wan = check_net( ping_addr, trydevice);
			if(wlan0_wan == 0)
				goto __check_ok;
		}
		//check eth0 lan
		memset(ping_addr,0,32);
		memset(trydevice,0,32);
		sprintf(trydevice,"eth0");
		memcpy(ping_addr,__gate_way,32);
		eth0_lan = check_net( ping_addr, trydevice);
		if(eth0_lan == 0)
			goto __check_ok;

		//check wlan0 lan
		if(check_wlan0){
			memset(ping_addr,0,32);
			memset(trydevice,0,32);
			sprintf(trydevice,"wlan0");
			memcpy(ping_addr,__gate_way,32);
			wlan0_lan = check_net( ping_addr, trydevice);
		}
__check_ok:
		memset(curr_device,0,32);
		if(eth0_wan ==0 ){
			sprintf(curr_device,"eth0");
			printf("####################use eth0##############\n");
			goto __done;
		}
		if(wlan0_wan ==0){
			printf("####################use wlan0##############\n");
			sprintf(curr_device,"wlan0");
			goto __done;
		}
		if(eth0_lan ==0){
			printf("####################use eth0##############\n");
			sprintf(curr_device,"eth0");
			goto __done;
		}
		if(wlan0_lan == 0){
			printf("####################use wlan0##############\n");
			sprintf(curr_device,"wlan0");
			goto __done;
		}
		printf("######################all net can't use#################\n");
		return -1;
__done:
	return 0;
		/*all cannot connected ......*/
}



