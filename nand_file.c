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
#include <sys/vfs.h>
#include <dirent.h>
#include <time.h> 
#include <pthread.h>

#include "nand_file.h"
#include "utilities.h"
#include "vpu_server.h"

#if 1
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt, __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif

char  *nand_shm_addr;
char *nand_shm_file_path;
char *nand_shm_file_end_head;

#define LIMITED_NAND_FILE_SIZE_SECTORS NAND_RECORD_FILE_SECTOR_SIZE*0

int read_file_segment(struct nand_write_request* req);
int write_file_segment(struct nand_write_request* req);

static int fd = -1;
static struct nand_cache cache;
static int cur_sector = 0;
static int header_sector = 0;
static int partition_sector_num = 0;
static int max_sequence;
static int nand_blocked = 1;
static int record_file_size = 0;
static pthread_mutex_t  write_file_lock;
static int nand_shm_id;

static pthread_rwlock_t  file_index_table_lock;

#if 0
int nand_find_start_sector()
{
	int sectors;
	int ret = 0;
	int sequence = 0;
	nand_record_file_header header;
	int cur_max_sequence = -1;
	
	if( partition_sector_num == 0 )
		return -1;

	for( sectors = 0 ; sectors < partition_sector_num; sectors += NAND_RECORD_FILE_SECTOR_SIZE ){
		lseek64(fd, (int64_t)sectors*(int64_t)512, SEEK_SET);
		read( fd, (char*)&header, sizeof(header));
		if( header.head[0]!=0 || header.head[1]!=0 || header.head[2]!=0 || header.head[3]!=1 || header.head[4] != 0xc ){
			ret = sectors;
			max_sequence = cur_max_sequence+1;
//			dbg("find start sector 0\n");
			goto found;
		}
		sequence = hex_string_to_int(header.PackageSequenceNumber, sizeof(header.PackageSequenceNumber));
//		dbg("sequence:%d,cur_max_sequence:%d\n",sequence, cur_max_sequence);
		if( sequence >= cur_max_sequence ){
			cur_max_sequence = sequence;
		}
		else{
			ret = sectors;
			max_sequence = cur_max_sequence+1;
//			dbg("find start sector 1\n");
			goto found;
		}
	}
	sectors = 0;
	ret = sectors;
	max_sequence = cur_max_sequence+1;
	
found:
//	printf("found the start sector at %d, sequence=%d\n", ret, max_sequence);
	return ret;
}
#else
static int nand_get_sequence( int sectors )
{
	nand_record_file_header header;
	unsigned int data;
	int sequence;
	unsigned char buf[512];
	struct nand_write_request req;

#if 1
	req.buf = buf;
	req.start = sectors;
	req.sector_num = 1;
//	ioctl(fd, BLK_NAND_READ_DATA, &req);
	if(read_file_segment(&req)<0)
		return -1;
	memcpy(&header, req.buf, sizeof( header ));
#else
	lseek64(fd, (int64_t)sectors*(int64_t)512, SEEK_SET);
	read( fd, (char*)&header, sizeof(header));
#endif
	if( header.head[0]!=0 || header.head[1]!=0 || header.head[2]!=0 || header.head[3]!=1 || header.head[4] != 0xc ){
		memcpy(&data, header.head, 4);
		dbg("***********************************can't find sequence at sector:%x, head=%x ,partition_sector_num = %d \n", sectors,data , partition_sector_num);
		return -1;
	}
	sequence = hex_string_to_int(header.PackageSequenceNumber, sizeof(header.PackageSequenceNumber));
//	dbg("find a sequence at sector:%d, sequence=%d\n",sectors,sequence);
	return sequence;
}

int nand_find_start_sector()
{
	int sectors, next_sector;
	int ret = 0;
	int sequence = 0;
	int cur_max_sequence = -1;
	int sequence2;
	
	if( partition_sector_num == 0 )
		return -1;

	for( sectors = 0 ; sectors < partition_sector_num; sectors += NAND_RECORD_FILE_SECTOR_SIZE ){
		next_sector = sectors + NAND_RECORD_FILE_SECTOR_SIZE;
		if( next_sector >= partition_sector_num ){
			next_sector = 0;
		}
		sequence = nand_get_sequence( sectors );
//		dbg("sectors=%d, sequence=%d\n", sectors, sequence);
		if( sectors == 0 ){
			if( sequence != -1 ){
				cur_max_sequence = sequence;
				continue;
			}
			cur_max_sequence = -1;
			goto found;
		}
		if( sequence == cur_max_sequence + 1 || sequence == cur_max_sequence /*it the sectors is skip from previos*/ ){
			cur_max_sequence = sequence;
			continue;
		}
		//cur_max_sequence > sequence, maybe it is the last one
		//dbg("try next_sector:%x\n",next_sector);
		sequence2 = nand_get_sequence( next_sector );
		if( sequence2 == -1 ){
			dbg("sequence2 = -1, so we found\n");
			goto found;
		}
		//dbg("try second sectors, max=%d,sequence2=%x\n", cur_max_sequence, sequence2);
		if( cur_max_sequence + 2 == sequence2 ){
			cur_max_sequence = sequence2;
			continue;
		}
		else{
			goto found;
		}
	}
	sectors = 0;
	
found:
//	printf("found the start sector at %d, sequence=%d\n", ret, max_sequence);
	ret = sectors;
	max_sequence = cur_max_sequence + 1;
	//dbg("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^find the start of sector:%d, sequence=%d\n", sectors, max_sequence);
	return ret;
}
#endif

//return the free disk size, unit is M = 1024*1024
int get_disk_free_size(char* disk)
{
	struct statfs s;
	
	if (statfs(disk, &s) != 0) {
		printf("get disk stat error\n");
		return -1;
	}
//	printf("block_size=%d, total_blocks=%d, free_blocks=%d\n", s.f_bsize, s.f_blocks, s.f_bfree);
	return s.f_bfree * ( s.f_bsize / 1024 ) / 1024;
}

int mk_save_dir(char* dir)
{
	struct stat st;
	
	if (stat(dir, &st) != 0) {
		if (mkdir(dir, 0777) < 0) {
			printf("create dir error\n");
			return -1;
		}
	}else{
		if(!S_ISDIR(st.st_mode)){
			printf("%s is existed, and isn't a directory\n",dir);
			return -1;
		}
	}
	printf("create directory: %s ok\n", dir);
	return 0;
}

struct FILE_INDEX_TABLE
{
	int index_count;
	int cur_index;
	int cur_fd;
	char* table[MAX_FILE_COUNT];	//for 32G SDCARD, 32000/200 = 210 
};
struct FILE_INDEX_TABLE file_index_table;

static int add_to_file_index_table(char* full_path_file_name)
{
	int n, pos;
	for( pos = 0; pos < file_index_table.index_count; pos++ ){
		if( strcmp( full_path_file_name, file_index_table.table[pos] ) < 0 ){
			break;
		}
	}
	if( pos >= MAX_FILE_COUNT ){
		return -1;
	}
	for( n = file_index_table.index_count; n > pos; n-- ){
		file_index_table.table[n] = file_index_table.table[n-1];
	}
	file_index_table.table[pos] = full_path_file_name;
	file_index_table.index_count++;
	return 0;
}

static int free_file_index_table()
{
	return 0;
}

static void init_file_index_table()
{
	file_index_table.index_count = 0;
	file_index_table.cur_index = -1;
	file_index_table.cur_fd = -1;
}
int open_download_file(int start_sector)
{
	int file_index;
	file_index = start_sector/ ( FILE_SEGMENT_SIZE / 512 );
	if(!file_index_table.table[file_index])
		return -1;
	return (open( file_index_table.table[file_index], O_RDONLY));
}

int read_file_segment(struct nand_write_request* req)
{
	int file_index,file_offset;
	int fd;
	int ret;
	
	file_index = req->start / ( FILE_SEGMENT_SIZE / 512 );
	file_offset = ( req->start % ( FILE_SEGMENT_SIZE / 512 ))*512;
	pthread_rwlock_rdlock(&file_index_table_lock);
	if(!file_index_table.table[file_index]){
		pthread_rwlock_unlock(&file_index_table_lock);
		return -1;
	}
	fd = open( file_index_table.table[file_index], O_RDONLY);
	if( fd == -1 ){
		printf("open read file error file_index=%d ,  file=%s\n" , file_index,file_index_table.table[file_index]);
		pthread_rwlock_unlock(&file_index_table_lock);
		return -1;
	}
	lseek(fd, file_offset, SEEK_SET);
	if( file_offset + req->sector_num * 512 <= FILE_SEGMENT_SIZE ){
		ret = read( fd, req->buf, req->sector_num * 512 );
		if( ret != req->sector_num * 512 ){
			close(fd);
			pthread_rwlock_unlock(&file_index_table_lock);
			return -1;
		}
	}
	else{
		printf("read size is too large\n");
		close(fd);
		pthread_rwlock_unlock(&file_index_table_lock);
		return -1;
	}
	close( fd );
	pthread_rwlock_unlock(&file_index_table_lock);
	return 0;
}

int write_file_segment(struct nand_write_request* req)
{
	int file_index, file_offset;
	int fd ;

	pthread_mutex_lock(&write_file_lock);
	fd = file_index_table.cur_fd;
	file_index = req->start / ( FILE_SEGMENT_SIZE / 512 );
	file_offset = ( req->start % ( FILE_SEGMENT_SIZE / 512 ))*512;
	if(!file_index_table.table[file_index])
			goto error;
	if( file_index != file_index_table.cur_index ){
		if( fd != -1 ){
			close( fd );
		}
		fd = file_index_table.cur_fd = open( file_index_table.table[file_index], O_WRONLY);
		if( fd == -1 ){
			pthread_rwlock_wrlock(&file_index_table_lock);
			printf("open file segment error:%s\n", file_index_table.table[file_index]);
			if(strcmp(file_index_table.table[file_index] , nand_shm_file_path)==0)
				memset(nand_shm_file_path , 0 , strlen(nand_shm_file_path));
			free(file_index_table.table[file_index]);
			file_index_table.table[file_index] = NULL;
			dbg("#############file open error discard it ##########\n");
			pthread_rwlock_unlock(&file_index_table_lock);
			goto error;
		}
		file_index_table.cur_index = file_index;
		dbg("file segment %s opened ok\n", file_index_table.table[file_index]);
	}
	//dbg("write to file segment: file_index=%d, file_offset=%x, sector_num=%x\n", file_index, file_offset, req->sector_num);
	lseek(fd, file_offset, SEEK_SET);
	if( file_offset + req->sector_num * 512 <= FILE_SEGMENT_SIZE ){
		if( write(fd, req->buf, req->sector_num*512) != req->sector_num*512 ){
			printf("write error\n");
			goto error;
		}
		//return 0;
	}
	else{
		printf("write size is too large\n");
		goto error;
	}
	pthread_mutex_unlock(&write_file_lock);
	return 0;
error:
	pthread_mutex_unlock(&write_file_lock);
	return -1;
}

static int get_partition_sector_number()
{
	return file_index_table.index_count * ( FILE_SEGMENT_SIZE / 512 ); 
}
static int create_new_file_segment(char* fp_dir)
{
	int max_index = file_index_table.index_count;
	char buf[100];
	char* p;
	unsigned new_index;
	int new_file;

	if( max_index == 0 ){
		new_index = 0;
	}
	else{
		p = file_index_table.table[max_index-1];
		p += strlen(SAVE_FULL_PATH) + 1;
		new_index = dec_string_to_int(p,8);
		new_index++;
	}
	sprintf(buf,"%s/%.8d.dat", fp_dir, new_index);
	printf("file_segment_name=%s\n", buf);
	if( (new_file = open(buf,O_CREAT|O_RDWR))== -1 ){
		return -1;
	} 
	if( ftruncate(new_file, FILE_SEGMENT_SIZE) == -1 ){
		return -1;
	}
	close(new_file);
	file_index_table.table[max_index] = malloc(strlen(buf)+1);
	strcpy(file_index_table.table[max_index], buf );
	file_index_table.index_count++;
	return 0;
}

int create_file_segments(int free_msize, char* fp_dir)
{
	DIR *dp;
	struct dirent *entry;
	char* full_name;
	//int size;
	struct stat st;
	
	dp = opendir(fp_dir);
	if( !dp ){
		printf("%s:open dir %s error\n", __func__, fp_dir);
		return -1;
	}

	while ((entry = readdir(dp)) != NULL) {
		if( strlen(entry->d_name) == 12 && !strncmp(entry->d_name+8,".dat", 4)){
			int len = strlen(fp_dir) + strlen(entry->d_name) + 2 ;
			full_name = malloc(len);
			if( !full_name ){
				free_file_index_table();
				return -1;
			}
			sprintf(full_name,"%s%s%s", fp_dir, "/", entry->d_name);
			if (stat(full_name, &st) != 0) {
				continue;
			}else{
				if( st.st_size != FILE_SEGMENT_SIZE ){
					printf("remove damagered file:%s\n",full_name);
					unlink(full_name);
					continue;
				}
			}
			add_to_file_index_table(full_name);
		}
	}
	closedir(dp);
/*	
	while(( size = get_disk_free_size(fp_dir)) > FILE_SEGMENT_SIZE/(1024*1024) + 100 ){
		if( create_new_file_segment(fp_dir) ){
			printf("create file segment error\n");
			return -1;
		}
	}
*/	
	return 0;
}

int nand_open(char* name)
{
	int disk_free_size;

	pthread_mutex_init(&write_file_lock , NULL);
	pthread_rwlock_init(&file_index_table_lock , NULL);
	if((nand_shm_id = shmget(RECORD_SHM_KEY ,RECORD_SHM_SIZE ,0666))<0){
		perror("shmget : open");
		exit(0);
	}
	if((nand_shm_addr = (char *)shmat(nand_shm_id , 0 , 0))<0){
		perror("shmat :");
		exit(0);
	}
	nand_shm_file_path = nand_shm_addr + RECORD_SHM_FILE_PATH_POS;
	nand_shm_file_end_head = nand_shm_addr + RECORD_SHM_FILE_END_HEAD_POS;
	memset(nand_shm_file_path , 0 ,512);
	init_file_index_table();
	if( mk_save_dir(SAVE_FULL_PATH) ){
		return -1;
	}
//	disk_free_size = get_disk_free_size(SAVE_DISK);
	disk_free_size = 0;	//get_disk_free_size will last very long, so we skip it because yyf will promise the creation of record files
	if( create_file_segments( disk_free_size, SAVE_FULL_PATH ) != 0 ){
		return -1;
	}

	int i;
	for( i = 0; i < file_index_table.index_count; i++ ){
		printf("index file=%s\n", file_index_table.table[i]);
	}

	memset(&cache, 0, sizeof(struct nand_cache));
	cache.buf = malloc(NAND_BLOCK_SIZE);
	if(cache.buf == 0){
		printf("%s: malloc error\n", __func__);
		return -1;
	}
	cache.size = NAND_BLOCK_SIZE;
	cache.index = 0;
	partition_sector_num = get_partition_sector_number();
#if LIMITED_NAND_FILE_SIZE_SECTORS
	partition_sector_num = LIMITED_NAND_FILE_SIZE_SECTORS;
#endif
	printf("%s: sector num=%d\n", __func__, partition_sector_num);
	header_sector = cur_sector = nand_find_start_sector();
	return 0;
}

int nand_write_start_header(nand_record_file_header* header)
{
	char buffer[20];
	struct nand_write_request req;
	int file_index;
	//unsigned int flag;
	char *buf;
	int len;
	sprintf( buffer,"%08x", max_sequence );
	memcpy( header->PackageSequenceNumber, buffer, sizeof(header->PackageSequenceNumber));
	memcpy( cache.buf, (char*)header, sizeof( nand_record_file_header ));
	record_file_size  = cache.index = sizeof( nand_record_file_header );
	nand_blocked = 0;
	dbg("********************write start header, sequence=%d, sector=%x***************************\n", max_sequence, cur_sector );
	file_index = header_sector/ ( FILE_SEGMENT_SIZE / 512 );
	if(!file_index_table.table[file_index]){
		dbg("file index table error\n");
		return 0;
	}
	len =  strlen(file_index_table.table[file_index]);
	memcpy(nand_shm_file_path , file_index_table.table[file_index] , len);
	nand_shm_file_path[len] = 0;
	buf = (char *)malloc(1024);
	if(!buf)
		return 0;
	memset(buf , 0 , 1024);
	//flag = 0xfffffffe;
	//memcpy(buf , &flag ,sizeof(flag));
	req.start = header_sector + NAND_RECORD_FILE_SECTOR_SIZE;
	if( req.start >= partition_sector_num ){
		req.start = partition_sector_num;
	}
	req.start -= END_HEADER_LOCATION*512/512;
	req.sector_num = 2;
	req.buf = (unsigned char *)buf;
	req.erase = 1;
	write_file_segment(&req);
	dbg("##################mask file ok###############\n");
	free(buf);
	return 0;
}

void nand_update_file_status()
{
	char buffer[20];
	struct nand_record_file_header *header;
	time_t timep;
	struct tm * gtm;

	header = (struct nand_record_file_header*)nand_shm_file_end_head;
	memset(buffer,0,20);
	sprintf( buffer,"%08x", record_file_size);
	memcpy( header->TotalPackageSize, buffer, sizeof(header->TotalPackageSize));
	sprintf(buffer,"%08x",max_sequence);
	memcpy(header->PackageSequenceNumber , buffer , sizeof(header->PackageSequenceNumber));
	time (&timep);
	gtm = localtime(&timep);
	sprintf(buffer,"%04d%02d%02d%02d%02d%02d",gtm->tm_year+1900,gtm->tm_mon+1,gtm->tm_mday,gtm->tm_hour,gtm->tm_min,gtm->tm_sec);
	memcpy(header->LastTimeStamp, buffer, sizeof(header->LastTimeStamp));
}

int nand_write_end_header(nand_record_file_header* header)
{
	struct nand_write_request req;
	char buf[20];
	
	sprintf(buf , "%08x",max_sequence);
	memcpy(header->PackageSequenceNumber , buf , sizeof(header->PackageSequenceNumber));

	memcpy( cache.buf, (char*)header, sizeof( nand_record_file_header ));
	cache.index =  sizeof( nand_record_file_header );
	memset(&(cache.buf[cache.index]), 0xff, 512 - cache.index);
	req.start = header_sector + NAND_RECORD_FILE_SECTOR_SIZE;
	if( req.start >= partition_sector_num ){
		req.start = partition_sector_num;
	}
	req.start -= END_HEADER_LOCATION*512/512;
	req.sector_num = 512/512;
	req.buf = cache.buf;
	req.erase = 1;
	dbg("#######################write back header, start sector=%x,end sector:%x#######################\n", header_sector,req.start);
	//ioctl(fd, BLK_NAND_WRITE_DATA, &req);
	write_file_segment(&req);
	cache.index = 0;
	max_sequence ++;
	header_sector += NAND_RECORD_FILE_SECTOR_SIZE;
	cur_sector = header_sector;
	if( header_sector >= partition_sector_num ){
		cur_sector = header_sector = 0;
	}
	nand_blocked = 0;
	return 0;
}

int nand_write_index_table(char* index_table)
{
	struct nand_write_request req;

	req.start = header_sector + NAND_RECORD_FILE_SECTOR_SIZE;
	if( req.start >= partition_sector_num ){
		req.start = partition_sector_num;
	}
	req.start -= INDEX_TABLE_LOCATION*512/512;
	req.sector_num = INDEX_TABLE_SIZE/512;
	req.buf = (unsigned char *)index_table;
	req.erase = 1;
	dbg("#######################write index table, start sector=%x,end sector:%x#######################\n", header_sector,req.start);
	//ioctl(fd, BLK_NAND_WRITE_DATA, &req);
	write_file_segment(&req);
	return 0;
}

static char file_all_error = 0;
int nand_write(char* buf, int size)
{
	struct nand_write_request req;
	int end_sector;
	int write_size=size;
	int i;
	//printf("%s: enter, size=%d, buf=0x%x\n", __func__, size, buf);

	if(file_all_error)
		return 0;
#ifndef NAND_MODE_IOCTL
	while(size + cache.index >= cache.size){
		memcpy(&(cache.buf[cache.index]), buf, cache.size - cache.index);
		write(fd, cache.buf, cache.size);
		size -= cache.size - cache.index;
		buf += cache.size - cache.index;
		cache.index = 0;
	}

	if(size){
		//printf("%s: cache, index=%d, buf=0x%x\n", __func__, cache.index, cache.buf);
		memcpy(&(cache.buf[cache.index]), buf, size);
		cache.index += size;
	}
#else
	end_sector = cur_sector + ( cache.index + size ) / 512 + 1;
//	dbg("write:header_sector=%d,cur_sector=%d,cach.index=%d,end_sector=%d\n", header_sector,cur_sector, cache.index,end_sector );
	if( header_sector == cur_sector && cache.index == 0 ){
		// TODO: we need a header data at first
//		dbg("need to open a header data file\n");
		nand_blocked = 1;
		return VS_MESSAGE_NEED_START_HEADER;
	}
	else if( end_sector + END_HEADER_LOCATION*512/512 >= header_sector + NAND_RECORD_FILE_SECTOR_SIZE ||end_sector + END_HEADER_LOCATION*512/512 >= partition_sector_num ){
		// TODO:  should close a header at last		
//		dbg("need to close a header data file\n");
		if( cache.index != 0 ){
			nand_flush();
		}
		nand_blocked = 1;
		return VS_MESSAGE_NEED_END_HEADER;
	}
	if( nand_blocked ){
//		dbg("-----------------nand write is blocked-----------------------\n");
		return -1;
	}
	record_file_size += write_size;
	while(size + cache.index >= cache.size){
		memcpy(&(cache.buf[cache.index]), buf, cache.size - cache.index);
		//write(fd, cache.buf, cache.size);
		req.start = cur_sector;
		req.sector_num = NAND_BLOCK_SIZE/512;
		req.buf = cache.buf;
		req.erase = 1;
		//ioctl(fd, BLK_NAND_WRITE_DATA, &req);
		if(write_file_segment(&req)<0){
			for(i=0 ; i < file_index_table.index_count ; i++){
				if(file_index_table.table[i])
					return -1;
			}
			dbg("#############all file error stop record now###########\n");
			file_all_error = 1;
			return 0;
		}
		nand_update_file_status();
		size -= cache.size - cache.index;
		buf += cache.size - cache.index;
		cache.index = 0;
		cur_sector += req.sector_num;
		if(cur_sector >= partition_sector_num){
			cur_sector = 0;
			dbg("the sector count is error, start=%d, size=%d++++++++++++++++++++\n", req.start, req.sector_num);
		}
	}

	if(size){
		//printf("%s: cache, index=%d, buf=0x%x\n", __func__, cache.index, cache.buf);
		memcpy(&(cache.buf[cache.index]), buf, size);
		cache.index += size;
	}

#endif

	//printf("%s: leave\n", __func__);
	return 0;
}

int nand_flush()
{
	int ret = 0;
	struct nand_write_request req;

	//printf("%s: enter\n", __func__);
#ifndef NAND_MODE_IOCTL
	if(cache.index){
		ret = write(fd, cache.buf, cache.index);
		cache.index = 0;
	}
#else
	if(cache.index){
		//ret = write(fd, cache.buf, cache.index);
		memset(&(cache.buf[cache.index]), 0xff, cache.size - cache.index);
		req.start = cur_sector;
		req.sector_num = cache.index/512+1;
		req.buf = cache.buf;
		req.erase = 1;
		//printf("nand write: buf[0] = 0x%x\n", ((int*)req.buf)[0]);
		//ioctl(fd, BLK_NAND_WRITE_DATA, &req);
		write_file_segment(&req);
		cache.index = 0;
	}
#endif

	return ret;
}

int nand_read(char* buf, int size)
{
	//printf("%s: enter\n", __func__);
	nand_flush();
	return read(fd, buf, size);
}

int nand_get_size(char* disk)
{
	int sector_size;

	// file_index_table may not be built yet
	sector_size = file_index_table.index_count * ( FILE_SEGMENT_SIZE / 512 );
	printf("to do 0\n");
//	dbg("%s have total %d sectors\n", disk, sector_size);
	return sector_size;
}

long long nand_seekto(long long position)
{
	long long ret;
	
	nand_flush();
	ret = lseek64(fd, position, SEEK_SET);
	return ret;
}
unsigned int nand_get_position()
{
	return (unsigned int)record_file_size;
}

int nand_close()
{
	if(fd >= 0){
		nand_flush();
		close(fd);
		fd = -1;
	}

	if(cache.buf){
		free(cache.buf);
		memset(&cache, 0, sizeof(struct nand_cache));
	}
	return 0;
}

//下面的API是为回放功能而做的
static char file_time_buffer[64];
int nand_open_simple(char* name)
{
	printf("to do 2\n");
	return 0;
//	fd = open(name, O_RDONLY);
//	ioctl(fd, BLKGETSIZE, &partition_sector_num);
#if LIMITED_NAND_FILE_SIZE_SECTORS
	partition_sector_num = LIMITED_NAND_FILE_SIZE_SECTORS;
#endif

	return fd;
}

int nand_close_simple( )
{
	if(fd>=0){
		close(fd);
		fd = -1;
	}
	return 0;
}

int nand_get_max_file_num()
{
	return partition_sector_num / NAND_RECORD_FILE_SECTOR_SIZE + 1;
}

int nand_get_sector_num()
{
	return partition_sector_num;
}

 int nand_get_next_file_start_sector(int cur_sector)
{
	int next_sector;
	int sequence_next;
	next_sector = cur_sector + NAND_RECORD_FILE_SECTOR_SIZE;
	if( next_sector >= partition_sector_num ){
		dbg("meet the disk end at sector: %x\n", cur_sector);
		return -1;
	}
	return next_sector;
	sequence_next = nand_get_sequence( next_sector );
	if( sequence_next != -1 )
		return next_sector;
	//dbg("next sector is bad, try next next sector\n");
	for(next_sector +=NAND_RECORD_FILE_SECTOR_SIZE;sequence_next==-1&&next_sector<partition_sector_num;next_sector +=NAND_RECORD_FILE_SECTOR_SIZE)
	{
		//printf("next_sector = %d , partition_sector_num = %d\n",next_sector , partition_sector_num);
		sequence_next = nand_get_sequence(next_sector);
	}
	if(next_sector == partition_sector_num)
		return -1;
	return next_sector;
	/*
	next_sector = next_sector + NAND_RECORD_FILE_SECTOR_SIZE;
	if( next_sector >= partition_sector_num ){
		dbg("meet the disk end\n");
		return -1;
	}
	sequence_next = nand_get_sequence( next_sector );
	if( sequence_next != -1 )
		return next_sector;
	dbg("meet the recorded disk end at %d\n", cur_sector);
	return -1;
	*/
}

int check_nand_file(int file_start_sector)
{
	unsigned char buf[512];
	struct nand_write_request req;
	unsigned int flag;
	req.start = file_start_sector+ NAND_RECORD_FILE_SECTOR_SIZE;
	if( req.start >= partition_sector_num ){
		req.start = partition_sector_num;
	}
	req.start -= INDEX_TABLE_LOCATION*512/512;
	req.buf = buf;
	req.sector_num = 1;
	if(read_file_segment(&req)<0){
		dbg("############error check index table flag###########\n");
		return -1;
	}
	memcpy(&flag , buf , sizeof(flag));
	if(flag == 0xfffffffe){
		dbg("##########file deleted###########\n");
		return -1;
	}
	return 0;
}

char* nand_get_file_time(int file_start_sector)
{
	nand_record_file_header header, end;
	unsigned int data;
	int sequence_head, sequence_end;
	int last_sector;
	int header_is_valid, end_is_valid;
	unsigned char buf[1024];
	struct nand_write_request req;
	int header_package_size;
	unsigned int flag;

/*
	//curr file we also need to send !!
	if(header_sector == file_start_sector){
		dbg("###########curr file we will not send #############\n");
		return (char*)0xffffffff;
	}
*/
	req.start = file_start_sector+ NAND_RECORD_FILE_SECTOR_SIZE;
	if( req.start > partition_sector_num ){
		req.start = partition_sector_num;
	}
	req.start -= END_HEADER_LOCATION*512/512;
	req.buf = buf;
	req.sector_num = 2;
	if(read_file_segment(&req)<0){
		dbg("############error check index table flag###########\n");
		return (char *)0xffffffff;
	}
	memcpy(&flag , buf +512, sizeof(flag));
	if(flag == 0xfffffffe){
		dbg("##########file deleted###########\n");
		return (char *)0xffffffff;
	}
	memcpy(&end, buf, sizeof( header ));

#if 1
	req.buf = buf;
	req.start = file_start_sector;
	req.sector_num = 1;
	//ioctl(fd, BLK_NAND_READ_DATA, &req);
	if(read_file_segment(&req)<0){
		return (char *)0xffffffff;
	}
	memcpy(&header, req.buf, sizeof( header ));
#else
	lseek64(fd, (int64_t)file_start_sector*(int64_t)512, SEEK_SET);
	read( fd, (char*)&header, sizeof(header));
#endif
	
	memset(file_time_buffer,0,64);
/*
	last_sector = file_start_sector + NAND_RECORD_FILE_SECTOR_SIZE;
	if( last_sector >= partition_sector_num ){
		last_sector = partition_sector_num;
	}
	last_sector -= END_HEADER_LOCATION*512/512;
*/
#if 1
/*
	req.buf = buf;
	req.start = last_sector;
	req.sector_num = 1;
	//ioctl(fd, BLK_NAND_READ_DATA, &req);
	if(read_file_segment(&req)<0){
		return (char *)0xffffffff;
	}
	memcpy(&end, req.buf, sizeof( header ));
	*/
#else
	lseek64(fd, (int64_t)last_sector*(int64_t)512, SEEK_SET);
	read( fd, (char*)&end, sizeof(end));
#endif
	if( header.head[0]!=0 || header.head[1]!=0 || header.head[2]!=0 || header.head[3]!=1 || header.head[4] != 0xc ){
		memcpy(&data, header.head, 4);
		//dbg("-----------------can't find sequence at START sector:%d, head=%x\n", file_start_sector,data);
		header_is_valid = 0;
//		return 0;
	}
	else{
		header_is_valid = 1;
	}
	if( end.head[0]!=0 || end.head[1]!=0 || end.head[2]!=0 || end.head[3]!=1 || end.head[4] != 0xc ){
		memcpy(&data, end.head, 4);
		//dbg("-----------------can't find sequence at END sector:%d, head=%x\n", last_sector,data);
		end_is_valid = 0;
	}
	else{
		end_is_valid = 1;
	}
	sequence_head = hex_string_to_int(header.PackageSequenceNumber, sizeof(header.PackageSequenceNumber));
	sequence_end = hex_string_to_int(end.PackageSequenceNumber, sizeof(end.PackageSequenceNumber));
	header_package_size = hex_string_to_int(header.TotalPackageSize, sizeof(header.TotalPackageSize));
	//dbg("sequence_head= %d,sequence_end = %d\n", sequence_head, sequence_end);
	//dbg("header_is_valid = %d , end_is_valid = %d\n",header_is_valid , end_is_valid);
	if(header_is_valid&&sequence_end == sequence_head && end_is_valid ){
		// so good to found the exact one
//		dbg("%d,%d\n", sequence_head, sequence_end);
		sprintf(file_time_buffer, "%08x", file_start_sector );
		file_time_buffer[8]=':';
		if( header_package_size == 1 ){
			return (char*)0xffffffff;
			//memcpy(&file_time_buffer[8+1], &header.TotalPackageSize, 8);
		}
		else{
			memcpy(&file_time_buffer[8+1], end.TotalPackageSize, 8);
		}
		file_time_buffer[8+1+8]=':';
		memcpy(&file_time_buffer[8+1+8+1], header.StartTimeStamp, 14);
		file_time_buffer[8+1+8+1+14] = '-';
		memcpy(&file_time_buffer[8+1+8+1+14+1], end.LastTimeStamp, 14);
		//dbg("a good file: sector=%d, time=%s\n", file_start_sector, file_time_buffer);
		return file_time_buffer;
	}
	if( header_is_valid ){
		sprintf(file_time_buffer, "%08x", file_start_sector );
		file_time_buffer[8]=':';
		if( header_package_size == 1 ){
			return (char*)0xffffffff;
			//memcpy(&file_time_buffer[8+1], &header.TotalPackageSize, 8);
		}
		else{
			memcpy(&file_time_buffer[8+1], "        ", 8);
		}
		file_time_buffer[8+1+8]=':';
		memcpy(&file_time_buffer[8+1+8+1], header.StartTimeStamp, 14);
		file_time_buffer[8+1+8+1+14] = '-';
		memcpy(&file_time_buffer[8+1+8+1+14+1], "              ", 14);
		//dbg("only head is good: sector=%d, time=%s\n", file_start_sector, file_time_buffer);
		return file_time_buffer;
	}
	return 0;
}

int nand_clean()
{
	int max_file_num;
	int start_sector;
	int next_sector;
	struct nand_write_request req;
	unsigned char* buf;

	buf = malloc(512);
	memset(buf, 0, 512);
	req.buf = buf;
	req.sector_num = 512/512;
	req.erase = 1;
	req.start = -1;
	dbg("start clean\n");
	nand_open_simple("/dev/nand-user");
	max_file_num = nand_get_max_file_num();
	dbg("max file num=%d\n", max_file_num);
	start_sector = 0;
	do{
		next_sector = nand_get_next_file_start_sector(start_sector);
		if( next_sector == -1 ){
			req.start = start_sector;
			//ioctl(fd, BLK_NAND_WRITE_DATA, &req);
			write_file_segment(&req);
			dbg("disk end\n");
			break;
		}
		req.start = start_sector;
		//ioctl(fd, BLK_NAND_WRITE_DATA, &req);
		write_file_segment(&req);
		start_sector = next_sector;
	}while(1);
	dbg("clean end\n");
	nand_close_simple();
	free(buf);
	return 0;
}

int nand_invalid_file(int file_start_sector, int file_end_sector)
{
	struct nand_write_request req;
	unsigned char* buf;
	nand_record_file_header* header;
	char tmp[20];
	int hs;
	int ret;
	unsigned int flag;
	ret = 0;
	hs = header_sector;
	if(file_start_sector <=file_end_sector){
		if(file_start_sector<=hs&&file_end_sector>=hs)
			ret = 1;
	}else{
		if(hs<=file_end_sector||hs>=file_start_sector)
			ret = 1;
	}

	if(file_start_sector <0)
		file_start_sector = 0;
	if(file_end_sector <0)
		file_end_sector = partition_sector_num -NAND_RECORD_FILE_SECTOR_SIZE ;

	dbg("invalid file from %x to %x\n", file_start_sector, file_end_sector);
	nand_open_simple("/dev/nand-user");
	memset(tmp,0,20);
	sprintf( tmp,"%08x", 1);
	buf = malloc(512);
	memset(buf, 0, 512);
	flag = 0xfffffffe;
	memcpy(buf , &flag , sizeof(flag));
	
	req.buf = buf;
	req.sector_num = 512/512;
	req.erase = 1;
	header = (nand_record_file_header*)req.buf;

	if( file_start_sector == file_end_sector ){
			req.start = file_start_sector+ NAND_RECORD_FILE_SECTOR_SIZE;
			if( req.start >= partition_sector_num ){
				req.start = partition_sector_num;
			}
			req.start -= INDEX_TABLE_LOCATION*512/512;
			if(file_start_sector != header_sector)
				write_file_segment(&req);
	}
	else if(file_start_sector < file_end_sector){
		if( file_end_sector > partition_sector_num ){
			dbg("end file sector error\n");
			file_end_sector = partition_sector_num;
		}
		while(file_start_sector <= file_end_sector){
			req.start = file_start_sector+ NAND_RECORD_FILE_SECTOR_SIZE;
			if( req.start >= partition_sector_num ){
				req.start = partition_sector_num;
			}
			req.start -= INDEX_TABLE_LOCATION*512/512;
			
			if(file_start_sector != header_sector)
				write_file_segment(&req);
			file_start_sector += NAND_RECORD_FILE_SECTOR_SIZE;
		}
	}
	else{
		while( file_start_sector <= partition_sector_num ){
			req.start = file_start_sector+ NAND_RECORD_FILE_SECTOR_SIZE;
			if( req.start >= partition_sector_num ){
				req.start = partition_sector_num;
			}
			req.start -= INDEX_TABLE_LOCATION*512/512;
			
			if(file_start_sector != header_sector)
				write_file_segment(&req);
			file_start_sector += NAND_RECORD_FILE_SECTOR_SIZE;
		}
		file_start_sector = 0;
		while(file_start_sector <= file_end_sector){
			req.start = file_start_sector+ NAND_RECORD_FILE_SECTOR_SIZE;
			if( req.start >= partition_sector_num ){
				req.start = partition_sector_num;
			}
			req.start -= INDEX_TABLE_LOCATION*512/512;
			if(file_start_sector != header_sector)
				write_file_segment(&req);
			file_start_sector += NAND_RECORD_FILE_SECTOR_SIZE;
		}
	}
	nand_close_simple();
	free(buf);

	return ret;
}

int nand_prepare_record_header(nand_record_file_header* header)
{
	char buffer[20];
	time_t timep;
	struct tm * gtm;
	int value;
	
	memset((char*)header, 0xff, sizeof(nand_record_file_header));

	header->head[0] = header->head[1] = header->head[2] = 0;  header->head[3] = 0x1; header->head[4] = 0xc;
	
	time (&timep);
	gtm = localtime(&timep);
//	dbg("year:%d,month:%d,day:%d,hour:%d,minute:%d,second:%d\n", gtm->tm_year+1900,gtm->tm_mon+1,gtm->tm_mday,gtm->tm_hour,gtm->tm_min,gtm->tm_sec);
	sprintf(buffer,"%04d%02d%02d%02d%02d%02d",gtm->tm_year+1900,gtm->tm_mon+1,gtm->tm_mday,gtm->tm_hour,gtm->tm_min,gtm->tm_sec);
	memcpy((char*)header->StartTimeStamp, buffer, sizeof(header->StartTimeStamp));

	value = 25;
	sprintf(buffer,"%8x", value);
	memcpy((char*)header->FrameRateUs, buffer, sizeof(header->FrameRateUs));
	
	sprintf(buffer,"%4x", 640);
	memcpy((char*)header->FrameWidth, buffer, sizeof(header->FrameWidth));
	
	sprintf(buffer,"%4x", 480);
	memcpy((char*)header->FrameHeight, buffer, sizeof(header->FrameHeight));
	return 0;
}

int nand_prepare_close_record_header(nand_record_file_header* header)
{
	char buffer[20];
	time_t timep;
	struct tm * gtm;

	memset(buffer,0,20);
	sprintf( buffer,"%08x", record_file_size);
	dbg("record_file_size=%d\n", record_file_size);
	memcpy( header->TotalPackageSize, buffer, sizeof(header->TotalPackageSize));
	record_file_size = 0;
//	dbg("file size=%s\n", buffer);
	
	time (&timep);
	gtm = localtime(&timep);
//	dbg("year:%d,month:%d,day:%d,hour:%d,minute:%d,second:%d\n", gtm->tm_year+1900,gtm->tm_mon+1,gtm->tm_mday,gtm->tm_hour,gtm->tm_min,gtm->tm_sec);
	sprintf(buffer,"%04d%02d%02d%02d%02d%02d",gtm->tm_year+1900,gtm->tm_mon+1,gtm->tm_mday,gtm->tm_hour,gtm->tm_min,gtm->tm_sec);
	memcpy((char*)header->LastTimeStamp, buffer, sizeof(header->LastTimeStamp));	
#if 1
//	end_time = get_us();
//	dbg("total time of packet = %d minutes %d senconds\n", ( end_time - start_time )/1000000/60, ( end_time - start_time )/1000000%60);
#endif
	return 0;
}

