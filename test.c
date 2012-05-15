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
extern int built_net(int check_wlan0);
extern __gate_way[32];
extern char *scanresult;
extern int result_len;
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

//	sleep(2);
	if (pthread_create(&tid, NULL, (void *) start_video_record, system_sess) < 0) {
		free_system_session(system_sess);
		return -1;
	} 
	if (pthread_create(&tid, NULL, (void *) grab_sound_thread, NULL) < 0) {
		return -1;
	} 
/*
	if (pthread_create(&tid, NULL, (void *) check_net_thread, NULL) < 0) {
		printf("unable to create check net thread\n");
		return -1;
	} 
*/
	playback_init();
	printf("video monitor and record is running\n");

	sleep(3);
	
	if(start_audio_and_video_session()<0){
		printf("unable to start audio and video session\n");
		return -1;
	}
	sleep(1);
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
		sleep(DAEMON_REBOOT_TIME_MAX>>1);
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

#define HIDCMD_SCAN_WIFI	0
#define HIDCMD_SET_NETWORK_MODE 1
#define HIDCMD_GET_CONFIG	2
#define HIDCMD_SET_NETWORK_ADDRESS	3
#define HIDCMD_GET_NETWORK_ADDRESS	4

int main()
{
	int ret;
	int check_wlan0 =0;
	struct sess_ctx * system_sess=NULL;
	int count;
	int tryscan;
	pthread_t tid;
	FILE*fd;
	char buf[512];
	struct configstruct *conf_p;
	int i;
	char *argv[4];
	char *ip=NULL;
	char *mask=NULL;
	char *gateway=NULL;
	char *ssid=NULL;
	char *psk=NULL;
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
	check_wlan0 = 1;
//	system("ifconfig eth0 192.168.1.121");
//	system("echo host > /sys/devices/platform/fsl-usb2-otg/status");
	//sleep(3);
	/*
	netconfig_fd=fopen("/data/net.config","r");
	if(netconfig_fd){
		ip=malloc(256);
		mask=malloc(256);
		gateway=malloc(256);
		ssid=malloc(256);
		psk=malloc(256);
		
		memset(ip,0,256);
		memset(mask,0,256);
		memset(gateway,0,256);
		
		if(fscanf(netconfig_fd,"%s",ip)!=1){
			printf("cannot read ip\n");
			return 0;
		}else{
			fgetc(netconfig_fd);
			printf("ip=%s\n",ip);
		}
		if(fscanf(netconfig_fd,"%s",mask)!=1){
			printf("cannot read mask\n");
			return 0;
		}else{
			fgetc(netconfig_fd);
			printf("mask=%s\n",mask);
		}
		if(fscanf(netconfig_fd,"%s",gateway)!=1){
			printf("cannot read gateway\n");
			return 0;
		}else{
			fgetc(netconfig_fd);
			printf("gateway=%s\n",gateway);
		}
		if(fscanf(netconfig_fd,"%s",ssid)!=1){
			printf("cannot ssid\n");
			free(ssid);
			free(psk);
			ssid=NULL;
			psk=NULL;
			goto __ok;
		}else{
			fgetc(netconfig_fd);
			printf("ssid=%s\n",ssid);
		}
		if(fscanf(netconfig_fd,"%s",psk)!=1){
			printf("cannot read psk\n");
			free(ssid);
			free(psk);
			ssid=NULL;
			psk=NULL;
			goto __ok;
		}else{
			fgetc(netconfig_fd);
			printf("psk=%s\n",psk);
		}
__ok:
		memset(__gate_way,0,32);
		memcpy(__gate_way,gateway,32);
		if(ssid&&psk){
			check_wlan0 =1;
			system("mkdir /tmp/wpa_supplicant");
			system("killall wpa_supplicant");
			sleep(1);
			system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
			sleep(5);
			for(i=0;i<4;i++)
				argv[i]=malloc(256);
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
		
			sprintf(argv[0],"set_network");
			sprintf(argv[1],"0");
			sprintf(argv[2],"ssid");
			sprintf(argv[3],"\"%s\"",ssid);
			printf("try ssid\n");
			mywpa_cli(4,  argv );

			
			sprintf(argv[0],"set_network");
			sprintf(argv[1],"0");
			sprintf(argv[2],"key_mgmt");
			sprintf(argv[3],"NONE");
			printf("try key_mgmt\n");
			mywpa_cli(4,  argv );
			
			sprintf(argv[0],"set_network");
			sprintf(argv[1],"0");
			sprintf(argv[2],"wep_key0");
			sprintf(argv[3],"%s",psk);
			printf("try wep_key0\n");
			mywpa_cli(4,  argv );
			
			sprintf(argv[0],"set_network");
			sprintf(argv[1],"0");
			sprintf(argv[2],"wep_tx_keyidx");
			sprintf(argv[3],"0");
			printf("try wep_tx_keyidx\n");
			mywpa_cli(4,  argv );
			
			sprintf(argv[0],"set_network");
			sprintf(argv[1],"0");
			sprintf(argv[2],"auth_alg");
			sprintf(argv[3],"SHARED");
			printf("try auth_alg\n");
			mywpa_cli(4,  argv );
			sprintf(argv[0],"select_network");
			sprintf(argv[1],"0");
			mywpa_cli(2,argv);
			for(i=0;i<4;i++)
				  free(argv[i]);
			
			sleep(1);
			sprintf(ip_buf,"ifconfig wlan0 %s netmask %s",ip, mask);
			system(ip_buf);
			sleep(3);
			sprintf(ip_buf,"route add default   gw  %s",gateway);
			system(ip_buf);
			sleep(3);
		
			free(ip);
			free(mask);
			free(gateway);
			free(ssid);
			free(psk);
			system("udhcpc -i wlan0");
			sleep(3);
		}
		else{
		
			sprintf(ip_buf,"ifconfig eth0 %s netmask %s",ip, mask);
			system(ip_buf);
			sleep(3);
			sprintf(ip_buf,"route add default   gw  %s",gateway);
			sleep(3);
		
			free(ip);
			free(mask);
			free(gateway);
		}
		system("udhcpc -i eth0");
		sleep(3);
		fclose(netconfig_fd);
	}
	else{
		//system("ifconfig wlan0 down");
		//system("udhcpc -i eth0");
	}
	//system_sess = new_system_session("ipcam");
	*/
	sprintf(__gate_way,"192.168.1.1");
/*
	if(built_net(check_wlan0)<0)
		system("reboot");
		*/
	
	memset(&threadcfg,0,sizeof(threadcfg));
	pthread_mutex_init(&global_ctx_lock,NULL);
	pthread_mutex_init(&acceptlock,NULL);
	pthread_mutex_init(&strange_thing_lock , NULL);
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

		free(conf_p);
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
	test_video_record_and_monitor(system_sess);

	printf("exit program\n");
	return 0;
}
