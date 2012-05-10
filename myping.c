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
/*У����㷨*/
unsigned short cal_chksum(unsigned short *addr,int len)
{
   int nleft=len;
   int sum=0;
   unsigned short *w=addr;
   unsigned short answer=0;

   /*��ICMP��ͷ������������2�ֽ�Ϊ��λ�ۼ�����*/
   while(nleft>1)
   {
     sum+=*w++;
     nleft-=2;
   }
   /*��ICMP��ͷΪ�������ֽڣ���ʣ�����һ�ֽڡ������һ���ֽ���Ϊһ��2�ֽ����ݵĸ��ֽڣ�
   ���2�ֽ����ݵĵ��ֽ�Ϊ0�������ۼ�*/
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
/*����ICMP��ͷ*/
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
    gettimeofday(tval,NULL); /*��¼����ʱ��*/
    icmp->icmp_cksum=cal_chksum( (unsigned short *)icmp,packsize); /*У���㷨*/
    return packsize;
}


/*��������ICMP����*/
int send_packet()
{
    int packetsize;
      nsend++;
      packetsize=pack(nsend); /*����ICMP��ͷ*/
      if(sendto(sockfd,sendpacket,packetsize,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr))<0 )
      {
        perror("sendto error");
        return -1;
      }
      return 0;
}

/*��������ICMP����*/
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
       gettimeofday(&tvrecv,NULL); /*��¼����ʱ��*/
       if(unpack(recvpacket,n)==-1) {
	   	printf("unpack tell error\n");
	   	return -1;
       }
       nreceived++;
	return 0;
}
/*��ȥICMP��ͷ*/
int unpack(char *buf,int len)
{
    int iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;
    double rtt;
    ip=(struct ip *)buf;
    iphdrlen=ip->ip_hl<<2; /*��ip��ͷ����,��ip��ͷ�ĳ��ȱ�־��4*/
    icmp=(struct icmp *)(buf+iphdrlen); /*Խ��ip��ͷ,ָ��ICMP��ͷ*/
    len-=iphdrlen; /*ICMP��ͷ��ICMP���ݱ����ܳ���*/
    if(len<8) /*С��ICMP��ͷ�����򲻺���*/
    {
      printf("ICMP packets\'s length is less than 8\n");
      return -1;
    }
    /*ȷ�������յ����������ĵ�ICMP�Ļ�Ӧ*/
    if( (icmp->icmp_type==ICMP_ECHOREPLY) && (icmp->icmp_id==pid) )
    {
        tvsend=(struct timeval *)icmp->icmp_data;
        tv_sub(&tvrecv,tvsend); /*���պͷ��͵�ʱ���*/
        rtt=tvrecv.tv_sec*1000+tvrecv.tv_usec/1000; /*�Ժ���Ϊ��λ����rtt*/
        /*��ʾ�����Ϣ*/
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
    /*����ʹ��ICMP��ԭʼ�׽���,�����׽���ֻ��root��������*/
    if( (sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
    {
        perror("socket error");
        exit(1);
    }
    timeout.tv_sec = 5;
    timeout.tv_usec  = 0;
    /* ����rootȨ��,���õ�ǰ�û�Ȩ��*/
   // setuid(getuid());
    /*�����׽��ֽ��ջ�������50K��������ҪΪ�˼�С���ջ����������
    �Ŀ�����,��������pingһ���㲥��ַ��ಥ��ַ,������������Ӧ��*/
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

    /*�ж�������������ip��ַ*/
    if( (inaddr=inet_addr(argv[1]))==INADDR_NONE)
    {
        if((host=gethostbyname(argv[1]) )==NULL) /*��������*/
        {
           perror("gethostbyname error");
           exit(1);
        }
        memcpy( (char *)&dest_addr.sin_addr,host->h_addr,host->h_length);
    }
    else /*��ip��ַ*/
    memcpy( (char *)&dest_addr.sin_addr.s_addr,(char *)&inaddr,sizeof(in_addr_t));
    /*��ȡmain�Ľ���id,��������ICMP�ı�־��*/
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
/*����timeval�ṹ���*/
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



