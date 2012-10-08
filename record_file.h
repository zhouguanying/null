#ifndef RECORD_FILE_H_
#define RECORD_FILE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "nand_file.h"

typedef struct record_file
{
    int sequence;
    unsigned int start_sector;
    int real_size; //file size in byte
    unsigned int cur_sector;//for file read or write
    int fd;
    unsigned int index_table_pos;
    char StartTimeStamp[14];
    char LastTimeStamp[14];
} record_file_t;

record_file_t* record_file_open(int start_sector);

int record_file_read(record_file_t* file, unsigned char* buf, int sector_num);
int record_file_get_size(record_file_t* file);
int record_file_seekto(record_file_t* file, unsigned int percent);
int record_file_close(record_file_t* file);

#ifdef __cplusplus
}
#endif

#endif


