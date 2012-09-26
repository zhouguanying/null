#ifndef __NAND_FILE_H__
#define __NAND_FILE_H__

#include <linux/ioctl.h>

#define BLKGETSIZE _IO(0x12,96)    /* return device size /512 (long *arg) */
#define BLK_NAND_WRITE_DATA _IO(0x12,120)
#define BLK_NAND_READ_DATA _IO(0x12,121)

#define MAX_ALLOWED_FILE_NUMBER 500

#define NAND_MODE_IOCTL 1

#if NAND_MODE_IOCTL
#define NAND_BLOCK_SIZE 1024*1024
#else
#define NAND_BLOCK_SIZE 512*1024
#endif

#define SAVE_FILE_BLOCK_SIZE    (1024*1024)
#define SAVE_FILE_BLOCK_COUNT     200
#define FILE_SEGMENT_SIZE        (SAVE_FILE_BLOCK_SIZE*SAVE_FILE_BLOCK_COUNT)

#define MAX_FILE_COUNT        32000/(FILE_SEGMENT_SIZE/(1024*1024))    //32G at most now

#define NAND_RECORD_FILE_SIZE        FILE_SEGMENT_SIZE
#define NAND_RECORD_FILE_SECTOR_SIZE        ( NAND_RECORD_FILE_SIZE / 512 )

#define VS_MESSAGE_NEED_START_HEADER        7
#define VS_MESSAGE_NEED_END_HEADER            8

#define INDEX_TABLE_SIZE        (32*1024)
// the location is from the end of file
#define END_HEADER_LOCATION        (INDEX_TABLE_SIZE/512+1)
#define INDEX_TABLE_LOCATION    (INDEX_TABLE_SIZE/512)

/*recorded files are saved in directory SAVE_FULL_PATH, looks like: RECORDXXXXXXXX.DAT*/
#define SAVE_DISK    "/sdcard"
#define SAVE_DIRECTORY    "IPCAM"
#define SAVE_FULL_PATH    "/sdcard/IPCAM"
#define RECORD_FILE_NAME ""

#define FLAG0_TS_CHANGED_BIT   0
#define FLAG0_FR_CHANGED_BIT   1
#define FLAG0_FW_CHANGED_BIT  2
#define FLAG0_FH_CHANGED_BIT   3

#define SET_ALL_FLAG_CHANGE  ((1<<FLAG0_TS_CHANGED_BIT)|(1<<FLAG0_FR_CHANGED_BIT)|(1<<FLAG0_FW_CHANGED_BIT)|(1<<FLAG0_FH_CHANGED_BIT))



#pragma pack(1)
typedef struct __nand_record_file_internal_header
{
    char head[5]; /*{0,0,0,1,0xc}*/
    char flag[4];
    char StartTimeStamp[14];
    char FrameRateUs[8];
    char FrameWidth[4];
    char FrameHeight[4];
} __attribute__((packed)) nand_record_file_internal_header;
typedef struct __index_table_item
{
    unsigned int location;
} __attribute__((packed)) index_table_item_t;
typedef struct nand_record_file_header
{
    char head[5];
    char PackageSequenceNumber[8];
    char StartTimeStamp[14];
    char LastTimeStamp[14];
    char TotalPackageSize[8];
    char FrameRateUs[8];
    char FrameWidth[4];
    char FrameHeight[4];
} __attribute__((packed)) nand_record_file_header;
#pragma pack()

struct nand_write_request
{
    unsigned int  start;
    unsigned int sector_num;
    unsigned char* buf;
    int erase;
};

struct nand_cache
{
    unsigned char* buf;
    int index;
    int size;
};

extern char  *nand_shm_addr;
extern char *nand_shm_file_path;
extern char *nand_shm_file_end_head;

int nand_open(char* name);
int nand_write(char* buf, int size);
int nand_read(char* buf, int size);
int nand_flush();
int nand_get_size();
long long nand_seekto(long long position);
unsigned int nand_get_position();
int nand_close();

int nand_write_start_header(nand_record_file_header* header);
int nand_write_end_header(nand_record_file_header* header);
int nand_write_index_table(char* index_table);


//下面的API是为回放而设计的
int nand_open_simple(char* name);
int nand_close_simple();
int nand_get_max_file_num();
int nand_get_next_file_start_sector(int cur_sector);
char* nand_get_file_time(int file_start_sector);
int nand_get_sector_num();
int read_file_segment(struct nand_write_request* req);

int nand_clean();
int nand_invalid_file(int file_start_sector, int file_end_sector);

int nand_prepare_record_header(nand_record_file_header* header);
int nand_prepare_close_record_header(nand_record_file_header* header);

int open_download_file(int start_sector);
int check_nand_file(int file_start_sector);

#endif


