#ifndef __SOUND__H__
#define __SOUND__H__
#ifdef __cplusplus
extern "C"
{
#endif
//#include<pthread.h>
/*
*this struct is used to synchronize 
*the video and audio record
*/
#include "cli.h"
#define NUM_BUFFERS           	100
#define  SIZE_OF_AMR_PER_PERIOD  32
#define  PERIOD_TO_WRITE_EACH_TIME	1

/*before we send data we store sound data in 3 sec at first*/
#define BOOT_SOUND_STORE_SIZE 8000

typedef struct __sound_buf{
	char sess_clean_mask[MAX_NUM_IDS];
	char *buf;
}sound_buf_t;
struct __syn_sound_buf{
	pthread_rwlock_t	syn_buf_lock;
	sound_buf_t c_sound_array[NUM_BUFFERS];
	int start; /*where start to read >=0 <NUM_BUFERS*/
	int end;/*where end to read,also where to write  >=0 <NUM_BUFERS*/
	int buffsize; /*NUM_BUFFERS * SIZE_OF_AMR_PER_PERIOD*/
	char *cache; /*for the grab sound thread to use*/
	char *absolute_start_addr; /*the absolute start address of buffer  */
};

typedef struct __play_sound_unit_buf{
	int datalen;
	int maxlen;
	char *buf;
}play_sound_unit_buf_t;

struct play_sound_buf{
	char *absolute_start_addr;
	play_sound_unit_buf_t  p_sound_array[MAX_NUM_IDS];
	int curr_play_pos;
	char *cache;
	int cache_data_len;
	int cache_play_pos;
};

int init_and_start_sound();
char *new_get_sound_data(int sess_id, int * size);
void reset_syn_buf();
void set_syn_sound_data_clean(int sess_id);

int put_play_sound_data(int sess_id , char *buf , int len);
char *get_play_sound_data(int * len);

int grab_sound_thread();
int start_audio_monitor(struct sess_ctx*sess);
#ifdef __cplusplus
}
#endif
#endif
