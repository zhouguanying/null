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
	//int end_sector;
	int real_size; //file size in byte
	//int frame_width;
	//int frame_height;

	unsigned int cur_sector;//for file read or write
	int fd;
}record_file_t;

record_file_t* record_file_open(int start_sector);

/*
	return the real byte num that read. 
	if error, reture -1;
	if get the end, return 0;
*/
int record_file_read(record_file_t* file, unsigned char* buf, int sector_num);


/*
	file size, in byte.
*/
int record_file_get_size(record_file_t* file);


int record_file_seekto(record_file_t* file, unsigned int percent);

int record_file_close(record_file_t* file);


//todo: record_file_write
#ifdef __cplusplus
}
#endif

#endif


