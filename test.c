#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <sys/msg.h>
#include <assert.h>
#include <string.h>



#include "nand_file.h"
#include "utilities.h"
#include "cli.h"
#include "server.h"
#include "usb_detect.h"
#include "sound.h"
#include "playback.h"
#include "amixer.h"
#include "rtp.h"
#include "udp_transfer.h"
#include "vpu_server.h"
#include "cudt.h"
#include "video_cfg.h"

extern int check_net_thread();
extern int mywpa_cli(int argc, char *argv[]);
int built_net(int check_wlan0,int check_eth0 , int ping_wlan0 , int ping_eth0);
extern char *scanresult;
extern int result_len;
extern char inet_eth_device[64];
extern char inet_wlan_device[64];
extern char inet_eth_gateway[64];
extern char inet_wlan_gateway[64];

#if 1
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif


#define BUF_SIZE	1024*128

static nand_record_file_header record_header;
char* test_jpeg_file="/720_480.jpg";
extern int msqid;
struct configstruct{
	char name[64];
	char value[64];
};

int test_file_write()
{
	int i,j; 
	int * p;
	char* buf;
	int ret;

	buf = malloc(BUF_SIZE);
	if( !buf ){
		printf("malloc buf error\n");
		return -1;
	}

#if 1
		for( i = 0; i < 1000; i++ ){
			p = (int*)buf;
			for( j = 0; j < BUF_SIZE/4; j++ ){
				p[j] = i;
			}
	retry:
			ret = nand_write(buf, BUF_SIZE);
		//	dbg("ret=%d\n",ret);
			if( ret == 0 ){
		//		dbg("record_file_size=%d\n",record_file_size);
			}
			else if( ret == VS_MESSAGE_NEED_START_HEADER ){
				nand_prepare_record_header(&record_header);
				nand_write_start_header(&record_header);
				goto retry;
			}
			else if( ret == VS_MESSAGE_NEED_END_HEADER ){
				nand_prepare_close_record_header(&record_header);
				nand_write_end_header(&record_header);
				goto retry;
			}
		}
		nand_prepare_close_record_header(&record_header);
		nand_write_end_header(&record_header);
		nand_write(buf, BUF_SIZE);
		nand_flush();
#endif
#if 0
		int fd;
		FILE* fp;
		fd = open("/sdcard/ipcam_record/RECORD1.DAT", O_RDWR|O_CREAT);
		if( fd == -1 ){
			printf("open file error\n");
			return -1;
		}
		for( i = 0; i < 1024*100; i++ ){
			write(fd, buf, BUF_SIZE);
		}
		close(fd);
#endif
	return 0;
}

int test_jpeg_file_write()
{
	int ret;
	char* buf;
	int i; 
	int fd;
	struct stat st;

	fd = open(test_jpeg_file, O_RDONLY);
	if(fd==-1){
		printf("can not open input file\n");
		return -1;
	}
	
	if (stat(test_jpeg_file, &st) != 0) {
		return -1;
	}
	buf = malloc(st.st_size);
	if( !buf ){
		printf("malloc buf error\n");
		return -1;
	}
	read( fd, buf, st.st_size );
	close( fd );

	for( i = 0; i < 5000; i++ ){
retry:
		ret = nand_write(buf, st.st_size);
	//	dbg("ret=%d\n",ret);
		if( ret == 0 ){
	//		dbg("record_file_size=%d\n",record_file_size);
		}
		else if( ret == VS_MESSAGE_NEED_START_HEADER ){
			nand_prepare_record_header(&record_header);
			nand_write_start_header(&record_header);
			goto retry;
		}
		else if( ret == VS_MESSAGE_NEED_END_HEADER ){
			nand_prepare_close_record_header(&record_header);
			nand_write_end_header(&record_header);
			goto retry;
		}
	}
	nand_prepare_close_record_header(&record_header);
	nand_write_end_header(&record_header);

	return 0;

}

int test_cli(struct sess_ctx* system_sess)
{
	if( start_cli(system_sess) == NULL ){
		printf("cli start error\n");
		return -1;
	}
	printf("cli is running\n");
	while(1);
}

int test_video_monitor(struct sess_ctx* system_sess)
{
	pthread_t tid;

	if( start_cli(system_sess) == NULL ){
		printf("cli start error\n");
		return -1;
	}
	printf("cli is running\n");
//	start_video_monitor(system_sess);
	if (pthread_create(&tid, NULL, (void *) start_video_monitor, system_sess) < 0) {
		free_system_session(system_sess);
		return -1;
	} 
	printf("video monitor is running\n");
	while(1);
}

int test_video_record_and_monitor(struct sess_ctx* system_sess)
{
	pthread_t tid;
	vs_ctl_message msg;
	int ret;

	printf("cli is running\n");
	msqid = msgget(DAEMON_MESSAGE_QUEUE_KEY,0666);
	if(msqid ==-1){
		dbg("unable to open daemon message");
		exit (0);
	}
	if( start_cli(system_sess) == NULL ){
		printf("cli start error\n");
		return -1;
	}
	
	/*
	if (pthread_create(&tid, NULL, (void *) start_video_monitor, system_sess) < 0) {
		free_system_session(system_sess);
		return -1;
	} 
	*/
	
	/*
	if (pthread_create(&tid, NULL, (void *) grab_sound_thread, NULL) < 0) {
		return -1;
	} 
	*/
	
	sleep(1);
	if (pthread_create(&tid, NULL, (void *) start_video_record, system_sess) < 0) {
		free_system_session(system_sess);
		return -1;
	} 
	if(strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0){
		if (pthread_create(&tid, NULL, (void *) check_net_thread, NULL) < 0) {
			printf("unable to create check net thread\n");
			return -1;
		} 
	}
	playback_init();
	printf("video monitor and record is running\n");
	start_udt_lib();
	/*
	sleep(1);
	in_addr_t to;
	to = inet_addr("192.168.1.121");
	audiosess_add_dstaddr(to,htons(49150),htons(49151));
	*/
	

	
#define MONITOR 0
#if MONITOR
	monitor_try_connected_thread();
#else
	if( start_udp_transfer()<0){
		printf("start_udp_transfer error\n");
		exit(0);
	}
#endif

	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_MAIN_PROCESS_START;
	msg.msg[1] = 0;
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
		system("reboot &");
		exit(0);
	}
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_MAIN_PROCESS_ALIVE;
	msg.msg[1] = 0;
	while(1) {
		sleep(3);
		ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
		if(ret == -1){
			dbg("send daemon message error\n");
			system("reboot &");
			exit(0);
		}
	}
}

void report_status_normal()
{
	usleep(500*1000);
	ioctl_usbdet_led(0);
	usleep(500*1000);
	ioctl_usbdet_led(1);
	return;
}

int do_update()
{
	struct stat st;
	int ret = -1;

	if (stat("/sdcard/aa", &st) == 0) {
		ret = 0;
		system("rm /data/aa -rf && cp /sdcard/aa /data/aa -rf");
		system("rm /sdcard/aa");
	}
	/*kernal*/
	if (stat("/sdcard/imx23_linux.sb", &st) == 0) {
		ret = 0;
		system("flashcp /sdcard/imx23_linux.sb /dev/mtd0");
		system("rm /sdcard/imx23_linux.sb");	
	}
	/*system*/
	if (stat("/sdcard/rootfs.squashfs", &st) == 0) {
		ret = 0;
		system("flashcp /sdcard/rootfs.squashfs /dev/mtd1");
		system("rm /sdcard/rootfs.squashfs");
	}
	if (stat("/sdcard/setup.dsk", &st) == 0) {
		ret = 0;
		system("flashcp /sdcard/setup.dsk /dev/mtd3");
		system("rm /sdcard/setup.dsk");
	}
	return ret;
}

int usb_state_monitor()
{
	while(1){
		if(is_do_update())
			return 0;
		if( ioctl_usbdet_read()){
			if(is_do_update())
				return 0;
			system("reboot &");
			exit(0);
		}
		sleep(1);
	}
}

static int set_fl(int fd, int flags)
{

	int             val;

	val = fcntl(fd, F_GETFL, 0);
	if(val < 0){
		printf("fcntl get error");
		return -1;
	}

	val |= flags;
	if(fcntl(fd, F_SETFL, val) < 0){
		printf("fcntl set error");
		return -1;
	}

	return 0;

}

static int extract_value(struct configstruct *allconfig,int elements, char *name,int is_string,void *dst)
{
	int i;
	char *strp;
	int * intp;
	for(i = 0; i < elements;i++){	
		if(strncmp(allconfig[i].name,name,strlen(name))==0){
			if(is_string){
				strp = (char *)dst;
				memcpy(strp,allconfig[i].value,64);
			}else{
				intp = (int *)dst;
				*intp = atoi(allconfig[i].value);
			}
			return 0;
		}
	}
	return -1;
}

static int set_value(struct configstruct *allconfig,int elements, char *name,int is_string,void *value){
	int i;
	for(i = 0; i < elements;i++){	
		if(strncmp(allconfig[i].name,name,strlen(name))==0){
			if(is_string){
				memcpy(allconfig[i].value,value,64);
			}else{
				memset(allconfig[i].value,0,64);
				sprintf(allconfig[i].value,"%d",*(int*)value);
			}
			return 0;
		}
	}
	return -1;
}

static int write_config_value(struct configstruct *allconfig,int elements)
{
	FILE*fp;
	int i;
	int len;
	char buf[512];
	fp = fopen(RECORD_PAR_FILE, "w");
	if(!fp){
		printf("write configure file error , something wrong\n");
		return -1;
	}
	for(i=0;i<elements;i++){
		memset(buf,0,512);
		sprintf(buf,"%s=%s\n",allconfig[i].name,allconfig[i].value);
		len = strlen(buf);
		if(len!=fwrite(buf,1,len,fp)){
			printf("write config file error\n");
		}
	}
	fflush(fp);
	fclose(fp);
	return 0;
}
static inline void vs_msg_update_system_time()
{
	int ret;
	vs_ctl_message msg;
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_UPDATE_SYSTEM_TIME;
	msg.msg[1] = 0;
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
		system("reboot &");
		exit(0);
	}
	dbg("snd update system time ok\n");
}
int set_system_time(char * time)
{
	char buf[32];
	struct tm _tm;
	struct timeval tv;
	time_t timep;
	memset(buf,0,sizeof(buf));
	memcpy(buf , time , 4);
	_tm.tm_year = atoi(buf);
	if(_tm.tm_year ==0)
		return -1;
	_tm.tm_year-=1900;
	memset(buf,0,sizeof(buf));
	memcpy(buf , time+4 , 2);
	_tm.tm_mon =atoi(buf);
	if(_tm.tm_mon==0)
		return -1;
	_tm.tm_mon --;
	memset(buf , 0 ,sizeof(buf));
	memcpy(buf ,time+6,2);
	_tm.tm_mday = atoi(buf);
	if(_tm.tm_mday == 0)
		return -1;
	memset(buf , 0 ,sizeof(buf));
	memcpy(buf ,time+8,2);
	if(*buf=='0'&&*(buf+1)=='0')
		_tm.tm_hour = 0;
	else{
		_tm.tm_hour = atoi(buf);
		if(_tm.tm_hour==0)
			return -1;
	}
	memset(buf , 0 ,sizeof(buf));
	memcpy(buf ,time+10,2);
	if(*buf=='0'&&*(buf+1)=='0')
		_tm.tm_min = 0;
	else{
		_tm.tm_min = atoi(buf);
		if(_tm.tm_min==0)
			return -1;
	}
	memset(buf , 0 ,sizeof(buf));
	memcpy(buf ,time+12,2);
	if(*buf=='0'&&*(buf+1)=='0')
		_tm.tm_sec = 0;
	else{
		_tm.tm_sec = atoi(buf);
		if(_tm.tm_sec==0)
			return -1;
	}
	timep = mktime(&_tm);
	tv.tv_sec = timep;
	tv.tv_usec = 0;
	//vs_msg_update_system_time();
	//sleep(5);
	//tv.tv_sec+=3;
	if(settimeofday(&tv , NULL)<0){
		printf("settime fail\n");
		return -1;
	}
	system("hwclock -w");
	printf("set time sucess\n");
	printf("time==%s\n",time);
	return 0;
}
int set_raw_config_value(char * buffer)
{
	struct configstruct *conf_p;
	FILE*fp;
	char *s;
	char *d;
	char *sp;
	char *dp;
	char buf[512];
	char name[64];
	char value[64];
	int lines;
	conf_p = (struct configstruct *)calloc(100,sizeof(struct configstruct));
	if(!conf_p){
		printf("unable to calloc 100 configstruct \n");
		return -1;
	}
	fp =fopen(RECORD_PAR_FILE, "r");
	if(!fp){
		printf("open video.cfg error\n");
		free(conf_p);
		return -1;
	}
	memset(conf_p,0,100*sizeof(struct configstruct));
	lines = 0;
	memset(buf,0,512);
	while(fgets(buf,512,fp)!=NULL){
		sp=buf;
		dp=conf_p[lines].name;
		while(*sp==' '||*sp=='\t')sp++;
		while(*sp&&*sp!='='){
			*dp=*sp;
			dp++;
			sp++;
		}
		sp++;
		while(*sp&&(*sp==' '||*sp=='\t'))sp++;
		dp=conf_p[lines].value;
		while(*sp&&*sp!='\n'){
			*dp=*sp;
			dp++;
			sp++;
		}
		//printf("name==%s , value=%s\n",conf_p[lines].name,conf_p[lines].value);
		lines++;
		memset(buf,0,512);
	}
	fclose(fp);
	s=buffer;
	while(*s){
		memset(name,0,sizeof(name));
		memset(value,0,sizeof(value));
		while(*s==' '||*s=='\t')s++;
		d = name;
		while(*s&&*s!='='){
			if(*s=='\n')
				goto out;
			*d =*s;
			d++;
			s++;
		}
		if(!*s)break;
		s++;
		while(*s&&(*s==' '||*s=='\t'))s++;
		d=value;
		while(*s&&*s!='\n'){
			*d =*s;
			d++;
			s++;
		}
		if(*s=='\n')
			s++;
		if(strncmp(name,"system_time",strlen("system_time"))==0)
			/*set_system_time(value)*/;
		/*
		else if(strncmp(name, CFG_PSWD ,strlen(CFG_PSWD))==0){
			printf("#################we found pswd now set it###############\n");
			printf("############pswd=%s################\n",value);
			if(strlen(value)!=0){
				FILE*pswdfp = fopen(PASSWORD_FILE , "w");
				if(pswdfp){
					fwrite(value,1,strlen(value),pswdfp);
					fclose(pswdfp);
					printf("##################set pswd ok##############\n");
				}else{
					printf("##################set pswd fail#################\n");
				}
			}
		}*/
		else
			set_value(conf_p,lines, name, 1,  value);
	}
out:
	write_config_value(conf_p,lines);
	free(conf_p);
	printf("write config file sucess\n");
	return 0;
}
int get_gateway(char * device ,char *gateway){
	char buf[512];
	FILE * routefp;
	memset(buf,0,512);
	sprintf(buf,"route | grep default | grep %s > /tmp/route",device);
	system(buf);
	routefp = fopen("/tmp/route","r");
	if(!routefp){
		printf("get eth default gateway error\n");
		return -1;
	}
	memset(buf,0,512);
	if(fgets(buf,512,routefp)!=NULL){
		char *src = buf;
		char *dst;
		while(*src==' ' || *src=='\t')src++;
		while(*src!=' '&&*src!='\t')src++;
		while(*src==' ' || *src=='\t')src++;
		dst = gateway;
		while(*src!=' '&&*src!='\t'){
			*dst = *src;
			dst++;
			src++;
		}
	}else{
		fclose(routefp);
		system("rm /tmp/route");
		return -1;
	}
	fclose(routefp);
	printf("%s gateway==%s\n",device,gateway);
	system("rm /tmp/route");
	return 0;
}

int get_ip(char * device , char *ip , char *mask)
{
     struct ifconf ifconf;   
     char buf[1024];        
    ifconf.ifc_len = 1024;  
    ifconf.ifc_buf = buf;   

    struct ifreq *ifreq;    
    ifreq = ifconf.ifc_req;  
     int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(sockfd, SIOCGIFCONF, &ifconf); 
    while(ifconf.ifc_len >= sizeof(struct ifreq))
    {
        struct ifreq brdinfo;
	if(strncmp(device,ifreq->ifr_name,strlen(device))==0){
	        printf("device = %s\n", ifreq->ifr_name);  
	        strcpy(ip,inet_ntoa((((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr)));
	        printf("%s  ip = %s\n",device, ip);
	        strcpy(brdinfo.ifr_name, ifreq->ifr_name);
	        ioctl(sockfd, SIOCGIFNETMASK, &brdinfo);     
		strcpy(mask,inet_ntoa(((struct sockaddr_in*)&(brdinfo.ifr_addr))->sin_addr));
	        printf("%s net mask   = %s\n",device, mask);
		close(sockfd);
		return 0;
    	}
        ifreq ++;
        ifconf.ifc_len -= sizeof(struct ifreq);
    }
	close(sockfd);
	return -1;
}

int get_dns(char  *dns1 , char *dns2)
{
	FILE *dnsfp;
	char buf[512];
	char *dns[2];
	char *p;
	int i ;
	dns1[0]=0;
	dns2[0]=0;
	dns[0] = dns1;
	dns[1] = dns2;
	dnsfp = fopen("/tmp/resolv.conf","r");
	if(!dnsfp){
		printf("cannot open resolv.conf \n");
		return -1;
	}
	memset(buf,0,512);
	i = 0;
	while(fgets(buf,512,dnsfp)!=NULL&&i<2){
		p= buf;
		while(*p==' ' || *p=='\t')p++;
		if(strncmp(p,"nameserver",strlen("nameserver"))==0){
			p+=strlen("nameserver");
			while(*p==' ' || *p=='\t')p++;
			while(*p!=' '&&*p!='\t'&&*p!='\n'&&*p!=0){
				*dns[i]=*p;
				dns[i]++;
				p++;
			}
			i++;
		}
		memset(buf,0,512);
	}
	fclose(dnsfp);
	return 0;
}
int set_dns(char  *dns1 , char *dns2)
{
	FILE *dnsfp;
	char buf[512];
	dnsfp = fopen("/tmp/resolv.conf","w");
	if(!dnsfp){
		printf("cannot open resolve.conf for write\n");
		return -1;
	}
	memset(buf,0,512);
	sprintf(buf,"nameserver  %s\n",dns1);
	fwrite(buf,1,strlen(buf),dnsfp);
	memset(buf,0,512);
	sprintf(buf,"nameserver  %s\n",dns2);
	fwrite(buf,1,strlen(buf),dnsfp);
	fflush(dnsfp);
	fclose(dnsfp);
	return 0;
}
//"ssid=%s\tsignal_level=%s\tproto=%s\tkey_mgmt=%s\tpairwise=%s\tgroup=%s\n"

static int is_chareter(char a)
{
	if((a>='A'&&a<='Z')||(a>='a'&&a<='z'))
		return 1;
	else
		return 0;
}
int __strncmp(char *s1 , char *s2,int n)
{
	int i;
	for(i = 0; i<n;i++){
		if(is_chareter(*s1)&&is_chareter(*s2)){
			if(*s1-*s2!=0&&abs(*s1-*s2)!=32)
				return (*s1-*s2);
		}else{
			if(*s1-*s2!=0)
				return (*s1-*s2);
		}
		s1++;
		s2++;
	}
	return 0;
}

char * get_parse_scan_result( int *numssid , char * wish_ssid);
int config_wifi(struct configstruct *conf_p, int lines)
{
	int i;
	char buf[256];
	char *argv[4];
	char network_id[32];
	int numssid;
	char *p;
	char * parse_result = NULL;
	struct stat st;
	char *ssid,*signal_level,*proto,*key_mgmt,*pairwise , *group;
	for(i=0;i<4;i++){
		argv[i] = (char *)malloc(256);
		if(!argv[i]){
			printf("cannot malloc buff to configure wifi something wrong\n");
			exit(0);
		}
		memset(argv[i],0,256);
	}
	if(stat("/data/wpa.conf",&st)<0)
		system("cp /etc/wpa.conf  /data/wpa.conf");
	system("mkdir /tmp/wpa_supplicant");
	system("killall wpa_supplicant");
	sleep(1);
	system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
	sleep(5);

	memset(buf,0,256);
	extract_value(conf_p, lines, CFG_WLAN_SSID, 1, buf);
	printf("inet_wlan_ssid = %s\n",buf);
	if(!buf[0]){
		printf("error ssid\n");
		scanresult = NULL;
		goto error;
	}
	
	parse_result = get_parse_scan_result(& numssid, buf);
	if(!parse_result){
		scanresult = NULL;
		goto error;
	}
	
	p = parse_result;
	while(*p){
		ssid = p;
		signal_level = ssid;
		while(*signal_level!='\t')signal_level++;
		*signal_level = 0;
		signal_level++;
		proto = signal_level;
		while(*proto!='\t')proto++;
		*proto = 0;
		proto++;
		key_mgmt = proto;
		while(*key_mgmt!='\t')key_mgmt++;
		*key_mgmt=0;
		key_mgmt++;
		pairwise = key_mgmt;
		while(*pairwise!='\t')pairwise++;
		*pairwise = 0;
		pairwise++;
		group = pairwise;
		while(*group!='\t')group++;
		*group = 0;
		group++;
		p = group;
		while(*p!='\n')p++;
		*p=0;
		p++;
		while(*ssid!='=')ssid++;
		ssid++;
		while(*signal_level!='=')signal_level++;
		signal_level++;
		while(*proto!='=')proto++;
		proto++;
		while(*key_mgmt!='=')key_mgmt++;
		key_mgmt++;
		while(*pairwise!='=')pairwise++;
		pairwise++;
		while(*group!='=')group++;
		group++;
		if(__strncmp(buf,ssid,strlen(buf))==0){
			break;
		}
	}
	if(!*ssid){
		scanresult  = NULL;
		goto error;
	}
	if(__strncmp(buf,ssid,strlen(ssid))!=0){
		printf("*********error cannot scan the specify ssid***********\n");
		scanresult = NULL;
		goto error;
	}

	scanresult = (char *)malloc(2048);
	if(!scanresult){
		printf("malloc buff for scanresult error\n");
		goto error;
	}
	
	sprintf(argv[0],"remove_network");
	sprintf(argv[1],"0");
	mywpa_cli(2,  argv);
	
	sprintf(argv[0],"ap_scan");
	sprintf(argv[1],"1");
	mywpa_cli(2,  argv);
	
	sprintf(argv[0],"add_network");
	mywpa_cli(1,argv);
	if(strncmp(scanresult,"FAIL",strlen("FAIL"))==0){
		goto error;
	}
	memset(network_id , 0 ,sizeof(network_id));
	memcpy(network_id , scanresult , result_len-1);
	
	sprintf(argv[0],"set_network");
	sprintf(argv[1],"%s",network_id);
	sprintf(argv[2],"ssid");
	sprintf(argv[3],"\"%s\"",ssid);
	printf("try ssid\n");
	mywpa_cli(4,  argv );
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}
	/*
	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_mode", 1, buf);
	printf("inet_wlan_mode = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"mode");
		sprintf(argv[3],"%s",buf);
		printf("try mode\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
	}
	*/
	//memset(buf,0,256);
	//extract_value(conf_p, lines, "inet_wlan_key_mgmt", 1, buf);
	printf("inet_wlan_key_mgmt = %s\n",key_mgmt);
	sprintf(argv[0],"set_network");
	sprintf(argv[1],"%s",network_id);
	sprintf(argv[2],"key_mgmt");
	sprintf(argv[3],"%s",key_mgmt);
	printf("try key_mgmt\n");
	mywpa_cli(4,  argv );
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}

	
	if(strncmp(key_mgmt, "WPA-PSK",7)==0){
		//memset(buf,0,256);
		//extract_value(conf_p, lines, "inet_wlan_proto", 1, buf);
		printf("inet_wlan_proto = %s\n",proto);
		if(*proto){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"proto");
			sprintf(argv[3],"%s",proto);
			printf("try proto\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

		//memset(buf,0,256);
		//extract_value(conf_p, lines, "inet_wlan_group", 1, buf);
		printf("inet_wlan_group = %s\n",group);
		if(*group){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"group");
			sprintf(argv[3],"%s",group);
			printf("try group\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

	//	memset(buf,0,256);
		//extract_value(conf_p, lines, "inet_wlan_pairwise", 1, buf);
		printf("inet_wlan_pairwise = %s\n",pairwise);
		if(*pairwise){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"pairwise");
			sprintf(argv[3],"%s",pairwise);
			printf("try pairwise\n");
			mywpa_cli(4,  argv );
			/*
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
			*/
		}

		memset(buf,0,256);
		extract_value(conf_p, lines, CFG_WLAN_KEY, 1, buf);
		printf("inet_wlan_psk = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"psk");
			sprintf(argv[3],"\"%s\"",buf);
			printf("try psk\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}
	}else{
		memset(buf,0,256);
		extract_value(conf_p, lines, CFG_WLAN_KEY, 1, buf);
		printf("inet_wlan_wep_key0 = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"wep_key0");
			sprintf(argv[3],"%s",buf);
			printf("try wep_key0\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

		/*
		memset(buf,0,256);
		extract_value(conf_p, lines, "inet_wlan_wep_key1", 1, buf);
		printf("inet_wlan_wep_key1 = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"wep_key1");
			sprintf(argv[3],"%s",buf);
			printf("try wep_key1\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

		memset(buf,0,256);
		extract_value(conf_p, lines, "inet_wlan_wep_key2", 1, buf);
		printf("inet_wlan_wep_key2 = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"wep_key2");
			sprintf(argv[3],"%s",buf);
			printf("try wep_key2\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}
		*/
		if(buf[0]){
			memset(buf,0,256);
			extract_value(conf_p, lines, "inet_wlan_wep_tx_keyindx", 1, buf);
			printf("inet_wlan_wep_tx_keyindx = %s\n",buf);
			if(!buf[0])
				sprintf(buf,"0");
			if(buf[0]){
				sprintf(argv[0],"set_network");
				//sprintf(argv[1],"0");
				sprintf(argv[1],"%s",network_id);
				sprintf(argv[2],"wep_tx_keyidx");
				sprintf(argv[3],"%s",buf);
				printf("try wep_tx_keyidx\n");
				mywpa_cli(4,  argv );
				if(strncmp(scanresult,"OK",strlen("OK"))!=0){
					goto error;
				}
			}
		}

		/*
		memset(buf,0,256);
		extract_value(conf_p, lines, "inet_wlan_auth_alg", 1, buf);
		printf("inet_wlan_auth_alg = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"auth_alg");
			sprintf(argv[3],"%s",buf);
			printf("try auth_alg\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}
		*/
	}
	
	
	sprintf(argv[0],"select_network");
	//sprintf(argv[1],"0");
	sprintf(argv[1],"%s",network_id);
	printf("try select_network\n");
	mywpa_cli(2,argv);
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}
	free(scanresult);
	free(parse_result);
	for(i=0;i<4;i++)
		  free(argv[i]);
	return 0;
error:
	if(scanresult)
		free(scanresult);
	if(parse_result)
		free(parse_result);
	for(i=0;i<4;i++)
			free(argv[i]);
	return -1;
}

int get_netlink_status(const char *if_name)

{

    int skfd,err;

    struct ifreq ifr;

    struct ethtool_value edata;

    edata.cmd = ETHTOOL_GLINK;

    edata.data = 0;

    memset(&ifr, 0, sizeof(ifr));

    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name) - 1);

    ifr.ifr_data = (char *) &edata;

    if ((skfd=socket(AF_INET,SOCK_DGRAM,0))==0)
  {
    printf("socket error:%s\n",strerror(skfd));
        return -1;
  }

    if((err=ioctl( skfd, SIOCETHTOOL, &ifr )) == -1)

    {
    printf("ioctl error:%s\n",strerror(err));
        close(skfd);
        return -1;

    }

    close(skfd);
    return edata.data;

}

 char * scan_wifi(int *len)
 { 	
 	struct stat st;
	int i;
	int j;
	char *argv[4];
	int tryscan;
 	scanresult = (char *)malloc(4096);
	if(!scanresult){
		printf("cannot malloc buf for scanresult\n");
		return NULL;
	}
	system("ps -efww |grep wpa_supplicant | grep -v grep > /tmp/twpa");
	stat("/tmp/twpa", &st);
	if(st.st_size <=0){
		printf("start wpa_supplicant\n");
		if(stat("/data/wpa.conf",&st)<0)
			system("cp /etc/wpa.conf  /data/wpa.conf");
		system("mkdir /tmp/wpa_supplicant");
		system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
		sleep(3);
	}
	system("rm /tmp/twpa");
	for(i=0;i<4;i++){
		argv[i]=malloc(256);
		if(!argv[i]){
			printf("can not malloc buf for scan wifi\n");
			for(j=0;j<i;j++)
				free(argv[j]);
			free(scanresult);
			return NULL;
		}
	}
	
	sprintf(argv[0],"ap_scan");
	sprintf(argv[1],"1");
	mywpa_cli(2,  argv);
	sleep(1);
	
	printf("###########begin scan###########\n");
	tryscan = 5;
	while(tryscan){
		sprintf(argv[0],"scan");
		mywpa_cli(1,argv);
		sleep(6);
		sprintf(argv[0],"scan_results");
		mywpa_cli(1,argv);
		if(result_len>48)
			break;
		tryscan --;
	}
	for(i= 0; i < 4; i++)
		free(argv[i]);
	printf("############end###############\n");
	*len = result_len;
	if(tryscan>0)
		return scanresult;
	free(scanresult);
	return NULL;
 }

#define RESERVE_SCAN_FILE		 "/data/wifi_last_scan.cfg"
char * get_parse_scan_result( int *numssid , char *wish_ssid)
 {
 	char *rawbuf;
	char *buf;
	int len;
	char *bssid;
	char *frequency;
	char *signal_leval;
	char *flags;
	char *ssid;
	char proto[64];
	char key_mgmt[64];
	char pairwise[64];
	char group[64];
	char *p;
	char *d;
	int i = 0;
	FILE *last_scan_fp;
	int j;
	*numssid = 0;
	buf = (char *)malloc(4096);
	if(!buf){
		printf("cannot malloc buf int parse scan result\n");
		return NULL;
	}
	memset(buf , 0 , 4096);
	if(wish_ssid!=NULL){
		last_scan_fp = fopen(RESERVE_SCAN_FILE,"r");
		if(last_scan_fp){
			while(fgets(buf,4096,last_scan_fp)!=NULL){
				p = buf;
				while(*p!='=')p++;
				p++;
				if(__strncmp(p, wish_ssid, strlen(wish_ssid))==0){
					fclose(last_scan_fp);
					*numssid = 1;
					return buf;
				}
				memset(buf,0,strlen(buf));
			}
			fclose(last_scan_fp);
		}
	}
	rawbuf = scan_wifi(&len);
	if(!rawbuf){
		printf("scan fail\n");
		free(buf);
		return NULL;
	}
	p = rawbuf +48;
	i+=48;
	d = buf;
	while(i<len){
		bssid = p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p =0;
		p++;
		i++;
		frequency = p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		signal_leval = p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		flags=p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		ssid=p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		(*numssid)++;
		memset(proto , 0, sizeof(proto));
		memset(key_mgmt , 0 , sizeof(key_mgmt));
		memset(pairwise , 0 ,sizeof(pairwise));
		memset(group , 0 ,sizeof(group));
		if(!*flags){
		}else{
			if(*flags=='[')flags++;
			if(*flags==']'){
			}else{
				if(strncmp(flags,"WEP",strlen("WEP"))==0){
					flags+=strlen("WEP");
					sprintf(key_mgmt,"NONE");
				}
				if(strncmp(flags,"WPA-PSK",strlen("WPA-PSK"))==0){
					flags+=strlen("WPA-PSK");
					sprintf(key_mgmt,"WPA-PSK");
					sprintf(proto,"WPA");
					if(*flags=='-'){
						flags++;
						for(j=0;*flags!=']';j++){
							if(*flags=='+'){
								group[j]=' ';
								flags++;
							}else if(*flags=='-'){
								while(*flags!=']'&&*flags!='+')flags++;
								j--;
							}else{
								group[j]=*flags;
								flags++;
							}
						}
						memcpy(pairwise , group,sizeof(pairwise));
					}
				}
				if(strncmp(flags,"WPA2-PSK",strlen("WPA2-PSK"))==0){
					flags+=strlen("WPA2-PSK");
					sprintf(key_mgmt,"WPA-PSK");
					sprintf(proto,"WPA2");
					if(*flags=='-'){
						flags++;
						for(j=0;*flags!=']';j++){
							if(*flags=='+'){
								group[j]=' ';
								flags++;
							}else if(*flags=='-'){
								while(*flags!=']'&&*flags!='+')flags++;
								j--;
							}else{
								group[j]=*flags;
								flags++;
							}
						}
						memcpy(pairwise , group,sizeof(pairwise));
					}
				}
			}
		}
		sprintf(d,"ssid=%s\tsignal_level=%s\tproto=%s\tkey_mgmt=%s\tpairwise=%s\tgroup=%s\n",ssid,signal_leval,proto,key_mgmt,pairwise,group);
		//printf("%s",d);
		d+=strlen(d);
	}
	last_scan_fp = fopen(RESERVE_SCAN_FILE,"w");
	if(!last_scan_fp){
		printf("save the scan result error :cannot open file for write\n");
	}else{
		fwrite(buf,1,strlen(buf),last_scan_fp);
		fclose(last_scan_fp);
	}
	free(rawbuf);
	return buf;
 }

int get_cam_id(unsigned int *id)
{
	FILE*fp;
	int id0,id1,id2;
	char buf[64];
	fp = fopen("/sys/uid/otp/id","r");
	if(!fp){
		return -1;
	}
	memset(buf,0,sizeof(buf));
	if(fgets(buf,64,fp)==NULL){
	  fclose(fp);
	  return -1;
	}
	if(sscanf(buf,"%x %x %x %x",&id0,&id1,&id2,id)!=4){
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}

#define HID_READ_VIDEO_CFG 		   1
#define HID_WRITE_VIDEO_CFG	 	   2
#define HID_READ_SEARCH_WIFI		   3
#define HID_RESET_TO_DEFAULT		   4
#define HID_SET_PSWD				   5

#define HID_FAILE					   4

#define HIDCMD_SCAN_WIFI	0
#define HIDCMD_SET_NETWORK_MODE 1
#define HIDCMD_GET_CONFIG	2
#define HIDCMD_SET_NETWORK_ADDRESS	3
#define HIDCMD_GET_NETWORK_ADDRESS	4
int test_printf_getConfig();
int querryfs(char *fs , long *maxsize,long * freesize);
char * get_clean_video_cfg();
void sig_handle(int signo)
{
	exit(0);
}
void read_pswd()
{
	struct stat st;
	FILE*pswdfp;
	memset(threadcfg.pswd,0,128);
	if(stat(PASSWORD_FILE , &st)==0){
		pswdfp = fopen(PASSWORD_FILE,"r");
		fread(threadcfg.pswd,1,128,pswdfp);
		fclose(pswdfp);
		printf("pswd=%s\n",threadcfg.pswd);
	}else{
		printf("pswd not set\n");
	}
}

int creat_record_file(int argc, char **argv);
int prepare_record()
{
	char buf[512];
	char block_name[64];
	FILE *fp;
	char *s,*e;
	char *argv[3];
	int i;
	int need_format;
	struct stat st;
	memset(buf,0,512);
	system("cat /proc/mounts | grep /sdcard > /tmp/sdcard_mount");
	fp = fopen("/tmp/sdcard_mount","r");
	if(!fp){
		printf("open mount file fail\n");
		return -1;
	}
	if(fgets(buf,512,fp)==NULL){
		fclose(fp);
		printf("can't find mount information\n");
		system("rm /tmp/sdcard_mount");
		return -1;
	}
	fclose(fp);
	usleep(50000);
	system("rm /tmp/sdcard_mount");
	/*
	if(stat("/sdcard/ipcam_record",&st)==0){
		dbg("rm ipcam_record\n");
		system("rm -rf /sdcard/ipcam_record");
		system("sync");
	}
	*/
	s=buf;
	while(*s&&(*s==' '||*s=='\t'))s++;
	if(!*s){
		printf("have not info about sdcard in mount file\n");
		return -1;
	}
	e = s;
	while(*e&&*e!=' '&&*e!='\t')e++;
	if(*e==' '||*e=='\t')
		*e=0;
	printf("get block name %s\n",s);
	memset(block_name , 0 ,sizeof(block_name));
	if(strncmp(s,"/dev",4)!=0)
		sprintf(block_name , "/dev/%s",s);
	else
		sprintf(block_name , "%s",s);
	printf("now the block name is %s\n",block_name);
	need_format = stat("/sdcard/ipcam_record",&st);
	system("umount /sdcard");
	if(need_format == 0){
		memset(buf,0,sizeof(buf));
		sprintf(buf,"mkdosfs  %s",block_name);
		system(buf);
		sleep(1);
	}
	for(i = 0 ; i<3 ; i++){
		argv[i] = (char *)malloc(64);
		if(!argv[i]){
			dbg("cannot malloc buf \n");
			exit(0);
		}
	}
	for(i=0;i<3;i++)
		memset(argv[i],0,64);
	sprintf(argv[0],"fatty");
	sprintf(argv[1],"200");
	memcpy(argv[2],block_name,64);
	creat_record_file(3,  argv);
	memset(buf,0,sizeof(buf));
	sprintf(buf,"mount -t auto  %s  /sdcard/",block_name);
	system(buf);
	dbg("prepare record file ok\n");
	for(i=0;i<3;i++)
		free(argv[i]);
	//system("mount -t auto /dev/mmcblk0p1 /sdcard/");
	//sleep(10);
	return 0;
}

int update_config_file_carefully()
{
	FILE *fp;
	char buf[256];
	char *old_file_buf;
	char *p;
	int version_flag = 0;
	int brightness_flag = 0;
	int contrast_flag = 0;
	int pos;
	dbg("try update configure file carefully , it may be slow\n");
	fp = fopen(RECORD_PAR_FILE , "r");
	if(!fp){
		dbg("can not open old config file for write\n");
		exit(0);
	}
	old_file_buf = (char *)malloc(2048);
	if(!old_file_buf){
		dbg("can not malloc buf for old configure file\n");
		exit(0);
	}
	memset(buf,0,sizeof(buf));
	memset(old_file_buf , 0 ,2048);
	pos = 0;
	while(fgets(buf,256,fp)!=NULL){
		p = buf;
		while(*p==' '||*p=='\t')p++;
		if(!version_flag&&strncmp(p,CFG_VERSION,strlen(CFG_VERSION)) ==0){
			version_flag = 1;
		}else if(!contrast_flag&&strncmp(p,CFG_CONTRAST,strlen(CFG_CONTRAST))==0){
			contrast_flag = 1;
		}else if(!brightness_flag&&strncmp(p,CFG_BRIGHTNESS,strlen(CFG_BRIGHTNESS))==0){
			brightness_flag = 1;
		}else{
			memcpy(old_file_buf+pos,p,strlen(p));
			pos+=strlen(p);
		}
		memset(buf,0,sizeof(buf));
	}
	fclose(fp);
	usleep(100000);
	memset(buf,0,sizeof(buf));
	sprintf(buf,"cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
	system(buf);
	system("sync");
	usleep(100000);
	if( set_raw_config_value(old_file_buf)<0)
		exit(0);
	free(old_file_buf);
	return 0;
}
int main()
{
	int ret;
	int check_wlan0 =0;
	int check_eth0 = 0;
	int ping_eth = 0;
	int ping_wlan = 0;
	int count;
	//int tryscan;
	pthread_t tid;
	FILE*fd;
	char buf[512];
	struct configstruct *conf_p;
	//int i;
	char *ip=NULL;
	char *mask=NULL;
	
	//sleep(1);
	//signal(SIGINT , sig_handle);

	if( open_usbdet() != 0 ){
		printf("open usb detect error\n");
		return -1;
	}

	if( do_update() == 0 ){
		system("reboot &");
		exit (0);
	}
	if( ioctl_usbdet_read()){
		int hid_fd;
		//FILE *config_fp;
		char hid_r_cmd[2];
		char hid_w_cmd[2];
		unsigned short  data_len;
		char *p;
		char *s;
		int i;
		int ret;
		long sd_maxsize;
		long sd_freesize;
		int cmd;
		//int size;
		char *hid_buf;
		int numssid;
		
		system("switch gadget && sleep 2");

		/*check the configure file if it is the newest one*/
		fd = fopen(RECORD_PAR_FILE, "r");
		if(!fd){
			memset(buf,0,512);
			sprintf(buf , "cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
			system(buf);
		}else{
			p = NULL;
			while(fgets(buf,512,fd)!=NULL){
				p=buf;
				while(*p==' '||*p=='\t')p++;
				if(strncmp(p,CFG_VERSION,strlen(CFG_VERSION))==0){
					break;
				}
			}
			fclose(fd);
			//usleep(50000);
			if(p&&strncmp(p,CFG_VERSION,strlen(CFG_VERSION))==0){
				while(*p&&*p!='=')p++;
				if(*p=='=')p++;
				while(*p&&(*p==' '||*p=='\t'))p++;
				if(!*p||strncmp(p,CURR_VIDEO_CFG_VERSION,strlen(CURR_VIDEO_CFG_VERSION))!=0){
					update_config_file_carefully();
				}
			}else{
				memset(buf,0,512);
				sprintf(buf , "cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
				system(buf);
			}
		}

		
		if((hid_fd = open("/dev/hidg0", O_RDWR)) != -1 && set_fl( hid_fd, O_NONBLOCK ) != -1 ){
			while( 1 ){
				do{
					if(!( ioctl_usbdet_read())){
						system("reboot &");
						exit(0);
					}
					ret = read(hid_fd, hid_r_cmd, 2);
				}while(ret!=2);
				cmd=(int)hid_r_cmd[1];
				switch(cmd)
				{
					case HID_READ_VIDEO_CFG:
						printf("HID_READ_VIDEO_CFG\n");
						//sleep(60);
						if(get_cam_id(&threadcfg.cam_id)<0){
							printf("************************************************\n");
							printf("*            get camera id error,something wrong               *\n");
							printf("************************************************\n");
							goto hid_fail;
						}
						hid_buf = (char *)malloc(4096);
						if(!hid_buf)
							goto hid_fail;
						memset(hid_buf , 0,4096);
						p=hid_buf;
						data_len = 0;
						sprintf(p,APP_VERSION);
						data_len +=strlen(p);
						p+=strlen(p);
						sprintf(p , "cam_id=%x\n",threadcfg.cam_id);
						data_len +=strlen(p);
						p+=strlen(p);
						s = get_clean_video_cfg();
						if(!s){
							free(hid_buf);
							goto hid_fail;
						}
						memcpy(p,s,strlen(s));
						data_len+=strlen(s);
						p+=strlen(s);
						free(s);
						querryfs("/sdcard", &sd_maxsize, &sd_freesize);
						sprintf(p,"tfcard_maxsize=%ld\n",sd_maxsize);
						data_len +=strlen(p);
						p+=strlen(p);
						sprintf(p,"tfcard_freesize=%ld\n",sd_freesize);
						data_len +=strlen(p);
						if(data_len%2)
							data_len++;
						ret = write(hid_fd, (char *)&data_len , 2);
						p = (char *)&data_len;
						printf("data_len==%d  %2x %2x\n",(int)data_len,*p ,*(p+1));
						printf("ret ==%d\n" , ret);
						printf("##########################\n");
						sleep(2);
						for(i = 0; i<data_len; i+=2)
						{
							do{
								usleep(20000);
								ret = write(hid_fd , hid_buf+i ,2);
								//printf("write ret==%d , i==%d\n",ret , i);
							}while(ret != 2);
						}
						free(hid_buf);
						break;
					case HID_READ_SEARCH_WIFI:
						printf("HID_READ_SEARCH_WIFI\n");
						hid_buf = get_parse_scan_result(& numssid, NULL);
						data_len = strlen(hid_buf);
						if(data_len%2)
							data_len++;
						printf("data_len==%d\n",(int)data_len);
						do{
							usleep(20000);
							ret = write(hid_fd, (char *)&data_len , 2);
						}while(ret != 2);
						printf("##########################\n");
						sleep(1);
						for(i = 0; i<data_len; i+=2)
						{
							do{
								usleep(20000);
								ret = write(hid_fd , hid_buf+i ,2);
							}while(ret != 2);
						}
						free(hid_buf);
						break;
					case HID_WRITE_VIDEO_CFG:
						printf("HID_WRITE_VIDEO_CFG\n");
						hid_buf = (char *)malloc(4096);
						if(!hid_buf)
							exit(0);
						memset(hid_buf , 0 ,4096);
						do{
							ret = read(hid_fd, hid_r_cmd, 2);
						}while(ret!=2);
						memcpy(&data_len , hid_r_cmd , 2);
						if(data_len%2)
							data_len++;
						printf("data_len==%d\n",(int)data_len);
						p = hid_buf;
						for(i=0;i<data_len;i+=2){
							do{
								usleep(10000);
								ret = read(hid_fd, hid_r_cmd, 2);
								if(ret == 1)
									printf("***********read one char****************\n");
							}while(ret!=2);
							*p = hid_r_cmd[0];
							p++;
							*p = hid_r_cmd[1];
							p++;
							printf("i==%d , %2x %2x \n",i , hid_r_cmd[0],hid_r_cmd[1]);
						}
						printf("####################GET VIDEO_CFG###################\n");
						printf("%s",hid_buf);
						printf("##################################################\n");
						set_raw_config_value(hid_buf);
						free(hid_buf);
						break;
					case HID_RESET_TO_DEFAULT:
						printf("HID_CFG_RESET_TO_DEFAULT\n");
						memset(buf,0,512);
						sprintf(buf,"rm %s",PASSWORD_FILE);
						system(buf);
						memset(buf , 0 ,512);
						sprintf(buf,"cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
						system(buf);
						break;
					case HID_SET_PSWD:
						printf("HID_SET_PSWD\n");
						do{
							ret = read(hid_fd, hid_r_cmd, 2);
						}while(ret!=2);
						memcpy(&data_len , hid_r_cmd , 2);
						memset(buf,0,512);
						sprintf(buf,PASSWORD_PART_ARG);
						p = buf;
						p += strlen(buf);
						printf("data_len=%d\n",(int)data_len);
						for(i=0;i<data_len;i+=2){
							do{
								usleep(10000);
								ret = read(hid_fd, hid_r_cmd, 2);
								if(ret == 1)
									printf("***********read one char****************\n");
							}while(ret!=2);
							*p = hid_r_cmd[0];
							p++;
							*p = hid_r_cmd[1];
							p++;
							printf("i==%d , %2x %2x \n",i , hid_r_cmd[0],hid_r_cmd[1]);
						}
						data_len +=strlen(PASSWORD_PART_ARG);
						fd = fopen(PASSWORD_FILE,"w");
						if(!fd){
							printf("open password file fail\n");
							goto hid_fail;
						}
						if(fwrite(buf,1,data_len,fd)!=data_len){
							fflush(fd);
							fclose(fd);
							usleep(50000);
							memset(buf,0,512);
							sprintf(buf,"rm %s",PASSWORD_FILE);
							system(buf);
							printf("write pswd fail\n");
							goto hid_fail;
						}
						printf("%s\n",buf);
						fclose(fd);
						break;
					default:
					hid_fail:
						hid_w_cmd[0] = 0;
						hid_w_cmd[1] = 0;
						write(hid_fd, hid_w_cmd , 2);
				}	
				/*
				printf("size==%d\n",size);
				data_len=size;
				i=0;
				ip=buf;
				while(data_len>0){
					ret = read(hid_fd, &hid_r_cmd, 2);
					if(ret!=2){
						continue;
					}
					buf[i]=hid_r_cmd[0];
					printf("%c",buf[i]);
					if(buf[i]=='\n'){
						data_len--;
						printf("\ndata_len==%d\n",data_len);
					}
					i++;
					buf[i]=hid_r_cmd[1];
					printf("%c",buf[i]);
					if(buf[i]=='\n'){
						data_len--;
						printf("\ndata_len==%d\n",data_len);
						
					}
					i++;
				}
				fwrite(buf,1,i,netconfig_fd);
				fflush(netconfig_fd);
				fclose(netconfig_fd);
				printf("\n####################ok################\n");
				*/
			}
		}
	}

	if (pthread_create(&tid, NULL, (void *) usb_state_monitor, NULL) < 0) {
		return -1;
	} 
	for( count = 0; count < 2; count++ ){
		report_status_normal();
	}

	system("switch host");
	sleep(1);
	system("switch host");
	sleep(3);
	
	memset(&threadcfg,0,sizeof(threadcfg));
	pthread_mutex_init(&global_ctx_lock,NULL);
	pthread_mutex_init(&(threadcfg.threadcfglock),NULL);
	init_g_sess_id_mask();
	//read the config data from video.cfg
read_config:	
	fd = fopen(RECORD_PAR_FILE, "r");
	if(fd==NULL){
		printf("open config file error,now try to open reserve config file\n");
		fd=fopen(MONITOR_PAR_FILE,"r");
		if(fd!=NULL){
			int size=0;
			FILE*fdx=fopen(RECORD_PAR_FILE,"w");
			printf("open reserve config file sucess\n");
			if(fdx!=NULL){
				char*buff=malloc(sizeof(char)*256);
				memset(buff,0,256);
				while((size=fread(buff,1,256,fd))!=0){
					if(size!=fwrite(buff,1,size,fdx)){
						printf("write config file error\n");
						fclose(fdx);
						sleep(1);
						system("rm -rf /data/video.cfg");
						sleep(1);
						fclose(fd);
						free(buff);
						return -1;
					}
				}
				free(buff);
				fflush(fdx);
				fclose(fdx);
			}else{
				printf("create new config file error\n");
				fclose(fd);
				return -1;
			}
			fseek(fd,0,0);
		}else{
			printf("open reserve config file error\n");
			return -1;
		}
	}
	if(fd==NULL){
		printf("open config file erro\n");
		exit(0);
	}else{
		int lines;
		printf("try to read config file\n");
		conf_p = (struct configstruct *)calloc(100,sizeof(struct configstruct));
		if(!conf_p){
			printf("unable to calloc 100 configstruct \n");
			fclose(fd);
			return -1;
		}
		memset(conf_p,0,100*sizeof(struct configstruct));
		lines = 0;
		memset(buf,0,512);
		while(fgets(buf,512,fd)!=NULL){
			char *sp=buf;
			char *dp=conf_p[lines].name;
			while(*sp==' '||*sp=='\t')sp++;
			while(*sp!='='){
				*dp=*sp;
				dp++;
				sp++;
			}
			sp++;
			while(*sp&&(*sp==' '||*sp=='\t'))sp++;
			dp=conf_p[lines].value;
			while(*sp&&*sp!='\n'){
				*dp=*sp;
				dp++;
				sp++;
			}
			printf("name==%s , value=%s\n",conf_p[lines].name,conf_p[lines].value);
			lines++;
			memset(buf,0,512);
		}

		fclose(fd);

		memset(buf , 0 ,512);
		extract_value(conf_p, lines, CFG_VERSION, 1, buf);
		if(!buf[0]||strncmp(buf,CURR_VIDEO_CFG_VERSION,strlen(CURR_VIDEO_CFG_VERSION))!=0){
			free(conf_p);
			dbg("the video.cfg is too old try to read copy the newer one\n");
			//memset(buf,0,512);
			//sprintf(buf,"rm %s",RECORD_PAR_FILE);
			//system(buf);
			//usleep(500000);
			update_config_file_carefully();
			goto read_config;
		}
		printf("cfg_v==%s\n",buf);
		if(get_cam_id(&threadcfg.cam_id)<0){
			printf("************************************************\n");
			printf("*            get camera id error,something wrong               *\n");
			printf("************************************************\n");
			return 0;
		}
		//set_value(conf_p, lines, "cam_id", 0, &threadcfg.cam_id);
		
		printf("cam_id = %x\n",threadcfg.cam_id);

		read_pswd();
		
		//extract_value(conf_p, lines, "name", 1, threadcfg.name);
		//printf("name = %s\n",threadcfg.name);

		sprintf(threadcfg.name,"ipcam");

		//extract_value(conf_p, lines, CFG_PSWD, 1, threadcfg.password);
		//printf("password = %s\n",threadcfg.password);

		//extract_value(conf_p, lines, "server_addr", 1, threadcfg.server_addr);
		//printf("server_addr = %s\n",threadcfg.server_addr);
		
		sprintf(threadcfg.server_addr , UDP_SERVER_ADDR);
		
		extract_value(conf_p, lines, CFG_MONITOR_MODE , 1, threadcfg.monitor_mode);
		printf("monitor_mode = %s\n",threadcfg.monitor_mode);

		if(!(int)threadcfg.monitor_mode[0]||(strncmp(threadcfg.monitor_mode , "normal",6)!=0&&strncmp(threadcfg.monitor_mode , "inteligent",10)!=0)){
			memset(threadcfg.monitor_mode,0,sizeof(threadcfg.monitor_mode));
			sprintf(threadcfg.monitor_mode,"inteligent");
			set_value(conf_p, lines, CFG_MONITOR_MODE , 1, threadcfg.monitor_mode);
		}

		extract_value(conf_p, lines, CFG_MONITOR_RATE, 0,(void *) &threadcfg.framerate);
		printf("framerate= %d\n",threadcfg.framerate);

		if(!threadcfg.framerate)
			threadcfg.framerate = 25;
		else if(threadcfg.framerate<1)
			threadcfg.framerate = 1;
		else if(threadcfg.framerate >25)
			threadcfg.framerate = 25;
		init_sleep_time();

		extract_value(conf_p, lines, CFG_COMPRESSION, 1, threadcfg.compression);
		printf("compression = %s\n",threadcfg.compression);

		extract_value(conf_p, lines, CFG_MONITOR_RESOLUTION, 1, threadcfg.resolution);
		printf("resolution = %s\n",threadcfg.resolution);

		if(!(int)threadcfg.resolution[0])
			sprintf(threadcfg.resolution,"vga");

		extract_value(conf_p, lines, CFG_GOP, 0, (void *)&threadcfg.gop);
		printf("gop = %d\n",threadcfg.gop);
	
		extract_value(conf_p, lines, CFG_ROTATION_ANGLE, 0, (void *)&threadcfg.rotation_angle);
		printf("rotation_angle = %d\n",threadcfg.rotation_angle);

		//extract_value(conf_p, lines, "output_ratio", 0, (void *)&threadcfg.output_ratio);
		//printf("output_ratio = %d\n",threadcfg.output_ratio);

		extract_value(conf_p, lines, CFG_BITRATE, 0, (void *)&threadcfg.bitrate);
		printf("bitrate = %d\n",threadcfg.bitrate);

		//extract_value(conf_p, lines, CFG_VOLUME, 0,(void *) &threadcfg.volume);
		threadcfg.volume = 100;
		printf("volume = %d\n",threadcfg.volume);
		
		extract_value(conf_p, lines, CFG_BRIGHTNESS, 0, (void *)&threadcfg.brightness);
		printf("brightness = %d\n",threadcfg.brightness);

		extract_value(conf_p, lines, CFG_CONTRAST, 0, (void *)&threadcfg.contrast);
		printf("contrast = %d\n",threadcfg.contrast);

		extract_value(conf_p, lines, CFG_SATURATION, 0,(void *) &threadcfg.saturation);
		printf("saturation = %d\n",threadcfg.saturation);

		extract_value(conf_p, lines, CFG_GAIN, 0, (void *)&threadcfg.gain);
		printf("gain = %d\n",threadcfg.gain);

		extract_value(conf_p, lines, CFG_RECORD_MODE, 1, threadcfg.record_mode);
		printf("record_mode = %s\n",threadcfg.record_mode);

		if(!(int)threadcfg.record_mode[0])
			sprintf(threadcfg.record_mode,"inteligent");

		extract_value(conf_p, lines, CFG_RECORD_RESOLUTION, 1, threadcfg.record_resolution);
		printf("record_resolution = %s\n",threadcfg.record_resolution);

		if(!(int)threadcfg.record_resolution[0])
			memcpy(threadcfg.record_resolution , threadcfg.resolution , 64);
		else
			memcpy(threadcfg.resolution ,threadcfg.record_resolution,64);
		
		set_value(conf_p, lines, CFG_RECORD_RESOLUTION, 1, &threadcfg.resolution);

		extract_value(conf_p, lines, CFG_RECORD_NORMAL_SPEED, 0, (void *)&threadcfg.record_normal_speed);
		printf("record_normal_speed= %d\n",threadcfg.record_normal_speed);

		if(!threadcfg.record_normal_speed)
			threadcfg.record_normal_speed = 25;
		else if(threadcfg.record_normal_speed<1)
			threadcfg.record_normal_speed = 1;
		else if(threadcfg.record_normal_speed >25)
			threadcfg.record_normal_speed = 25;

		extract_value(conf_p, lines, CFG_RECORD_NORMAL_DURATION, 0, (void *)&threadcfg.record_normal_duration);
		printf("record_normal_duration= %d\n",threadcfg.record_normal_duration);

		if(threadcfg.record_normal_duration<=0)
			threadcfg.record_normal_duration =1;
		else if(threadcfg.record_normal_duration>25)
			threadcfg.record_normal_duration = 25;



		extract_value(conf_p, lines, CFG_RECORD_SENSITIVITY, 0, (void *)&threadcfg.record_sensitivity);
		printf("record_sensitivity = %d\n",threadcfg.record_sensitivity);

		if(threadcfg.record_sensitivity<1||threadcfg.record_sensitivity>3)
			threadcfg.record_sensitivity = 1;

		extract_value(conf_p, lines, CFG_RECORD_SLOW_SPEED, 0, (void *)&threadcfg.record_slow_speed);
		printf("record_slow_speed = %d\n",threadcfg.record_slow_speed);

		if(threadcfg.record_slow_speed<1||threadcfg.record_slow_speed>25)
			threadcfg.record_slow_speed = 1;
		/*
		extract_value(conf_p, lines, "record_slow_resolution", 1, threadcfg.record_slow_resolution);
		printf("record_slow_resolution = %s\n",threadcfg.record_slow_resolution);
		
		if(!(int)threadcfg.record_slow_resolution[0])
			sprintf(threadcfg.record_slow_resolution,"qvga");
		*/
		extract_value(conf_p, lines, CFG_RECORD_FAST_SPEED, 0, (void *)&threadcfg.record_fast_speed);
		printf("record_fast_speed = %d\n",threadcfg.record_fast_speed);

		if(threadcfg.record_fast_speed<1||threadcfg.record_fast_speed>25)
			threadcfg.record_fast_speed = 25;
		/*
		extract_value(conf_p, lines, "record_fast_resolution", 1, threadcfg.record_fast_resolution);
		printf("record_fast_resolution = %s\n",threadcfg.record_fast_resolution);
		
		if(!(int)threadcfg.record_fast_resolution[0])
			sprintf(threadcfg.record_fast_resolution,"vga");
		*/
		extract_value(conf_p, lines, CFG_RECORD_FAST_DURATION, 0, (void *)&threadcfg.record_fast_duration);
		printf("record_fast_duration = %d\n",threadcfg.record_fast_duration);

		if(threadcfg.record_fast_duration<1)
			threadcfg.record_fast_duration = 3;

		extract_value(conf_p, lines, CFG_EMAIL_ALARM, 0, (void *)&threadcfg.email_alarm);
		printf("email_alarm = %d\n",threadcfg.email_alarm);

		if(threadcfg.email_alarm<0)
			threadcfg.email_alarm =0;

		extract_value(conf_p, lines, CFG_MAILBOX, 1, threadcfg.mailbox);
		printf("mailbox = %s\n",threadcfg.mailbox);

		extract_value(conf_p, lines, CFG_SOUND_DUPLEX, 0, (void *)&threadcfg.sound_duplex);
		printf("sound_duplex = %d\n",threadcfg.sound_duplex);

		if(threadcfg.sound_duplex<0)
			threadcfg.sound_duplex = 0;
		
		extract_value(conf_p, lines, CFG_NET_MODE, 1, threadcfg.inet_mode);
		printf("inet_mode = %s\n",threadcfg.inet_mode);

		if(!(int)threadcfg.inet_mode[0])
			sprintf(threadcfg.inet_mode,"inteligent");

		threadcfg.inet_udhcpc = 1;
		extract_value(conf_p, lines, CFG_UDHCPC, 0, (void *)&threadcfg.inet_udhcpc);
		printf("inet_udhcpc = %d\n",threadcfg.inet_udhcpc);

		extract_value(conf_p, lines, CFG_ETH_DEVICE, 1, inet_eth_device);
		printf("inet_eth_device = %s\n",inet_eth_device);

		if(!inet_eth_device[0])
			sprintf(inet_eth_device , "eth0");
		
		extract_value(conf_p, lines, CFG_WLAN_DEVICE, 1, inet_wlan_device);
		printf("inet_wlan_device = %s\n",inet_wlan_device);

		if(!inet_wlan_device[0])
			sprintf(inet_wlan_device, "wlan0");
		
		check_eth0 = 0;
		check_wlan0 = 0;
		if(strncmp(threadcfg.inet_mode,"eth_only",strlen("eth_only"))==0
			||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0){
			check_eth0 = 1;
			printf("------------configure eth----------------\n");
				ping_eth = get_netlink_status(inet_eth_device);
				if(ping_eth<0)
					ping_eth = 0;
				if(threadcfg.inet_udhcpc){
				eth_dhcp:
					memset(buf,0,512);
					sprintf(buf,"udhcpc -i %s &",inet_eth_device);
					system(buf);
					if(ping_eth>0)
						sleep(5);
					memset(inet_eth_gateway,0,sizeof(inet_eth_gateway));
					if(ping_eth>0)
						get_gateway(inet_eth_device, inet_eth_gateway);
					set_value(conf_p, lines, CFG_ETH_GATEWAY, 1, inet_eth_gateway);
					memset(buf,0,512);
					ip = buf;
					mask = buf+256;
					if(ping_eth>0)
						get_ip(inet_eth_device,ip,mask);
					set_value(conf_p, lines, CFG_ETH_IP, 1, ip);
					set_value(conf_p, lines, CFG_ETH_MASK, 1, mask);
					memset(buf , 0 , 512);
					ip = buf;
					mask = buf + 256;
					get_dns(ip,mask);
					set_value(conf_p, lines, CFG_ETH_DNS1, 1, ip);
					set_value(conf_p, lines, CFG_ETH_DNS2, 1, mask);
				}else{
					memset(buf,0,512);
					ip = buf;
					mask = buf +256;
					extract_value(conf_p, lines, CFG_ETH_DNS1, 1, ip);
					extract_value(conf_p, lines, CFG_ETH_DNS2, 1, mask);
					if(ip[0]||mask[0]){
						set_dns(ip, mask);
						memset(buf,0,512);
						sprintf(buf,"ifconfig %s down",inet_eth_device);
						system(buf);
						sleep(1);
						sprintf(buf,"ifconfig %s up",inet_eth_device);
						system(buf);
						sleep(1);
					}else{
						printf("it is not dhcp mode but the dns is not set , something wrong\n");
						goto eth_dhcp;
					}
					memset(buf,0,512);
					ip=buf+256;
					mask = buf + 256+32;
					extract_value(conf_p, lines, CFG_ETH_IP, 1, ip);
					printf("inet_eth_ip = %s\n",ip);
					extract_value(conf_p, lines, CFG_ETH_MASK, 1, mask);
					printf("inet_eth_mask = %s\n",mask);
					if(ip[0]&&mask[0]){
						sprintf(buf,"ifconfig %s %s netmask %s",inet_eth_device,ip, mask);
						printf("before set ip and mask buf==%s\n",buf);
						system(buf);
						sleep(1);
					}else{
						printf("it is not dhcp mode but the ip or mask not set , something wrong\n");
						goto eth_dhcp;
					}
					memset(buf,0,512);
					memset(inet_eth_gateway,0,sizeof(inet_eth_gateway));
					extract_value(conf_p, lines, CFG_ETH_GATEWAY, 1, inet_eth_gateway);
					printf("inet_eth_gateway = %s\n",inet_eth_gateway);
					if(!inet_eth_gateway[0]){
						printf("it is not dhcp mode but the gateway not set\n");
					}else{
						sprintf(buf,"route add default   gw  %s  %s",inet_eth_gateway,inet_eth_device);
						printf("before set gateway buf==%s\n",buf);
						system(buf);
						sleep(1);
					}
				}
		}
		if(strncmp(threadcfg.inet_mode,"wlan_only",strlen("wlan_only"))==0
			||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0){
			printf("------------configure wlan----------------\n");
			check_wlan0 = 1;
			if(config_wifi( conf_p, lines)<0){
				ping_wlan = 0;
				check_wlan0 = 0;
				printf("configure wifi error check your data\n");
			}else{
				ping_wlan = 1;
				if(threadcfg.inet_udhcpc){
				wlan_udhcpc:
					memset(buf,0,512);
					sprintf(buf,"udhcpc -i %s &",inet_wlan_device);
					system(buf);
					sleep(5);
					memset(inet_wlan_gateway,0,sizeof(inet_wlan_gateway));
					get_gateway(inet_wlan_device, inet_wlan_gateway);
					set_value(conf_p, lines, CFG_WLAN_GATEWAY, 1, inet_wlan_gateway);
					memset(buf,0,512);
					ip = buf;
					mask = buf+256;
					get_ip(inet_wlan_device,ip,mask);
					set_value(conf_p, lines, CFG_WLAN_IP, 1, ip);
					set_value(conf_p, lines, CFG_WLAN_MASK, 1, mask);
					memset(buf , 0 , 512);
					ip = buf;
					mask = buf + 256;
					get_dns(ip,mask);
					set_value(conf_p, lines, CFG_WLAN_DNS1, 1, ip);
					set_value(conf_p, lines, CFG_WLAN_DNS2, 1, mask);
				}else{
					memset(buf,0,512);
					ip = buf;
					mask = buf +256;
					extract_value(conf_p, lines, CFG_WLAN_DNS1, 1, ip);
					extract_value(conf_p, lines, CFG_WLAN_DNS2, 1, mask);
					if(ip[0]||mask[0]){
						set_dns(ip, mask);
						memset(buf,0,512);
						sprintf(buf,"ifconfig %s down",inet_wlan_device);
						system(buf);
						sleep(1);
						sprintf(buf,"ifconfig %s up",inet_wlan_device);
						system(buf);
						sleep(1);
					}else{
						printf("it is not dhcpc mode but the dns not set\n");
						goto wlan_udhcpc;
					}
					memset(buf,0,512);
					ip=buf+256;
					mask = buf + 256+32;
					extract_value(conf_p, lines, CFG_WLAN_IP, 1, ip);
					printf("inet_wlan_ip = %s\n",ip);
					extract_value(conf_p, lines, CFG_WLAN_MASK, 1, mask);
					printf("inet_wlan_mask = %s\n",mask);
					if(ip[0]&&mask[0]){
						sprintf(buf,"ifconfig %s %s netmask %s",inet_wlan_device,ip, mask);
						printf("before set ip and mask buf==%s\n",buf);
						system(buf);
						sleep(1);
					}else{
						printf("it is not udhcpc mode but the ip or mask not set\n");
						goto wlan_udhcpc;
					}
					memset(buf,0,512);
					memset(inet_wlan_gateway,0,sizeof(inet_wlan_gateway));
					extract_value(conf_p, lines, CFG_WLAN_GATEWAY, 1, inet_wlan_gateway);
					printf("inet_wlan_gateway = %s\n",inet_wlan_gateway);
					
					sprintf(buf,"route add default   gw  %s  %s",inet_wlan_gateway,inet_wlan_device);
					printf("before set gateway buf==%s\n",buf);
					system(buf);
					sleep(1);
					
				}
			}
		}
		write_config_value(conf_p, lines);
		memset(buf,0,512);
		free(conf_p);
	}

	if(built_net(check_wlan0,  check_eth0,ping_wlan , ping_eth)<0){
		printf("the network is error , check your network \n");
	}
	threadcfg.sdcard_exist = 1;
	prepare_record();
	ret = nand_open("/sdcard");
	if( ret != 0 ){
		threadcfg.sdcard_exist = 0;
		printf("open disk error\n");
	}

//	test_jpeg_file_write();
//	test_cli(system_sess);
//	test_video_monitor(system_sess);
//	uvc_test_main(1,NULL);

	if((init_and_start_sound())<0){
		printf("start sound error\n");
		return -1;
	}
	
	test_set_sound_card();

	test_video_record_and_monitor(NULL);

	printf("exit program\n");
	return 0;
}
