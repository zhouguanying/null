#ifndef __VPU_SERVER_H__
#define __VPU_SERVER_H__

#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
#include "nand_file.h"
#include "utilities.h"

#define    VS_MESSAGE_ID 0x55aa55aa
#define    VS_MESSAGE_DEBUG 23
#define    VS_MESSAGE_RECORD_ID  0X55bb55bb
#define    VS_MESSAGE_START_RECORDING    1
#define    VS_MESSAGE_STOP_MONITOR        4
// message send to daemon to help do something
#define	VS_MESSAGE_RESTART_ALL		5
#define	VS_MESSAGE_SOFTRESET			6
#define	VS_MESSAGE_START_REPLAY		7
#define	VS_MESSAGE_PAUSE_RECORDING	8
#define	VS_MESSAGE_STOP_REPLAY		9
#define	VS_MESSAGE_REBOOT_SYSTEM	10
#define	VS_MESSAGE_REQUEST_TIMEOVER_PROTECT	11
#define	VS_MESSAGE_STOP_TIMEOVER_PROTECT	12
#define	VS_MESSAGE_DISABLE_WIRTE_NAND	13
#define   VS_MESSAGE_RECORD_ALIVE               14
#define   VS_MESSAGE_MAIN_PROCESS_START  15
#define   VS_MESSAGE_MAIN_PROCESS_ALIVE   16
#define   VS_MESSAGE_DO_UPDATE			17
#define   VS_MESSAGE_DO_CLI_START			18
#define 	VS_MESSAGE_DO_CLI_ALIVE			19
#define 	VS_MESSAGE_UDP_STUN_START		20
#define 	VS_MESSAGE_UDP_STUN_ALIVE		21
#define 	VS_MESSAGE_UPDATE_SYSTEM_TIME	22
#define 	VS_MESSAGE_DEBUG					23
#define 	VS_MESSAGE_CLOSE_RECORD_FILE	24

#define KERNAL_UPDATE_FILE       "/tmp/kernel.update"
#define SYSTEM_UPDATE_FILE        "/tmp/system.update"

#define VS_MESSAGE_NEED_START_HEADER        7
#define VS_MESSAGE_NEED_END_HEADER            8

#define DAEMON_MESSAGE_QUEUE_KEY  456
#define DAEMON_REBOOT_TIME_MAX       60

#define RECORD_SHM_KEY      789
#define RECORD_SHM_SIZE   (64*1024 +4096)
#define RECORD_SHM_FILE_PATH_POS           0
#define RECORD_SHM_FILE_END_HEAD_POS      512
#define RECORD_SHM_INDEX_TABLE_POS        1024
#define RECORD_SHM_ATTR_TABLE_POS             RECORD_SHM_INDEX_TABLE_POS
#define RECORD_SHM_15SEC_TABLE_POS        (RECORD_SHM_ATTR_TABLE_POS + 32*1024)

#define ENCODER_SHM_KEY      790
#define ENCODER_SHM_SIZE   (512*1024 +4096)

#define ENCODER_FRAME_TYPE_NONE	0
#define ENCODER_FRAME_TYPE_I	1
#define ENCODER_FRAME_TYPE_P	2
#define ENCODER_FRAME_TYPE_JPEG	3

#define ENCODER_STATE_WAITCMD		0
#define ENCODER_STATE_WAIT_FINISH	1
#define ENCODER_STATE_FINISHED		2

#define ENCODER_MUTEX_SEM_NAME	"sem"

typedef struct
{
	int width;
	int height;
	int exit;
	
	int para_changed;	// below parameters can be changed dynamically
    int frame_rate;
    int brightness;
    int contrast;
    int saturation;
    int gain;

	int state;	//0: wait for cmd, only main can write; 1: need a frame, only encoder can write; 2: encoder finished, main can write 			
	int force_I_frame;	//must output an I frame even encoder is in process
	int next_frame_type;	//which kind of frame encoder should output
	int data_size;			//compressed data length
	char* data_main;				// compressed data buffer
	char* data_encoder;				//because main and encoder processes' shm address are same, so we have to use two addr 
} encoder_share_mem;

typedef struct
{
    int replay_file_start_sectors;
    int replay_file_seek_percent;
    int sended_frames;
    int v2ipd_to_client0_msg;
    int v2ipd_to_v2ipd_msg;
    int client0_to_v2ipd_msg;
    int record_enabled;
    int monitor_enabled;
    int replay_enabled;
    int total_buf_size;
    int size;
    int rd_ptr;
    int wr_ptr;
    char data[2];
} vs_share_mem;

typedef struct
{
    long msg_type;  //should always = FIFO_CONTRAL_MESSAGE_ID
    char msg[2];
} vs_ctl_message;

#endif


