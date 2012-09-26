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
//#include "udp_transfer.h"
#include "vpu_server.h"
#include "video_cfg.h"

#include "stun.h"
#include "cfg_network.h"
#include "mail_alarm.h"

/*for debug malloc*/
//#include "dbg_malloc.h"


#if 1
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)    do {} while (0)
#endif


#define BUF_SIZE    1024*128

char* test_jpeg_file="/720_480.jpg";
extern int msqid;
struct configstruct
{
    char name[64];
    char value[64];
};

static inline char * gettimestamp()
{
    static char timestamp[15];
    time_t t;
    struct tm *curtm;
    if(time(&t)==-1)
    {
        printf("get time error\n");
        exit(0);
    }
    curtm=localtime(&t);
    sprintf(timestamp,"%04d%02d%02d%02d%02d%02d",curtm->tm_year+1900,curtm->tm_mon+1,
            curtm->tm_mday,curtm->tm_hour,curtm->tm_min,curtm->tm_sec);
    return timestamp;
}

void start_udt_lib();

int test_video_record_and_monitor(struct sess_ctx* system_sess)
{
    pthread_t tid;
    vs_ctl_message msg;
    int ret;
    char buf[32];

    printf("cli is running\n");
    msqid = msgget(DAEMON_MESSAGE_QUEUE_KEY,0666);
    if(msqid ==-1)
    {
        dbg("unable to open daemon message");
        exit (0);
    }

    start_sound_thread();
    sleep(1);
    if (pthread_create(&tid, NULL, (void *) start_video_record, system_sess) < 0)
    {
        free_system_session(system_sess);
        return -1;
    }
    if (pthread_create(&tid, NULL, network_thread, NULL) < 0)
    {
        free_system_session(system_sess);
        return -1;
    }
    playback_init();
    printf("video monitor and record is running\n");
    start_udt_lib();
    sleep(1);
    if( start_cli(system_sess) == NULL )
    {
        printf("cli start error\n");
        return -1;
    }

    sprintf(buf , "%x",threadcfg.cam_id);
    if (pthread_create(&tid, NULL, stun_camera, (void *)buf) < 0)
    {
        return -1;
    }

    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_MAIN_PROCESS_START;
    msg.msg[1] = 0;
    ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
    if(ret == -1)
    {
        dbg("send daemon message error\n");
        system("reboot &");
        exit(0);
    }
    msg.msg_type = VS_MESSAGE_ID;
    msg.msg[0] = VS_MESSAGE_MAIN_PROCESS_ALIVE;
    msg.msg[1] = 0;
    while(1)
    {
        sleep(3);
        ret = msgsnd(msqid , &msg,sizeof(vs_ctl_message) - sizeof(long),0);
        if(ret == -1)
        {
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

    if (stat("/sdcard/aa", &st) == 0)
    {
        ret = 0;
        system("rm /data/aa -rf && cp /sdcard/aa /data/aa -rf");
        system("rm /sdcard/aa");
    }
    /*kernal*/
    if (stat("/sdcard/imx23_linux.sb", &st) == 0)
    {
        ret = 0;
        system("flashcp /sdcard/imx23_linux.sb /dev/mtd0");
        system("rm /sdcard/imx23_linux.sb");
    }
    /*system*/
    if (stat("/sdcard/rootfs.squashfs", &st) == 0)
    {
        ret = 0;
        system("flashcp /sdcard/rootfs.squashfs /dev/mtd1");
        system("rm /sdcard/rootfs.squashfs");
    }
    if (stat("/sdcard/setup.dsk", &st) == 0)
    {
        ret = 0;
        system("flashcp /sdcard/setup.dsk /dev/mtd3");
        system("rm /sdcard/setup.dsk");
    }
    return ret;
}

int usb_state_monitor()
{
    while(1)
    {
        if(is_do_update())
            return 0;
        if( ioctl_usbdet_read())
        {
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
    if(val < 0)
    {
        printf("fcntl get error");
        return -1;
    }

    val |= flags;
    if(fcntl(fd, F_SETFL, val) < 0)
    {
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
    for(i = 0; i < elements; i++)
    {
        if(strncmp(allconfig[i].name,name,strlen(name))==0)
        {
            if(is_string)
            {
                strp = (char *)dst;
                memcpy(strp,allconfig[i].value,64);
            }
            else
            {
                intp = (int *)dst;
                *intp = atoi(allconfig[i].value);
            }
            return 0;
        }
    }
    return -1;
}

static int set_value(struct configstruct *allconfig,int elements, char *name,int is_string,void *value)
{
    int i;
    for(i = 0; i < elements; i++)
    {
        if(strncmp(allconfig[i].name,name,strlen(name))==0)
        {
            if(is_string)
            {
                memcpy(allconfig[i].value,value,64);
            }
            else
            {
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
    if(!fp)
    {
        printf("write configure file error , something wrong\n");
        return -1;
    }
    for(i=0; i<elements; i++)
    {
        memset(buf,0,512);
        sprintf(buf,"%s=%s\n",allconfig[i].name,allconfig[i].value);
        len = strlen(buf);
        if(len!=fwrite(buf,1,len,fp))
        {
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
    if(ret == -1)
    {
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
    else
    {
        _tm.tm_hour = atoi(buf);
        if(_tm.tm_hour==0)
            return -1;
    }
    memset(buf , 0 ,sizeof(buf));
    memcpy(buf ,time+10,2);
    if(*buf=='0'&&*(buf+1)=='0')
        _tm.tm_min = 0;
    else
    {
        _tm.tm_min = atoi(buf);
        if(_tm.tm_min==0)
            return -1;
    }
    memset(buf , 0 ,sizeof(buf));
    memcpy(buf ,time+12,2);
    if(*buf=='0'&&*(buf+1)=='0')
        _tm.tm_sec = 0;
    else
    {
        _tm.tm_sec = atoi(buf);
        if(_tm.tm_sec==0)
            return -1;
    }
    timep = mktime(&_tm);
    tv.tv_sec = timep;
    tv.tv_usec = 0;
    if(settimeofday(&tv , NULL)<0)
    {
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
    if(!conf_p)
    {
        printf("unable to calloc 100 configstruct \n");
        return -1;
    }
    fp =fopen(RECORD_PAR_FILE, "r");
    if(!fp)
    {
        printf("open video.cfg error\n");
        free(conf_p);
        return -1;
    }
    memset(conf_p,0,100*sizeof(struct configstruct));
    lines = 0;
    memset(buf,0,512);
    while(fgets(buf,512,fp)!=NULL)
    {
        sp=buf;
        dp=conf_p[lines].name;
        while(*sp==' '||*sp=='\t')sp++;
        while(*sp&&*sp!='=')
        {
            *dp=*sp;
            dp++;
            sp++;
        }
        sp++;
        while(*sp&&(*sp==' '||*sp=='\t'))sp++;
        dp=conf_p[lines].value;
        while(*sp&&*sp!='\n')
        {
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
    while(*s)
    {
        memset(name,0,sizeof(name));
        memset(value,0,sizeof(value));
        while(*s==' '||*s=='\t')s++;
        d = name;
        while(*s&&*s!='=')
        {
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
        while(*s&&*s!='\n')
        {
            *d =*s;
            d++;
            s++;
        }
        if(*s=='\n')
            s++;
        if(strncmp(name,"system_time",strlen("system_time"))==0)
            /*set_system_time(value)*/;
        else
            set_value(conf_p,lines, name, 1,  value);
    }
out:
    write_config_value(conf_p,lines);
    free(conf_p);
    printf("write config file sucess\n");
    return 0;
}

//"ssid=%s\tsignal_level=%s\tproto=%s\tkey_mgmt=%s\tpairwise=%s\tgroup=%s\n"

int get_cam_id(unsigned int *id)
{
    FILE*fp;
    int id0,id1,id2;
    char buf[64];
    fp = fopen("/sys/uid/otp/id","r");
    if(!fp)
    {
        return -1;
    }
    memset(buf,0,sizeof(buf));
    if(fgets(buf,64,fp)==NULL)
    {
        fclose(fp);
        return -1;
    }
    if(sscanf(buf,"%x %x %x %x",&id0,&id1,&id2,id)!=4)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

#define HID_READ_VIDEO_CFG            1
#define HID_WRITE_VIDEO_CFG            2
#define HID_READ_SEARCH_WIFI           3
#define HID_RESET_TO_DEFAULT           4
#define HID_SET_PSWD                   5
#define HID_SET_SYS_TIME               6
#define HID_GET_WIFI_RESULT           7

//#define HID_FAILE                       4

#define HIDCMD_SCAN_WIFI    0
#define HIDCMD_SET_NETWORK_MODE 1
#define HIDCMD_GET_CONFIG    2
#define HIDCMD_SET_NETWORK_ADDRESS    3
#define HIDCMD_GET_NETWORK_ADDRESS    4
int test_printf_getConfig();
//int querryfs(char *fs , unsigned long long*maxsize,unsigned long long* freesize);
char * get_clean_video_cfg(int *size);
void sig_handle(int signo)
{
    exit(0);
}
void read_pswd()
{
    struct stat st;
    FILE*pswdfp;
    memset(threadcfg.pswd,0,128);
    if(stat(PASSWORD_FILE , &st)==0)
    {
        pswdfp = fopen(PASSWORD_FILE,"r");
        fread(threadcfg.pswd,1,128,pswdfp);
        fclose(pswdfp);
        printf("pswd=%s\n",threadcfg.pswd);
    }
    else
    {
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
    if(!fp)
    {
        printf("open mount file fail\n");
        return -1;
    }
    if(fgets(buf,512,fp)==NULL)
    {
        fclose(fp);
        printf("can't find mount information\n");
        system("rm /tmp/sdcard_mount");
        return -1;
    }
    fclose(fp);
    usleep(50000);
    system("rm /tmp/sdcard_mount");
    s=buf;
    while(*s&&(*s==' '||*s=='\t'))s++;
    if(!*s)
    {
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
    if(need_format == 0)
    {
        memset(buf,0,sizeof(buf));
        sprintf(buf,"mkdosfs  %s",block_name);
        system(buf);
        sleep(1);
    }
    for(i = 0 ; i<3 ; i++)
    {
        argv[i] = (char *)malloc(64);
        if(!argv[i])
        {
            dbg("cannot malloc buf \n");
            exit(0);
        }
    }
    for(i=0; i<3; i++)
        memset(argv[i],0,64);
    sprintf(argv[0],"fatty");
    sprintf(argv[1],"200");
    memcpy(argv[2],block_name,64);
    creat_record_file(3,  argv);
    memset(buf,0,sizeof(buf));
    sprintf(buf,"mount -t auto  %s  /sdcard/",block_name);
    system(buf);
    dbg("prepare record file ok\n");
    for(i=0; i<3; i++)
        free(argv[i]);
    return 0;
}

int update_config_file_carefully()
{
    FILE *fp;
    char buf[256];
    char *old_file_buf;
    char *p;
    int version_flag = 0;
    int pos;
    dbg("try update configure file carefully , it may be slow\n");
    fp = fopen(RECORD_PAR_FILE , "r");
    if(!fp)
    {
        dbg("can not open old config file for write\n");
        exit(0);
    }
    old_file_buf = (char *)malloc(2048);
    if(!old_file_buf)
    {
        dbg("can not malloc buf for old configure file\n");
        exit(0);
    }
    memset(buf,0,sizeof(buf));
    memset(old_file_buf , 0 ,2048);
    pos = 0;
    while(fgets(buf,256,fp)!=NULL)
    {
        p = buf;
        while(*p==' '||*p=='\t')p++;
        if(!version_flag&&strncmp(p,CFG_VERSION,strlen(CFG_VERSION)) ==0)
        {
            version_flag = 1;
        }/*else if(!contrast_flag&&strncmp(p,CFG_CONTRAST,strlen(CFG_CONTRAST))==0){
            contrast_flag = 1;
        }else if(!brightness_flag&&strncmp(p,CFG_BRIGHTNESS,strlen(CFG_BRIGHTNESS))==0){
            brightness_flag = 1;
        }*/else
        {
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
#define HID_RDWR_UNIT        63
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

    if( open_usbdet() != 0 )
    {
        printf("open usb detect error\n");
        return -1;
    }

    if( do_update() == 0 )
    {
        system("reboot &");
        exit (0);
    }
    prepare_record();
    if( ioctl_usbdet_read())
    {
        int hid_fd;
        //FILE *config_fp;
        char hid_unit_buf[HID_RDWR_UNIT];
        unsigned short  data_len;
        char *p;
        char *s;
        int i;
        int ret;
        int cmd;
        int size;
        char *hid_buf;
        int numssid;
        int cfg_len;
        FILE *wifi_fp;
        int scantime = 0;


        system("switch gadget && sleep 2");

        /*check the configure file if it is the newest one*/
        fd = fopen(RECORD_PAR_FILE, "r");
        if(!fd)
        {
            memset(buf,0,512);
            sprintf(buf , "cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
            system(buf);
        }
        else
        {
            p = NULL;
            while(fgets(buf,512,fd)!=NULL)
            {
                p=buf;
                while(*p==' '||*p=='\t')p++;
                if(strncmp(p,CFG_VERSION,strlen(CFG_VERSION))==0)
                {
                    break;
                }
            }
            fclose(fd);
            //usleep(50000);
            if(p&&strncmp(p,CFG_VERSION,strlen(CFG_VERSION))==0)
            {
                while(*p&&*p!='=')p++;
                if(*p=='=')p++;
                while(*p&&(*p==' '||*p=='\t'))p++;
                if(!*p||strncmp(p,CURR_VIDEO_CFG_VERSION,strlen(CURR_VIDEO_CFG_VERSION))!=0)
                {
                    update_config_file_carefully();
                }
            }
            else
            {
                memset(buf,0,512);
                sprintf(buf , "cp %s %s",MONITOR_PAR_FILE , RECORD_PAR_FILE);
                system(buf);
            }
        }

        ioctl_usbdet_led(1);
open_hid:
        if((hid_fd = open("/dev/hidg0", O_RDWR)) != -1 && set_fl( hid_fd, O_NONBLOCK ) != -1 )
        {
            while( 1 )
            {
                dbg("try get hid cmd\n");
                size = 0;
                do
                {
                    if(!( ioctl_usbdet_read()))
                    {
                        sleep(3);
                        if(!( ioctl_usbdet_read()))
                        {
                            system("reboot &");
                            exit(0);
                        }
                        else
                        {
                            close(hid_fd);
                            goto open_hid;
                        }
                    }
                    ret = read(hid_fd, hid_unit_buf + size, HID_RDWR_UNIT - size);
                    if(ret <=0)
                        continue;
                    size  += ret;
                    dbg("get cmd size = %d\n",size);
                    if(size <HID_RDWR_UNIT)
                    {
                        for(i = 0; i<size; i++)
                        {
                            if(hid_unit_buf[i])
                                break;
                        }
                        if(i == size)
                        {
                            break;
                        }
                    }
                }
                while(size <HID_RDWR_UNIT);

                cmd=(int)hid_unit_buf[1];
                dbg("ok begin to excute cmd\n");
                switch(cmd)
                {
                case HID_READ_VIDEO_CFG:
                    printf("HID_READ_VIDEO_CFG\n");
                    if(get_cam_id(&threadcfg.cam_id)<0)
                    {
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
                    s = get_clean_video_cfg(&cfg_len);
                    if(!s)
                    {
                        free(hid_buf);
                        goto hid_fail;
                    }
                    memcpy(p,s,cfg_len);
                    data_len+=cfg_len;
                    p+=cfg_len;
                    free(s);
                    //querryfs("/sdcard", &sd_maxsize, &sd_freesize);
                    sprintf(p,"tfcard_maxsize=%d\n",get_sdcard_size());
                    data_len +=strlen(p);
                    p+=strlen(p);
                    sprintf(p,"system_time=%s\n",gettimestamp());
                    data_len +=strlen(p);
                    //sprintf(p,"tfcard_freesize=%u\n",(unsigned int)(sd_freesize>>20));
                    //data_len +=strlen(p);

                    memcpy(hid_unit_buf,&data_len,sizeof(data_len));
                    ret = write(hid_fd, hid_unit_buf, HID_RDWR_UNIT);
                    p = (char *)&data_len;
                    printf("data_len==%d  %2x %2x\n",(int)data_len,*p ,*(p+1));
                    printf("ret ==%d\n" , ret);
                    printf("##########################\n");
                    sleep(2);
                    for(i = 0; i<(int)data_len; i+=HID_RDWR_UNIT)
                    {
                        do
                        {
                            usleep(20000);
                            ret = write(hid_fd , hid_buf+i ,HID_RDWR_UNIT);
                        }
                        while(ret != HID_RDWR_UNIT);
                    }
                    free(hid_buf);
                    dbg("send configure file sucess\n");
                    break;
                case HID_READ_SEARCH_WIFI:
                    printf("HID_READ_SEARCH_WIFI\n");
                    close(hid_fd);
                    system("switch host");
                    sleep(1);
                    system("switch host");
                    sleep(3);
                    do
                    {
                        hid_buf = get_parse_scan_result(& numssid, NULL);
                    }
                    while(hid_buf == NULL&&(scantime ++)<5);
                    if(hid_buf ==NULL)
                        goto hid_fail;
                    free(hid_buf);
                    system("switch gadget");
                    sleep(5);
                    if((hid_fd = open("/dev/hidg0", O_RDWR)) != -1 && set_fl( hid_fd, O_NONBLOCK ) != -1 )
                        dbg("reopen hid sucess now begin to send data\n");
                    else
                    {
                        dbg("reopen hid error reboot now\n");
                        system("reboot&");
                        exit(0);
                    }
                    break;
                case HID_GET_WIFI_RESULT:
                    printf("HID_GET_WIFI_RESULT\n");
                    hid_buf = (char *)malloc(4096);
                    if(!hid_buf)
                        goto hid_fail;
                    wifi_fp = fopen(RESERVE_SCAN_FILE  , "r");
                    if(!wifi_fp)
                    {
                        free(hid_buf);
                        goto hid_fail;
                    }
                    fseek(wifi_fp , 0 ,SEEK_END);
                    data_len =(unsigned short) ftell(wifi_fp);
                    fseek(wifi_fp, 0 , SEEK_SET);
                    fread(hid_buf , (int)data_len , 1 , wifi_fp);
                    printf("data_len==%d\n",(int)data_len);
                    hid_unit_buf[0]=0xff;
                    hid_unit_buf[1] = HID_READ_SEARCH_WIFI;
                    memcpy(hid_unit_buf+2,&data_len,sizeof(data_len));
                    do
                    {
                        usleep(20000);
                        ret = write(hid_fd, hid_unit_buf, HID_RDWR_UNIT);
                    }
                    while(ret!=HID_RDWR_UNIT);
                    printf("##########################\n");
                    sleep(2);
                    for(i = 0; i<(int)data_len; i+=HID_RDWR_UNIT)
                    {
                        do
                        {
                            usleep(20000);
                            ret = write(hid_fd , hid_buf+i ,HID_RDWR_UNIT);
                        }
                        while(ret != HID_RDWR_UNIT);
                    }
                    free(hid_buf);
                    fclose(wifi_fp);
                    break;
                case HID_WRITE_VIDEO_CFG:
                    printf("HID_WRITE_VIDEO_CFG\n");
                    hid_buf = (char *)malloc(4096);
                    if(!hid_buf)
                        exit(0);
                    memset(hid_buf , 0 ,4096);
                    memcpy(&data_len , hid_unit_buf + 2, 2);
                    printf("data_len==%d\n",(int)data_len);
                    size = 0;
                    while(size <(int)data_len)
                    {
                        do
                        {
                            ret = read(hid_fd , hid_buf+size, (int)data_len - size);
                            if(!( ioctl_usbdet_read()))
                            {
                                system("reboot &");
                                exit(0);
                            }
                        }
                        while(ret <=0);
                        size +=ret;
                    }
                    if(data_len %HID_RDWR_UNIT)
                        read(hid_fd , hid_unit_buf , HID_RDWR_UNIT - (data_len %HID_RDWR_UNIT));
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
                    memcpy(&data_len , hid_unit_buf + 2, 2);
                    memset(buf,0,512);
                    sprintf(buf,PASSWORD_PART_ARG);
                    p = buf;
                    p += strlen(buf);
                    printf("data_len=%d\n",(int)data_len);
                    size = 0;
                    while(size <(int)data_len)
                    {
                        do
                        {
                            ret = read(hid_fd , p+size, (int)data_len - size);
                            if(!( ioctl_usbdet_read()))
                            {
                                system("reboot &");
                                exit(0);
                            }
                        }
                        while(ret <=0);
                        size +=ret;
                    }
                    if(data_len %HID_RDWR_UNIT)
                        read(hid_fd , hid_unit_buf , HID_RDWR_UNIT - (data_len % HID_RDWR_UNIT));
                    data_len +=strlen(PASSWORD_PART_ARG);
                    fd = fopen(PASSWORD_FILE,"w");
                    if(!fd)
                    {
                        printf("open password file fail\n");
                        goto hid_fail;
                    }
                    if(fwrite(buf,1,data_len,fd)!=data_len)
                    {
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
                case HID_SET_SYS_TIME:
                    printf("HID_SET_SYS_TIME\n");
                    hid_unit_buf[16]=0;
                    dbg("time = %s\n",hid_unit_buf +2);
                    set_system_time(hid_unit_buf +2);
                    break;
                default:
                    if(!hid_unit_buf[1])
                        dbg("###########get garbage discard it############\n");
                    break;
hid_fail:
                    system("reboot&");
                    exit(0);
                }
            }
        }
    }

    if (pthread_create(&tid, NULL, (void *) usb_state_monitor, NULL) < 0)
    {
        return -1;
    }
    for( count = 0; count < 2; count++ )
    {
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
    if(fd==NULL)
    {
        printf("open config file error,now try to open reserve config file\n");
        fd=fopen(MONITOR_PAR_FILE,"r");
        if(fd!=NULL)
        {
            int size=0;
            FILE*fdx=fopen(RECORD_PAR_FILE,"w");
            printf("open reserve config file sucess\n");
            if(fdx!=NULL)
            {
                char*buff=malloc(sizeof(char)*256);
                memset(buff,0,256);
                while((size=fread(buff,1,256,fd))!=0)
                {
                    if(size!=fwrite(buff,1,size,fdx))
                    {
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
            }
            else
            {
                printf("create new config file error\n");
                fclose(fd);
                return -1;
            }
            fseek(fd,0,0);
        }
        else
        {
            printf("open reserve config file error\n");
            return -1;
        }
    }
    if(fd==NULL)
    {
        printf("open config file erro\n");
        exit(0);
    }
    else
    {
        int lines;
        printf("try to read config file\n");
        conf_p = (struct configstruct *)calloc(100,sizeof(struct configstruct));
        if(!conf_p)
        {
            printf("unable to calloc 100 configstruct \n");
            fclose(fd);
            return -1;
        }
        memset(conf_p,0,100*sizeof(struct configstruct));
        lines = 0;
        memset(buf,0,512);
        while(fgets(buf,512,fd)!=NULL)
        {
            char *sp=buf;
            char *dp=conf_p[lines].name;
            while(*sp==' '||*sp=='\t')sp++;
            while(*sp!='=')
            {
                *dp=*sp;
                dp++;
                sp++;
            }
            sp++;
            while(*sp&&(*sp==' '||*sp=='\t'))sp++;
            dp=conf_p[lines].value;
            while(*sp&&*sp!='\n')
            {
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
        if(!buf[0]||strncmp(buf,CURR_VIDEO_CFG_VERSION,strlen(CURR_VIDEO_CFG_VERSION))!=0)
        {
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
        if(get_cam_id(&threadcfg.cam_id)<0)
        {
            printf("************************************************\n");
            printf("*            get camera id error,something wrong               *\n");
            printf("************************************************\n");
            return 0;
        }
        //set_value(conf_p, lines, "cam_id", 0, &threadcfg.cam_id);

        printf("cam_id = %x\n",threadcfg.cam_id);

        read_pswd();

        sprintf(threadcfg.name,"ipcam");

        sprintf(threadcfg.server_addr , UDP_SERVER_ADDR);

        extract_value(conf_p, lines, CFG_MONITOR_MODE , 1, threadcfg.monitor_mode);
        printf("monitor_mode = %s\n",threadcfg.monitor_mode);

        if(!(int)threadcfg.monitor_mode[0]||(strncmp(threadcfg.monitor_mode , "normal",6)!=0&&strncmp(threadcfg.monitor_mode , "inteligent",10)!=0))
        {
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

        extract_value(conf_p, lines, CFG_COMPRESSION, 1, threadcfg.compression);
        printf("compression = %s\n",threadcfg.compression);

        extract_value(conf_p, lines, CFG_MONITOR_RESOLUTION, 1, threadcfg.resolution);
        printf("resolution = %s\n",threadcfg.resolution);

        if(strncmp(threadcfg.resolution , "vga",3)!=0&&strncmp(threadcfg.resolution , "qvga",4)!=0)
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

        if(strncmp(threadcfg.record_mode , "normal",6)!=0&&strncmp(threadcfg.record_mode , "inteligent",10)!=0&&
                strncmp(threadcfg.record_mode , "no_record",9)!=0)
        {
            sprintf(threadcfg.record_mode,"normal");
            set_value(conf_p, lines, CFG_RECORD_MODE, 1, (void *)threadcfg.record_mode);
        }

        extract_value(conf_p, lines, CFG_RECORD_RESOLUTION, 1,(void *) threadcfg.record_resolution);
        printf("record_resolution = %s\n",threadcfg.record_resolution);

        if(strncmp(threadcfg.record_resolution , "vga",3)!=0&&strncmp(threadcfg.record_resolution , "qvga",4)!=0)
            memcpy(threadcfg.record_resolution , threadcfg.resolution , 64);
        else
            memcpy(threadcfg.resolution ,threadcfg.record_resolution,64);

        memcpy(threadcfg.original_resolution , threadcfg.record_resolution , sizeof(threadcfg.original_resolution));

        set_value(conf_p, lines, CFG_RECORD_RESOLUTION, 1, (void *)&threadcfg.resolution);
        set_value(conf_p, lines, CFG_MONITOR_RESOLUTION, 1, (void *)&threadcfg.resolution);

        extract_value(conf_p, lines, CFG_RECORD_NORMAL_SPEED, 0, (void *)&threadcfg.record_normal_speed);
        printf("record_normal_speed= %d\n",threadcfg.record_normal_speed);

        if(!threadcfg.record_normal_speed)
            threadcfg.record_normal_speed = 1;
        else if(threadcfg.record_normal_speed<1)
            threadcfg.record_normal_speed = 1;
        else if(threadcfg.record_normal_speed >25)
            threadcfg.record_normal_speed = 25;

        extract_value(conf_p, lines, CFG_RECORD_NORMAL_DURATION, 0, (void *)&threadcfg.record_normal_duration);
        printf("record_normal_duration= %d\n",threadcfg.record_normal_duration);

        if(threadcfg.record_normal_duration<=0)
            threadcfg.record_normal_duration =1;
        else if(threadcfg.record_normal_duration>3)
            threadcfg.record_normal_duration = 3;



        extract_value(conf_p, lines, CFG_RECORD_SENSITIVITY, 0, (void *)&threadcfg.record_sensitivity);
        printf("record_sensitivity = %d\n",threadcfg.record_sensitivity);

        if(threadcfg.record_sensitivity<1||threadcfg.record_sensitivity>3)
            threadcfg.record_sensitivity = 1;

        extract_value(conf_p, lines, CFG_RECORD_SLOW_SPEED, 0, (void *)&threadcfg.record_slow_speed);
        printf("record_slow_speed = %d\n",threadcfg.record_slow_speed);

        if(threadcfg.record_slow_speed<1||threadcfg.record_slow_speed>25)
            threadcfg.record_slow_speed = 1;
        extract_value(conf_p, lines, CFG_RECORD_FAST_SPEED, 0, (void *)&threadcfg.record_fast_speed);
        printf("record_fast_speed = %d\n",threadcfg.record_fast_speed);

        if(threadcfg.record_fast_speed<1||threadcfg.record_fast_speed>25)
            threadcfg.record_fast_speed = 25;
        extract_value(conf_p, lines, CFG_RECORD_FAST_DURATION, 0, (void *)&threadcfg.record_fast_duration);
        printf("record_fast_duration = %d\n",threadcfg.record_fast_duration);

        if(threadcfg.record_fast_duration<1||threadcfg.record_fast_duration>3)
            threadcfg.record_fast_duration = 3;

        extract_value(conf_p, lines, CFG_EMAIL_ALARM, 0, (void *)&threadcfg.email_alarm);
        printf("email_alarm = %d\n",threadcfg.email_alarm);

        if(threadcfg.email_alarm<0)
            threadcfg.email_alarm =0;

        extract_value(conf_p, lines, CFG_MAILBOX, 1, threadcfg.mailbox);
        printf("mailbox = %s\n",threadcfg.mailbox);

        extract_value(conf_p, lines, CFG_MAILSENDER, 1, sender);
        printf("sender = %s\n",sender);

        extract_value(conf_p, lines, CFG_SENDERPSWD, 1, senderpswd);
        printf("senderpswd = %s\n",senderpswd);

        extract_value(conf_p, lines, CFG_SENDERSMTP, 1, mailserver);
        printf("mailserver = %s\n",mailserver);

        extract_value(conf_p, lines, CFG_SOUND_DUPLEX, 0, (void *)&threadcfg.sound_duplex);
        printf("sound_duplex = %d\n",threadcfg.sound_duplex);

        if(threadcfg.sound_duplex<0)
            threadcfg.sound_duplex = 1;

        extract_value(conf_p, lines, CFG_NET_MODE, 1, threadcfg.inet_mode);
        printf("inet_mode = %s\n",threadcfg.inet_mode);

        if(strncmp(threadcfg.inet_mode , "eth_only",8)!=0&&strncmp(threadcfg.inet_mode , "wlan_only",9)!=0&&
                strncmp(threadcfg.inet_mode , "inteligent",10)!=0)
            sprintf(threadcfg.inet_mode,"eth_only");

        if(strncmp(threadcfg.monitor_mode , "inteligent",10)==0)
        {
            if(strncmp(threadcfg.inet_mode , "wlan_only",9)==0)
                threadcfg.framerate = 12;
            else
                threadcfg.framerate = 25;
        }

        init_sleep_time();

        threadcfg.inet_udhcpc = 1;
        extract_value(conf_p, lines, CFG_UDHCPC, 0, (void *)&threadcfg.inet_udhcpc);
        printf("inet_udhcpc = %d\n",threadcfg.inet_udhcpc);

        extract_value(conf_p, lines, CFG_ETH_DEVICE, 1, inet_eth_device);
        printf("inet_eth_device = %s\n",inet_eth_device);

        if(strncmp(inet_eth_device , "eth0",4)!=0)
        {
            memset(inet_eth_device , 0 ,sizeof(inet_eth_device));
            sprintf(inet_eth_device , "eth0");
        }

        extract_value(conf_p, lines, CFG_WLAN_DEVICE, 1, inet_wlan_device);
        printf("inet_wlan_device = %s\n",inet_wlan_device);

        if(strncmp(inet_wlan_device , "wlan0",5)!=0)
        {
            memset(inet_wlan_device , 0 , sizeof(inet_wlan_device));
            sprintf(inet_wlan_device, "wlan0");
        }

        extract_value(conf_p, lines, CFG_WLAN_SSID, 1, w_ssid);
        extract_value(conf_p, lines, CFG_WLAN_KEY, 1, w_key);

        extract_value(conf_p, lines, CFG_ETH_DNS1, 1, e_dns1);
        extract_value(conf_p, lines, CFG_ETH_DNS2, 1, e_dns2);
        extract_value(conf_p, lines, CFG_ETH_IP, 1, e_ip);
        extract_value(conf_p, lines, CFG_ETH_MASK, 1, e_mask);

        extract_value(conf_p, lines, CFG_WLAN_DNS1, 1, w_dns1);
        extract_value(conf_p, lines, CFG_WLAN_DNS2, 1, w_dns2);
        extract_value(conf_p, lines, CFG_WLAN_IP, 1, w_ip);
        extract_value(conf_p, lines, CFG_WLAN_MASK, 1, w_mask);


        check_eth0 = 0;
        check_wlan0 = 0;
        if(strncmp(threadcfg.inet_mode,"eth_only",strlen("eth_only"))==0
                ||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0)
        {
            check_eth0 = 1;
            printf("------------configure eth----------------\n");
            ping_eth = get_netlink_status(inet_eth_device);
            if(ping_eth<0)
                ping_eth = 0;
            if(threadcfg.inet_udhcpc)
            {
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
            }
            else
            {
                memset(buf,0,512);
                ip = buf;
                mask = buf +256;
                extract_value(conf_p, lines, CFG_ETH_DNS1, 1, ip);
                extract_value(conf_p, lines, CFG_ETH_DNS2, 1, mask);
                if(ip[0]||mask[0])
                {
                    set_dns(ip, mask);
                    memset(buf,0,512);
                    sprintf(buf,"ifconfig %s down",inet_eth_device);
                    system(buf);
                    sleep(1);
                    sprintf(buf,"ifconfig %s up",inet_eth_device);
                    system(buf);
                    sleep(1);
                }
                else
                {
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
                if(ip[0]&&mask[0])
                {
                    sprintf(buf,"ifconfig %s %s netmask %s",inet_eth_device,ip, mask);
                    printf("before set ip and mask buf==%s\n",buf);
                    system(buf);
                    sleep(1);
                }
                else
                {
                    printf("it is not dhcp mode but the ip or mask not set , something wrong\n");
                    goto eth_dhcp;
                }
                memset(buf,0,512);
                memset(inet_eth_gateway,0,sizeof(inet_eth_gateway));
                extract_value(conf_p, lines, CFG_ETH_GATEWAY, 1, inet_eth_gateway);
                printf("inet_eth_gateway = %s\n",inet_eth_gateway);
                if(!inet_eth_gateway[0])
                {
                    printf("it is not dhcp mode but the gateway not set\n");
                }
                else
                {
                    sprintf(buf,"route add default   gw  %s  %s",inet_eth_gateway,inet_eth_device);
                    printf("before set gateway buf==%s\n",buf);
                    system(buf);
                    sleep(1);
                }
            }
        }
        if(strncmp(threadcfg.inet_mode,"wlan_only",strlen("wlan_only"))==0
                ||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0)
        {
            printf("------------configure wlan----------------\n");
            check_wlan0 = 1;
            if(config_wifi()<0)
            {
                ping_wlan = 0;
                check_wlan0 = 0;
                printf("configure wifi error check your data\n");
            }
            else
            {
                ping_wlan = 1;
                if(threadcfg.inet_udhcpc)
                {
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
                }
                else
                {
                    memset(buf,0,512);
                    ip = buf;
                    mask = buf +256;
                    extract_value(conf_p, lines, CFG_WLAN_DNS1, 1, ip);
                    extract_value(conf_p, lines, CFG_WLAN_DNS2, 1, mask);
                    if(ip[0]||mask[0])
                    {
                        set_dns(ip, mask);
                        memset(buf,0,512);
                        sprintf(buf,"ifconfig %s down",inet_wlan_device);
                        system(buf);
                        sleep(1);
                        sprintf(buf,"ifconfig %s up",inet_wlan_device);
                        system(buf);
                        sleep(1);
                    }
                    else
                    {
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
                    if(ip[0]&&mask[0])
                    {
                        sprintf(buf,"ifconfig %s %s netmask %s",inet_wlan_device,ip, mask);
                        printf("before set ip and mask buf==%s\n",buf);
                        system(buf);
                        sleep(1);
                    }
                    else
                    {
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

    if(built_net(check_wlan0,  check_eth0,ping_wlan , ping_eth)<0)
    {
        printf("the network is error , check your network \n");
    }
    threadcfg.sdcard_exist = 1;
    ret = nand_open("/sdcard");
    if( ret != 0 )
    {
        threadcfg.sdcard_exist = 0;
        printf("open disk error\n");
    }


    test_set_sound_card();
    if((init_and_start_sound())<0)
    {
        printf("start sound error\n");
        return -1;
    }

    test_video_record_and_monitor(NULL);

    printf("exit program\n");
    return 0;
}
