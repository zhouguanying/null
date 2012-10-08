#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "record_file.h"
#include "utilities.h"

#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt, __func__, ## args); \
    } while (0)

record_file_t* record_file_open(int start_sector)
{
    record_file_t* file;
    nand_record_file_header header, end;
    int sequence_start, sequence_end;
    int end_sector;
    struct nand_write_request req;
    unsigned char buffer[512];
    int partition_sector_num;

    file = (record_file_t *)malloc(sizeof(record_file_t));
    if (!file)
    {
        printf("mallock error\n");
        return 0;
    }

//    file->fd = open("/dev/nand-user", O_RDWR);
    file->fd = 0;
    if (file->fd < 0)
    {
        printf("open nand-user error\n");
        free(file);
        return 0;
    }

//    ioctl(file->fd, BLKGETSIZE, &partition_sector_num);
    partition_sector_num = nand_get_sector_num();

    req.buf = buffer;
    req.start = start_sector;
    req.sector_num = 1;
//    ioctl(file->fd, BLK_NAND_READ_DATA, &req);
    if (read_file_segment(&req) < 0)
    {
        free(file);
        return 0;
    }
    memcpy(&header, req.buf, sizeof(header));

    end_sector = start_sector + NAND_RECORD_FILE_SECTOR_SIZE;
    if (end_sector >= partition_sector_num)
    {
        end_sector = partition_sector_num;
    }
    end_sector -= END_HEADER_LOCATION * 512 / 512;

    req.buf = buffer;
    req.start = end_sector;
    req.sector_num = 1;
//    ioctl(file->fd, BLK_NAND_READ_DATA, &req);
    if (read_file_segment(&req) < 0)
    {
        free(file);
        return 0;
    }
    memcpy(&end, req.buf, sizeof(header));

    if (header.head[0] != 0 || header.head[1] != 0 || header.head[2] != 0 || header.head[3] != 1 || header.head[4] != 0xc)
    {
        printf("-----------------can't find sequence at START sector:%d\n", start_sector);
        sequence_start = -1;
        memset(file->StartTimeStamp, 0xff, sizeof(file->StartTimeStamp));
    }
    else
    {
        sequence_start = hex_string_to_int(header.PackageSequenceNumber, 8);
        memcpy(file->StartTimeStamp, header.StartTimeStamp, sizeof(file->StartTimeStamp));
    }
    if (end.head[0] != 0 || end.head[1] != 0 || end.head[2] != 0 || end.head[3] != 1 || end.head[4] != 0xc)
    {
        printf("-----------------can't find sequence at END sector:%d\n", start_sector);
        memset(file->LastTimeStamp, 0xff, sizeof(file->LastTimeStamp));
        sequence_end = -1;
    }
    else
    {
        sequence_end = hex_string_to_int(end.PackageSequenceNumber, 8);
        memcpy(file->LastTimeStamp, end.LastTimeStamp, sizeof(file->LastTimeStamp));
    }
    if (sequence_start == sequence_end && sequence_start != -1)
    {
        file->real_size = hex_string_to_int(end.TotalPackageSize, 8);
        file->sequence = sequence_start;
        file->start_sector = start_sector;
        file->cur_sector = 0;
        file->index_table_pos = (NAND_RECORD_FILE_SECTOR_SIZE - INDEX_TABLE_LOCATION) * 512;
        printf("--------%s: a good file, start_sector=%d, length=%d\n",
               __func__, start_sector, file->real_size);
    }
    else if (sequence_start != -1)
    {
        printf("%s: a half good file\n", __func__);
        file->real_size = (NAND_RECORD_FILE_SECTOR_SIZE - NAND_BLOCK_SIZE / 512) * 512;
        file->sequence = sequence_start;
        file->start_sector = start_sector;
        file->cur_sector = 0;
        file->index_table_pos = 0xffffffff;
    }
    else
    {
        printf("%s: a bad file\n", __func__);
        free(file);
        file = NULL;
    }

    return file;
}

/*
    return the real byte num that read.
    if error, reture -1;
    if get the end, return 0;
*/
int record_file_read(record_file_t* file, unsigned char* buf, int sector_num)
{
    int sec_num_to_read;
    int ret;
    struct nand_write_request req;

    if (file->cur_sector * 512 >= file->real_size)
    {
        ret = 0;
    }
    else
    {
        if ((file->cur_sector + sector_num) * 512 > file->real_size)
        {
            sec_num_to_read =
                (file->real_size + 511) / 512 - file->cur_sector;
            ret = file->real_size - file->cur_sector * 512;
        }
        else
        {
            sec_num_to_read = sector_num;
            ret = sector_num * 512;
        }
        req.buf = buf;
        req.start = file->cur_sector + file->start_sector;
        req.sector_num = sec_num_to_read;
//        ioctl(file->fd, BLK_NAND_READ_DATA, &req);
        if (read_file_segment(&req) < 0)
            return -1;

        file->cur_sector += sec_num_to_read;
    }

    return ret;
}

int record_file_get_size(record_file_t* file)
{
    return file->real_size;
}

int record_file_seekto(record_file_t* file, unsigned int percent)
{
    int ret = 0;
    unsigned int sector;

    sector = percent / 512;

    if (sector > (file->real_size + 511) / 512)
    {
        ret = -1;
        dbg("seek error\n");
    }
    else
    {
        file->cur_sector = sector;
    }

    return ret;
}

int record_file_close(record_file_t* file)
{
    if (file->fd)
        close(file->fd);
    free(file);

    return 0;
}

