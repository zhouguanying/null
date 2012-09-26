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
#include <netdb.h>

#include "includes.h"
#include "server.h"

#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)

// the next code for email alarm so boring code it seen my heartbeat stop when I typing it 

#define MAX_DATA_IN_LIST  		 3
#define MAILSERVER 				 "smtp.126.com"
#define RECEIVER 	 			 "linrizeng@sina.com"
#define SENDER   	 				 "ipedsender@126.com"
#define CONTENT 	 				 "email alarm!!"
#define USERNAME 				 "ipedsender@126.com"
#define PASSWD					 "iped2012iped2012"


#define CMDBUF_LEN				 0x400
    
struct mail_attach_data{
	char *image;
	unsigned int size;
	struct mail_attach_data*next;
};
static  struct  __attach_data_list_head{
	int count;
	int maxsize;  //max imge size,use to malloc buffer for text(include all this images)
	pthread_mutex_t  mail_data_lock;
	struct mail_attach_data *attach_data_list;
	struct mail_attach_data *data_list_tail;
}attach_data_list_head;

static struct {
	int count;
	int maxsize;
	struct mail_attach_data*attach_data_list;
}private_list_head;


static char table[]=
{'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/','='};
      
char mailserver[64];
char receiver[64];
char sender[64];
char sendername[64];
char senderpswd[64];

static char content[CMDBUF_LEN]=CONTENT;
static char boundary[]="000XMAIL000";
static unsigned int    mailfileno=0;
static unsigned int    mailsubjectno=0;

static inline char * gettimestamp()
{
	static char timestamp[32];
	time_t t;
	struct tm *curtm;
	 if(time(&t)==-1){
        	 printf("get time error\n");
         	 exit(0);
   	  }
	 curtm=localtime(&t);
	  sprintf(timestamp,"%04d-%02d-%02d-%02d-%02d-%02d",curtm->tm_year+1900,curtm->tm_mon+1,
                curtm->tm_mday,curtm->tm_hour,curtm->tm_min,curtm->tm_sec);
	 return timestamp;
}

void init_mail_attatch_data_list(char *mailbox){
	memset(&attach_data_list_head,0,sizeof(attach_data_list_head));
	pthread_mutex_init(&attach_data_list_head.mail_data_lock,NULL);
	memset(receiver,0,64);
	memcpy(receiver,mailbox,64);
	memcpy(sendername , sender , 64);
}

int add_image_to_mail_attatch_list_no_block(char *image,int size){
	struct mail_attach_data*p;
	pthread_mutex_lock(&attach_data_list_head.mail_data_lock);
	if(attach_data_list_head.count>=MAX_DATA_IN_LIST){
		pthread_mutex_unlock(&attach_data_list_head.mail_data_lock);
		free(image);
		return -1;
	}
	p=malloc(sizeof(struct mail_attach_data));
	if(!p){
		pthread_mutex_unlock(&attach_data_list_head.mail_data_lock);
		free(image);
		return -1;
	}
	p->image=image;
	p->size=size;
	p->next=NULL;
	if(size>attach_data_list_head.maxsize)
		attach_data_list_head.maxsize=size;
	if(attach_data_list_head.data_list_tail==NULL){
		attach_data_list_head.attach_data_list=p;
		attach_data_list_head.data_list_tail=p;
	}else{
		attach_data_list_head.data_list_tail->next=p;
		attach_data_list_head.data_list_tail=p;
	}
	attach_data_list_head.count++;
	pthread_mutex_unlock(&attach_data_list_head.mail_data_lock);
	return 0;
}

static int checkreply(char *str,char *buf)
{
	if(strncmp(str,buf,3)!=0){
		printf("check replay faile str:%s\n",str);
		return -1;
	}
	return 0;
}
static int en_base64(char *sbuf,int ssize,char dbuf[],int *dsize)
{
	char *s;
	char *d;
	int i,sid,did,len,flag;
	int tlen;
	s=sbuf;
	i=0;
	len=ssize;
	flag=len%3;
	len=len/3;
	d=dbuf;
	for(sid=0,did=0;len>0;len--)
	{
		d[did]=(s[sid]&0xfc)>>2;
		d[did+1]=((s[sid]&0x3)<<4)|((s[sid+1]&0xf0)>>4);
		d[did+2]=((s[sid+1]&0x0f)<<2)|((s[sid+2]&0xc0)>>6);
		d[did+3]=s[sid+2]&0x3f;
		sid+=3;
		did+=4;
	}

	if(flag>0)
	{
		if(flag==1)
		{
			d[did]=(s[sid]&0xfc)>>2;
			d[did+1]=(s[sid]&0x3)<<4;
			d[did+2]=64;
			d[did+3]=64;
			did+=4;
		}

		if(flag==2)
		{
			d[did]=(s[sid]&0xfc)>>2;
			d[did+1]=((s[sid]&0x3)<<4)|((s[sid+1]&0xf0)>>4);
			d[did+2]=(s[sid+1]&0x0f)<<2;
			d[did+3]=64;
			did+=4;
		}
	}
      
	tlen=did;
	for(did=0;did<tlen;did++)
	{
		d[did]=table[(int)d[did]];
	}
	d[did]='\0';
	if(dsize!=NULL)
		*dsize=tlen;
	return 0;
}

static int sendtext(int sockfd,char *ptext)
{
	struct mail_attach_data *ap;
	char *p;
	char subject[32]="";
	int imagecodesize;
	char *image;
	int size;
	char filename[32]="";
	int len=0;
	int i;
	int ret;
	memset(ptext,0,0x400+private_list_head.maxsize*2);
	memset(subject,0,sizeof(subject));
	sprintf(subject,"email alarm_%u_%x_%s",mailsubjectno,threadcfg.cam_id , gettimestamp());
	mailsubjectno++;
	p=ptext;
	/*mail head*/
	sprintf(p,"To:<%s>\r\n",receiver);
	strcat(p,"From:<");
	strcat(p,sender);
	strcat(p,">\r\n");
	strcat(p,"MIME-Version:1.0");
	strcat(p,"\r\n");
	strcat(p,"Content-Type: multipart/mixed; boundary=");
	strcat(p,boundary);
	strcat(p,"\r\n");
	strcat(p,"Subject:");
	strcat(p,subject);
	strcat(p,"\r\n");
	strcat(p,"\r\n");
	strcat(p,"This is a multi-part message in MIME format.");
	strcat(p,"\r\n");
	strcat(p,"--");
	strcat(p,boundary);
	strcat(p,"\r\n");
	/*mail text*/
	strcat(p,"Content-Type: text/plain");
	strcat(p,"\r\n");
	strcat(p,"Content-Transfer-Encoding: 8bit");
	strcat(p,"\r\n");
	strcat(p,"\r\n");
	sprintf(content , "mail alarm  no %u camera id %x time %s",mailsubjectno - 1 , threadcfg.cam_id ,gettimestamp());
	strcat(p,content);
	strcat(p,"\r\n");
/*mail attachment*/
	len=strlen(p);
	p+=len;
	while(private_list_head.attach_data_list!=NULL){
		ap=private_list_head.attach_data_list;
		private_list_head.attach_data_list=private_list_head.attach_data_list->next;
		image=ap->image;
		size=ap->size;
		free(ap);
		memset(filename,0,sizeof(filename));
		sprintf(filename,"alarm_%u_%x_%s.jpg", mailfileno,threadcfg.cam_id , gettimestamp());
		mailfileno++;
		strcat(p,"--");
		strcat(p,boundary);
		strcat(p,"\r\n");
		strcat(p,"Content-Type: name=");   
		strcat(p,filename);
		strcat(p,"\r\n");
		strcat(p,"Content-Transfer-Encoding: base64");
		strcat(p,"\r\n");
		strcat(p,"Content-Disposition: inline; filename=");
		strcat(p,filename);
		strcat(p,"\r\n");
		strcat(p,"\r\n");
		//strcat(p,attachbuf);
		len+=strlen(p);
		p+=strlen(p);
		en_base64(image,size,p,&imagecodesize);
		free(image);
		len+=imagecodesize;
		p+=imagecodesize;
		strcat(p,"\r\n");
		len+=strlen(p);
		p+=strlen(p);
		i = 0;
		while(len> 0 ) {
			if( len >= 1000 ) {
				ret = send(sockfd, ptext+i, 1000,0);
				if(ret==-1){
					len=-1;
					goto __error;
				}
				len -= ret;
				i += ret;
			} else {
				ret = send(sockfd, ptext+i, len,0);
				if(ret==-1){
					len=-1;
					goto __error;
				}
			len-=ret;
			i+=ret;
			}
		}
		memset(ptext,0,0x400+private_list_head.maxsize*2);
		p=ptext;
		len=0;
	}
	strcat(p,"--");
	strcat(p,boundary);
	strcat(p,"\r\n");
	/*for mail end*/
	strcat(p,"\r\n.\r\n");
	len+=strlen(p);
	len=send(sockfd,ptext,len,0);
__error:
	//the memory may fail to malloc for sbuf or attachbuf,desert the last image that cannot send
	while(private_list_head.attach_data_list!=NULL){
		ap=private_list_head.attach_data_list;
		private_list_head.attach_data_list=private_list_head.attach_data_list->next;
		free(ap->image);
		free(ap);
		printf("in sendtext func free image that not send\n");
	}
	/*sendtext over*/
	return len;
}

 static int mail_alarm(){
 	struct mail_attach_data*ap;
	int ret;
	char buf[0x400];
	struct hostent*sina_mail_ent;
	struct sockaddr_in sina_mail_addr;
	int sockfd=-1;
	char *ptext;
	
	sina_mail_ent=gethostbyname(mailserver);
	if(sina_mail_ent==NULL){
		printf("error get sina mail ent\n");
		goto __error;
	}
		
	memcpy(&sina_mail_addr.sin_addr,sina_mail_ent->h_addr_list[0],sizeof(struct in_addr));
	sina_mail_addr.sin_family=AF_INET;
	sina_mail_addr.sin_port=htons(25);

	sockfd=-1;
	sockfd=socket(AF_INET,SOCK_STREAM,0);
	ret=connect(sockfd,(void *)&sina_mail_addr,sizeof(sina_mail_addr));
	if(ret==-1){
		perror("connect error!\n");
		goto __error;
	}
	printf("connect sucess\n");
	memset(buf,0,sizeof(buf));
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("recv error!\n");
		goto __error;
	}
	memset(buf,0,sizeof(buf));
	sprintf(buf,"EHLO Server\r\n");
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		perror("send EHLO error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after send EHLO recv error\n");
		goto __error;
	}
	buf[ret]='\0';
	ret=checkreply("250", buf);
	if(ret<0){
		goto __error;
	}
	memset(buf,0,sizeof(buf));
	sprintf(buf,"AUTH LOGIN\r\n");
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		printf("send login error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after set login recv error\n");
		goto __error;
	}
	buf[ret]='\0';
	ret=checkreply("334",  buf);
	if(ret<0)
		goto __error;
	memset(buf,0,sizeof(buf));
	en_base64(sendername,strlen(sendername),buf,NULL);
	strcat(buf,"\r\n");
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		perror("send username error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("afer set username recv error\n");
		goto __error;
	}
	buf[ret]='\0';
	ret=checkreply("334", buf);
	if(ret<0){
		close(sockfd);
		printf(" user name fail try again\n");
		goto __error;
	}
	memset(buf,0,sizeof(buf));
	en_base64(senderpswd,strlen(senderpswd),buf,NULL);
	strcat(buf,"\r\n");
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		perror("send passwd error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after set passwd recv fail\n");
		goto __error;
	}
	buf[ret]='\0';
	ret=checkreply("235",  buf);
	if(ret<0){
		close(sockfd);
		printf("passwd fail try again\n");
		goto __error;
	}
	printf("login sucess!\n");
	memset(buf,0,sizeof(buf));
	sprintf(buf,"MAIL FROM:<%s>\r\n",sender);
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		perror("send MAIL FROM error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after send MAIL FROM recv error\n");
		goto __error;
	}
	buf[ret]='\0';
	ret = checkreply("250", buf);
	if(ret <0)
		goto __error;
	memset(buf,0,sizeof(buf));
	sprintf(buf,"RCPT TO:<%s>\r\n",receiver);
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		printf("send RCPT TO error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after send RCPT TO recv error\n");
		goto __error;
	}
	buf[ret]='\0';
	ret=checkreply("250",  buf);
	if(ret<0)
		goto __error;
	memset(buf,0,sizeof(buf));
	sprintf(buf,"DATA\r\n");
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		printf("send DATA error\n");
		goto __error;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after send DATA recv error\n");
		goto __error;
	}
	buf[ret]='\0';
	ret=checkreply("354",  buf);
	if(ret<0)
		goto __error;
	ptext=(char *)malloc(0x400+private_list_head.maxsize*2);
	if(!ptext){
		goto __error;
	}
	ret=sendtext(sockfd,ptext);
	free(ptext);
	if(ret<0)
		goto __end;
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after set mail recv data error\n");
		goto __end;
	}
	buf[ret]='\0';
	ret=checkreply("250", buf);
	if(ret<0)
		goto __end;
	memset(buf,0,sizeof(buf));
	sprintf(buf,"QUIT\r\n");
	ret=send(sockfd,buf,strlen(buf),0);
	if(ret==-1){
		perror("send QUIT error!\n");
		goto __end;
	}
	ret=recv(sockfd,buf,sizeof(buf),0);
	if(ret==-1){
		perror("after send QUIT recv error\n");
		goto __end;
	}
	buf[ret]='\0';
	ret=checkreply("221",  buf);
	if(ret<0)
		goto __end;
	close(sockfd);
	printf("OK send mail sucess\n");
	return 0;
__error:
	while(private_list_head.attach_data_list!=NULL){
		ap=private_list_head.attach_data_list;
		private_list_head.attach_data_list=private_list_head.attach_data_list->next;
		free(ap->image);
		free(ap);
		printf("in mail alarm func free imge that not send\n");
	}
__end:
	printf("send mail fail\n");
	if(sockfd>=0)
		close(sockfd);
	return -1;
}
 int mail_alarm_thread(){
 	struct mail_attach_data *ap;
 	dbg("###############mail alarm thread ok###################\n");
 	while(1){
		pthread_mutex_lock(&attach_data_list_head.mail_data_lock);
		if(!attach_data_list_head.attach_data_list||attach_data_list_head.count<=0){
			pthread_mutex_unlock(&attach_data_list_head.mail_data_lock);
			usleep(500000);
			continue;
		}
		private_list_head.count=attach_data_list_head.count;
		private_list_head.maxsize=attach_data_list_head.maxsize;
		private_list_head.attach_data_list=attach_data_list_head.attach_data_list;
		attach_data_list_head.count=0;
		attach_data_list_head.maxsize=0;
		attach_data_list_head.attach_data_list=NULL;
		attach_data_list_head.data_list_tail=NULL;
		pthread_mutex_unlock(&attach_data_list_head.mail_data_lock);
		if(receiver[0]&&sender[0]&&senderpswd[0]&&mailserver[0]){
			printf("mail box =%s\n",receiver);
			mail_alarm();
		}else{
			dbg("#############mail alarm receiver not found#################\n");
			while(private_list_head.attach_data_list!=NULL){
				ap=private_list_head.attach_data_list;
				private_list_head.attach_data_list=private_list_head.attach_data_list->next;
				free(ap->image);
				free(ap);
			}
		}
 	}
 	return 0;
 }
