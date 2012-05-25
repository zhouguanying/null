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
extern int check_net_thread();
extern int mywpa_cli(int argc, char *argv[]);
int built_net(int check_wlan0,int check_eth0 , int ping_wlan0 , int ping_eth0);
extern char __gate_way[32];
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

	if( start_cli(system_sess) == NULL ){
		printf("cli start error\n");
		return -1;
	}
	printf("cli is running\n");
	msqid = msgget(DAEMON_MESSAGE_QUEUE_KEY,0666);
	if(msqid ==-1){
		dbg("unable to open daemon message");
		exit (0);
	}

	/*
	if (pthread_create(&tid, NULL, (void *) start_video_monitor, system_sess) < 0) {
		free_system_session(system_sess);
		return -1;
	} 
	*/
	
	
	if (pthread_create(&tid, NULL, (void *) grab_sound_thread, NULL) < 0) {
		return -1;
	} 
	sleep(1);

//	sleep(2);
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
	}
#endif

	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_MAIN_PROCESS_START;
	msg.msg[1] = 0;
	ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
	if(ret == -1){
		dbg("send daemon message error\n");
		exit(0);
	}
	msg.msg_type = VS_MESSAGE_ID;
	msg.msg[0] = VS_MESSAGE_MAIN_PROCESS_ALIVE;
	msg.msg[1] = 0;
	while(1) {
		sleep(2);
		ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
		if(ret == -1){
			dbg("send daemon message error\n");
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
	if (stat("/sdcard/imx23_linux.sb", &st) == 0) {
		ret = 0;
		system("flashcp /sdcard/imx23_linux.sb /dev/mtd0");
		system("rm /sdcard/imx23_linux.sb");	
	}
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
		if( ioctl_usbdet_read()){
			system("/data/aa &");
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
	char buf[512];
	fp = fopen(RECORD_PAR_FILE, "w");
	if(!fp){
		printf("write configure file error , something wrong\n");
		return -1;
	}
	for(i=0;i<elements;i++){
		memset(buf,0,512);
		sprintf(buf,"%s=%s\n",allconfig[i].name,allconfig[i].value);
		fwrite(buf,1,strlen(buf),fp);
	}
	fflush(fp);
	fclose(fp);
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
		system("rm /tmp/route");
		return -1;
	}
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

int get_dns(int *dns1 , int *dns2)
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
		while(p==' ' || p=='\t')p++;
		if(strncmp(p,"nameserver",strlen("nameserver"))==0){
			p+=strlen("nameserver");
			while(p==' ' || p=='\t')p++;
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
int set_dns(int *dns1 , int *dns2)
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
int config_wifi(struct configstruct *conf_p, int lines)
{
	int i;
	char buf[256];
	char *argv[4];
	char network_id[32];
	struct stat st;
	for(i=0;i<4;i++){
		argv[i] = (char *)malloc(256);
		if(!argv[i]){
			printf("cannot malloc buff to configure wifi something wrong\n");
			exit(0);
		}
		memset(argv[i],0,256);
	}
	scanresult = (char *)malloc(2048);
	if(!scanresult){
		printf("malloc buff for scanresult error\n");
		goto error;
	}
	if(stat("/data/wpa.conf",&st)<0)
		system("cp /etc/wpa.conf  /data/wpa.conf");
	system("mkdir /tmp/wpa_supplicant");
	system("killall wpa_supplicant");
	sleep(1);
	system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
	sleep(5);
	sprintf(argv[0],"scan");
	mywpa_cli(1,argv);

	sprintf(argv[0],"scan_results");
	mywpa_cli(1,argv);
	
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
	
	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_ssid", 1, buf);
	printf("inet_wlan_ssid = %s\n",buf);
	if(!buf[0]){
		printf("error ssid\n");
		goto error;
	}
	sprintf(argv[0],"set_network");
	//sprintf(argv[1],"0");
	sprintf(argv[1],"%s",network_id);
	sprintf(argv[2],"ssid");
	sprintf(argv[3],"\"%s\"",buf);
	printf("try ssid\n");
	mywpa_cli(4,  argv );
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}

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

	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_proto", 1, buf);
	printf("inet_wlan_proto = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"proto");
		sprintf(argv[3],"%s",buf);
		printf("try proto\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
	}

	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_key_mgmt", 1, buf);
	printf("inet_wlan_key_mgmt = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"key_mgmt");
		sprintf(argv[3],"%s",buf);
		printf("try key_mgmt\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
	}else{
		printf("error key_mgmt \n");
		goto error;
	}

	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_wep_key0", 1, buf);
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

	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_wep_tx_keyindx", 1, buf);
	printf("inet_wlan_wep_tx_keyindx = %s\n",buf);
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
	
	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_pairwise", 1, buf);
	printf("inet_wlan_pairwise = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"pairwise");
		sprintf(argv[3],"%s",buf);
		printf("try pairwise\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
	}

	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_group", 1, buf);
	printf("inet_wlan_group = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"group");
		sprintf(argv[3],"%s",buf);
		printf("try group\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
	}

	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_psk", 1, buf);
	printf("inet_wlan_psk = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"psk");
		sprintf(argv[3],"%s",buf);
		printf("try psk\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
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
	for(i=0;i<4;i++)
		  free(argv[i]);
	return 0;
error:
	if(scanresult)
		free(scanresult);
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
 	scanresult = (char *)malloc(2048);
	if(!scanresult){
		printf("cannot malloc buf for scanresult\n");
		return NULL;
	}
	system("ps -efww |grep wpa_supplicant | grep -v grep > /tmp/twpa");
	stat("/tmp/twpa", &st);
	if(st.st_size <=0){
		system("mkdir /tmp/wpa_supplicant");
		system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
		sleep(1);
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

char * get_parse_scan_result( int *numssid)
 {
 	char *rawbuf;
	char *buf;
	int len;
	char *bssid;
	char *frequency;
	char *signal_leval;
	char *flags;
	char *ssid;
	char proto[256];
	char key_mgmt[256];
	char pairwise[256];
	char group[256];
	char *p;
	char *d;
	int i = 0;
	*numssid = 0;
	rawbuf = scan_wifi(&len);
	if(!rawbuf){
		printf("scan fail\n");
		return NULL;
	}
	buf = (char *)malloc(2048);
	if(!buf){
		printf("cannot malloc buf int parse scan result\n");
		free(rawbuf);
		return NULL;
	}
	memset(buf , 0 , 2048);
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
		if(!*flags){
			//proto = NULL;
		}
		//sprintf(d,"ssid=%s\tsignal_level=%s\tproto=%s\tkey_mgmt=%s\tpairwise=%s\tgroup=%s\n",ssid,signal_leval,);
	}
	return buf;
 }

#define HIDCMD_SCAN_WIFI	0
#define HIDCMD_SET_NETWORK_MODE 1
#define HIDCMD_GET_CONFIG	2
#define HIDCMD_SET_NETWORK_ADDRESS	3
#define HIDCMD_GET_NETWORK_ADDRESS	4

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
	int i;
	char *ip=NULL;
	char *mask=NULL;
	FILE *netconfig_fd;
	
	sleep(1);
	if( open_usbdet() != 0 ){
		printf("open usb detect error\n");
		return -1;
	}

	for( count = 0; count < 2; count++ ){
		report_status_normal();
	}

	if( do_update() == 0 ){
		system("reboot &");
		exit (0);
	}

	if( ioctl_usbdet_read()){
		int hid_fd;
		char hid_cmd[2];
		int data_len;
		int ret;
		int cmd;
		int size;
		
		system("switch gadget && sleep 2");
		if((hid_fd = open("/dev/hidg0", O_RDWR)) != -1 && set_fl( hid_fd, O_NONBLOCK ) != -1 ){
			while( 1 ){
				while(1){
					if(!( ioctl_usbdet_read())){
						system("reboot &");
						exit(0);
					}
					ret = read(hid_fd, &hid_cmd, 2);
					if(ret==2){
						netconfig_fd=fopen("/data/net.config","w");
						if(netconfig_fd==NULL){
							printf("can not open net.config\n");
							return 0;
						}
						break;
					}
				}
				cmd=(int)hid_cmd[0];
				size=(int)hid_cmd[1];
				printf("size==%d\n",size);
				data_len=size;
				i=0;
				ip=buf;
				while(data_len>0){
					ret = read(hid_fd, &hid_cmd, 2);
					if(ret!=2){
						continue;
					}
					buf[i]=hid_cmd[0];
					printf("%c",buf[i]);
					if(buf[i]=='\n'){
						data_len--;
						printf("\ndata_len==%d\n",data_len);
					}
					i++;
					buf[i]=hid_cmd[1];
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
			}
		}
	}
	
	system("switch host");
	sleep(1);
	system("switch host");
	sleep(1);
	
	/*
	system("mkdir /tmp/wpa_supplicant");
	system("killall wpa_supplicant");
	sleep(1);
	system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
	sleep(5);
	
	for(i=0;i<4;i++)
		argv[i]=malloc(256);
	printf("###########begin scan###########\n");
	scanresult = (char *)malloc(2048);
	tryscan = 5;
	while(tryscan){
		sprintf(argv[0],"scan");
		mywpa_cli(1,argv);
		sleep(5);
		sprintf(argv[0],"scan_results");
		mywpa_cli(1,argv);
		if(result_len>48)
			break;
		tryscan --;
	}
	free(scanresult);
	scanresult = NULL;
	printf("############end###############\n");
	for(i= 0; i < 4; i++)
		free(argv[i]);
	*/
//	system("ifconfig eth0 192.168.1.121");
//	system("echo host > /sys/devices/platform/fsl-usb2-otg/status");
	//sleep(3);
	/*
	system("route | grep default > /tmp/route");
	FILE*routefd = fopen("/tmp/route","r");
	if(!routefd){
		printf("get default route error\n");
	}
	memset(buf,0,512);
	if(fgets(buf,512,fd)!=NULL){
		char *p = buf;
		while(p!=' '||p!='\t')p++;
		while(p==' ' || p=='\t')p++;
	}
	*/
	//sprintf(__gate_way,"192.168.1.1");
/*
	if(built_net(check_wlan0)<0)
		system("reboot");
		*/
	
	memset(&threadcfg,0,sizeof(threadcfg));
	pthread_mutex_init(&global_ctx_lock,NULL);
	pthread_mutex_init(&(threadcfg.threadcfglock),NULL);
	//read the config data from video.cfg
	
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
			while(*sp==' '||*sp=='\t')sp++;
			dp=conf_p[lines].value;
			while(*sp!='\n'){
				*dp=*sp;
				dp++;
				sp++;
			}
			printf("name==%s , value=%s\n",conf_p[lines].name,conf_p[lines].value);
			lines++;
			memset(buf,0,512);
		}

		fclose(fd);

		
		threadcfg.cam_id = -1;
		extract_value(conf_p, lines, "cam_id", 0, &threadcfg.cam_id);
		if(threadcfg.cam_id ==-1){
			printf("the config file is error\n");
			return -1;
		}
		printf("cam_id = %d\n",threadcfg.cam_id);
		
		extract_value(conf_p, lines, "name", 1, threadcfg.name);
		printf("name = %s\n",threadcfg.name);

		if(!(int)threadcfg.name[0])
			sprintf(threadcfg.name,"ipcam");

		extract_value(conf_p, lines, "password", 1, threadcfg.password);
		printf("password = %s\n",threadcfg.password);

		extract_value(conf_p, lines, "server_addr", 1, threadcfg.server_addr);
		printf("server_addr = %s\n",threadcfg.server_addr);

		extract_value(conf_p, lines, "monitor_mode", 1, threadcfg.monitor_mode);
		printf("monitor_mode = %s\n",threadcfg.monitor_mode);

		if(!(int)threadcfg.monitor_mode[0])
			sprintf(threadcfg.monitor_mode,"inteligent");

		extract_value(conf_p, lines, "framerate", 0, &threadcfg.framerate);
		printf("framerate= %d\n",threadcfg.framerate);

		if(!threadcfg.framerate)
			threadcfg.framerate = 25;
		else if(threadcfg.framerate<1)
			threadcfg.framerate = 1;
		else if(threadcfg.framerate >25)
			threadcfg.framerate = 25;


		extract_value(conf_p, lines, "compression", 1, threadcfg.compression);
		printf("compression = %s\n",threadcfg.compression);

		extract_value(conf_p, lines, "resolution", 1, threadcfg.resolution);
		printf("resolution = %s\n",threadcfg.resolution);

		if(!(int)threadcfg.resolution[0])
			sprintf(threadcfg.resolution,"vga");

		extract_value(conf_p, lines, "gop", 0, &threadcfg.gop);
		printf("gop = %d\n",threadcfg.gop);
	
		extract_value(conf_p, lines, "rotation_angle", 0, &threadcfg.rotation_angle);
		printf("rotation_angle = %d\n",threadcfg.rotation_angle);

		extract_value(conf_p, lines, "output_ratio", 0, &threadcfg.output_ratio);
		printf("output_ratio = %d\n",threadcfg.output_ratio);

		extract_value(conf_p, lines, "bitrate", 0, &threadcfg.bitrate);
		printf("bitrate = %d\n",threadcfg.bitrate);

		extract_value(conf_p, lines, "brightness", 0, &threadcfg.brightness);
		printf("brightness = %d\n",threadcfg.brightness);

		extract_value(conf_p, lines, "contrast", 0, &threadcfg.contrast);
		printf("contrast = %d\n",threadcfg.contrast);

		extract_value(conf_p, lines, "saturation", 0, &threadcfg.saturation);
		printf("saturation = %d\n",threadcfg.saturation);

		extract_value(conf_p, lines, "gain", 0, &threadcfg.gain);
		printf("gain = %d\n",threadcfg.gain);

		extract_value(conf_p, lines, "record_mode", 1, threadcfg.record_mode);
		printf("record_mode = %s\n",threadcfg.record_mode);

		if(!(int)threadcfg.record_mode[0])
			sprintf(threadcfg.record_mode,"inteligent");

		extract_value(conf_p, lines, "record_normal_speed", 0, &threadcfg.record_normal_speed);
		printf("record_normal_speed= %d\n",threadcfg.record_normal_speed);

		if(!threadcfg.record_normal_speed)
			threadcfg.record_normal_speed = 25;
		else if(threadcfg.record_normal_speed<1)
			threadcfg.record_normal_speed = 1;
		else if(threadcfg.record_normal_speed >25)
			threadcfg.record_normal_speed = 25;

		extract_value(conf_p, lines, "record_normal_duration", 0, &threadcfg.record_normal_duration);
		printf("record_normal_duration= %d\n",threadcfg.record_normal_duration);

		if(threadcfg.record_normal_duration<=0)
			threadcfg.record_normal_duration =1;
		else if(threadcfg.record_normal_duration>25)
			threadcfg.record_normal_duration = 25;



		extract_value(conf_p, lines, "record_sensitivity", 0, &threadcfg.record_sensitivity);
		printf("record_sensitivity = %d\n",threadcfg.record_sensitivity);

		if(threadcfg.record_sensitivity<1||threadcfg.record_sensitivity>3)
			threadcfg.record_sensitivity = 1;

		extract_value(conf_p, lines, "record_slow_speed", 0, &threadcfg.record_slow_speed);
		printf("record_slow_speed = %d\n",threadcfg.record_slow_speed);

		if(threadcfg.record_slow_speed<1||threadcfg.record_slow_speed>25)
			threadcfg.record_slow_speed = 1;

		extract_value(conf_p, lines, "record_slow_resolution", 1, threadcfg.record_slow_resolution);
		printf("record_slow_resolution = %s\n",threadcfg.record_slow_resolution);

		if(!(int)threadcfg.record_slow_resolution[0])
			sprintf(threadcfg.record_slow_resolution,"qvga");

		extract_value(conf_p, lines, "record_fast_speed", 0, &threadcfg.record_fast_speed);
		printf("record_fast_speed = %d\n",threadcfg.record_fast_speed);

		if(threadcfg.record_fast_speed<1||threadcfg.record_fast_speed>25)
			threadcfg.record_fast_speed = 25;

		extract_value(conf_p, lines, "record_fast_resolution", 1, threadcfg.record_fast_resolution);
		printf("record_fast_resolution = %s\n",threadcfg.record_fast_resolution);

		if(!(int)threadcfg.record_fast_resolution[0])
			sprintf(threadcfg.record_fast_resolution,"vga");

		extract_value(conf_p, lines, "record_fast_duration", 0, &threadcfg.record_fast_duration);
		printf("record_fast_duration = %d\n",threadcfg.record_fast_duration);

		if(threadcfg.record_fast_duration<1)
			threadcfg.record_fast_duration = 3;

		extract_value(conf_p, lines, "email_alarm", 0, &threadcfg.email_alarm);
		printf("email_alarm = %d\n",threadcfg.email_alarm);

		if(threadcfg.email_alarm<0)
			threadcfg.email_alarm =0;

		extract_value(conf_p, lines, "mailbox", 1, threadcfg.mailbox);
		printf("mailbox = %s\n",threadcfg.mailbox);

		extract_value(conf_p, lines, "sound_duplex", 0, &threadcfg.sound_duplex);
		printf("sound_duplex = %d\n",threadcfg.sound_duplex);

		if(threadcfg.sound_duplex<0)
			threadcfg.sound_duplex = 0;
		
		extract_value(conf_p, lines, "inet_mode", 1, threadcfg.inet_mode);
		printf("inet_mode = %s\n",threadcfg.inet_mode);

		if(!(int)threadcfg.inet_mode[0])
			sprintf(threadcfg.inet_mode,"inteligent");

		threadcfg.inet_udhcpc = 1;
		extract_value(conf_p, lines, "inet_udhcpc", 0, &threadcfg.inet_udhcpc);
		printf("inet_udhcpc = %d\n",threadcfg.inet_udhcpc);

		extract_value(conf_p, lines, "inet_eth_device", 1, inet_eth_device);
		printf("inet_eth_device = %s\n",inet_eth_device);

		extract_value(conf_p, lines, "inet_wlan_device", 1, inet_wlan_device);
		printf("inet_wlan_device = %s\n",inet_wlan_device);
		
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
					memset(buf,0,512);
					sprintf(buf,"udhcpc -i %s &",inet_eth_device);
					system(buf);
					if(ping_eth>0)
						sleep(5);
					memset(inet_eth_gateway,0,sizeof(inet_eth_gateway));
					if(ping_eth>0)
						get_gateway(inet_eth_device, inet_eth_gateway);
					set_value(conf_p, lines, "inet_eth_gateway", 1, inet_eth_gateway);
					memset(buf,0,512);
					ip = buf;
					mask = buf+256;
					if(ping_eth>0)
						get_ip(inet_eth_device,ip,mask);
					set_value(conf_p, lines, "inet_eth_ip", 1, ip);
					set_value(conf_p, lines, "inet_eth_mask", 1, mask);
				}else{
					memset(buf,0,512);
					ip=buf+256;
					mask = buf + 256+32;
					extract_value(conf_p, lines, "inet_eth_ip", 1, ip);
					printf("inet_eth_ip = %s\n",ip);
					extract_value(conf_p, lines, "inet_eth_mask", 1, mask);
					printf("inet_eth_mask = %s\n",mask);
					sprintf(buf,"ifconfig %s %s netmask %s",inet_eth_device,ip, mask);
					printf("before set ip and mask buf==%s\n",buf);
					system(buf);
					sleep(1);
					
					memset(buf,0,512);
					memset(inet_eth_gateway,0,sizeof(inet_eth_gateway));
					extract_value(conf_p, lines, "inet_eth_gateway", 1, inet_eth_gateway);
					printf("inet_eth_gateway = %s\n",inet_eth_gateway);
					
					sprintf(buf,"route add default   gw  %s  %s",inet_eth_gateway,inet_eth_device);
					printf("before set gateway buf==%s\n",buf);
					system(buf);
					sleep(1);
				}
		}
		if(strncmp(threadcfg.inet_mode,"wlan_only",strlen("wlan_only"))==0
			||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0){
			printf("------------configure wlan----------------\n");
			check_wlan0 = 1;
			if(config_wifi( conf_p, lines)<0){
				ping_wlan = 0;
				printf("configure wifi error check your data\n");
			}else{
				ping_wlan = 1;
				if(threadcfg.inet_udhcpc){
					memset(buf,0,512);
					sprintf(buf,"udhcpc -i %s &",inet_wlan_device);
					system(buf);
					sleep(20);
					memset(inet_wlan_gateway,0,sizeof(inet_wlan_gateway));
					get_gateway(inet_wlan_device, inet_wlan_gateway);
					set_value(conf_p, lines, "inet_wlan_gateway", 1, inet_wlan_gateway);
					memset(buf,0,512);
					ip = buf;
					mask = buf+256;
					get_ip(inet_wlan_device,ip,mask);
					set_value(conf_p, lines, "inet_wlan_ip", 1, ip);
					set_value(conf_p, lines, "inet_wlan_mask", 1, mask);
				}else{
					memset(buf,0,512);
					ip=buf+256;
					mask = buf + 256+32;
					extract_value(conf_p, lines, "inet_wlan_ip", 1, ip);
					printf("inet_wlan_ip = %s\n",ip);
					extract_value(conf_p, lines, "inet_wlan_mask", 1, mask);
					printf("inet_wlan_mask = %s\n",mask);
					sprintf(buf,"ifconfig %s %s netmask %s",inet_wlan_device,ip, mask);
					printf("before set ip and mask buf==%s\n",buf);
					system(buf);
					sleep(1);
					
					memset(buf,0,512);
					memset(inet_wlan_gateway,0,sizeof(inet_wlan_gateway));
					extract_value(conf_p, lines, "inet_wlan_gateway", 1, inet_wlan_gateway);
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

	ret = nand_open("/sdcard");
	if( ret != 0 ){
		printf("open disk error\n");
	}

if (pthread_create(&tid, NULL, (void *) usb_state_monitor, NULL) < 0) {
	return -1;
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
