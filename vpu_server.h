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
#define    VS_MESSAGE_RESTART_ALL        5
#define    VS_MESSAGE_SOFTRESET            6
#define    VS_MESSAGE_REBOOT_SYSTEM    10
#define    VS_MESSAGE_REQUEST_TIMEOVER_PROTECT    11
#define    VS_MESSAGE_STOP_TIMEOVER_PROTECT    12
#define    VS_MESSAGE_DISABLE_WIRTE_NAND    13
#define    VS_MESSAGE_RECORD_ALIVE               14
#define    VS_MESSAGE_MAIN_PROCESS_START  15
#define    VS_MESSAGE_MAIN_PROCESS_ALIVE   16
#define    VS_MESSAGE_DO_UPDATE            17
#define    VS_MESSAGE_DO_CLI_START            18
#define    VS_MESSAGE_DO_CLI_ALIVE            19
#define    VS_MESSAGE_UPDATE_SYSTEM_TIME    22

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


