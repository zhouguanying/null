#ifndef __VPU_SERVER_H__
#define __VPU_SERVER_H__

#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
//#include "vpu_lib.h"
//#include "vpu_io.h"
#include "capture.h"
#include "nand_file.h"
#include "utilities.h"

#define STREAM_ENC_PIC_RESET 1

#define VS_INSTANCE_NUM 2
#define VS_FRAME_BUFFER_NUM 4

//to be removed
#define VS_IS1_FRAME_WIDTH 704
#define VS_IS1_FRAME_HEIGHT 576
#define VS_IS2_FRAME_WIDTH (352)
#define VS_IS2_FRAME_HEIGHT (288)

#define VS_STREAM_BUF_SIZE 0x80000

#define VS_FRAME_BUFFER_STATE_IDLE 0
#define VS_FRAME_BUFFER_STATE_WORKING 1
#define VS_FRAME_BUFFER_STATE_FILLED 2
#define VS_FRAME_BUFFER_STATE_ENCODED 3

#define VS_SHARE_MEMORY_KEY 1000
#define VS_SEMAPHARE_NAME "vpuserver_shm_sem"


#define	VS_MESSAGE_ID 0x55aa55aa
#define   VS_MESSAGE_RECORD_ID  0X55bb55bb
#define	VS_MESSAGE_CLIENT0_ID	0x5555aaaa
#define	VS_MESSAGE_START_RECORDING	1
#define	VS_MESSAGE_STOP_RECORDING	2
#define	VS_MESSAGE_START_MONITOR	3
#define	VS_MESSAGE_STOP_MONITOR		4
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

 #define KERNAL_UPDATE_FILE   	"/tmp/kernel.update"
#define SYSTEM_UPDATE_FILE		"/tmp/system.update"

#define VS_MESSAGE_NEED_START_HEADER		7
#define VS_MESSAGE_NEED_END_HEADER			8

#define DAEMON_MESSAGE_QUEUE_KEY  456
#define DAEMON_REBOOT_TIME_MAX       60

#define V2IPD_SHARE_MEM_KEY		111
#define V2IPD_SHARE_MEM_SIZE		(512*1024)

#define VS_USE_MESSAGE	0
#define VS_FRAMERATE_CTL 1

typedef struct _vs_instance vs_instance;

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
}vs_share_mem;

typedef struct
{
	long msg_type;  //should always = FIFO_CONTRAL_MESSAGE_ID
	char msg[2];
}vs_ctl_message;

typedef struct
{
	unsigned long rd;
	unsigned long wr;
	unsigned long consumed;
	unsigned int size;
}vs_ring_buffer;

#if 0
typedef struct
{
	int index;
	vpu_mem_desc desc;
	FrameBuffer phy;
	unsigned char* virt_server;
	unsigned char* virt_client;
	EncOutputInfo output_info;
	int state;
	int force_iframe;

	//the instance this frame belong to.
	int instance;

	//unsigned char bitstream[1024*512];
}vs_frame;

typedef struct 
{
	unsigned char buf[1024];
	int size;
	int headerType;
}vs_header;

struct _vs_instance
{
	int id;
	EncHandle handle;
	EncOpenParam open_param;
	vpu_mem_desc bit_stream_buf;
	vs_ring_buffer ring;
	vs_frame frame[VS_FRAME_BUFFER_NUM];
	vs_header header;
	int owner;
};

typedef struct
{
	vs_instance* instance;
	unsigned char* bitstream_buffer_virt;
	int shm_handle;
	void* shm_addr;
	sem_t* sem;
}vs_context;
#endif

typedef struct 
{
	int width;
	int height;
	int framerate;
	int bitrate;
}vs_record_parameter;

extern int daemon_msg_queue;
extern int v2ipd_shm_id;
extern vs_share_mem* v2ipd_share_mem;

#if 0
vs_context* vs_client_create_context(int instance_id);
int vs_client_get_header(vs_context* context, unsigned char** buf, int *size);
vs_frame* vs_client_get_idle_frame(vs_context* context);
int vs_client_queue_frame(vs_context* context, vs_frame* frame);
int vs_client_get_output_info(vs_context* context, vs_frame* frame, unsigned char** buf, int *size);
int vs_client_consum_encoded_data(vs_context* context, vs_frame* frame, int size);
int vs_client_destroy_context(vs_context* context);


vs_context* vs_server_create_context(vs_record_parameter* record, vs_record_parameter* monitor);
int vs_server_get_filled_frame(vs_context* context, vs_frame** frame);
int vs_server_encode_frame(vs_context* context, vs_frame* frame);
int vs_server_destroy_context(vs_context* context);
#endif

vs_record_parameter* vs_get_record_para(void);
vs_record_parameter* vs_get_monitor_para(void);

int vs_get_record_config();
int vs_set_record_config(int mode);

#endif


