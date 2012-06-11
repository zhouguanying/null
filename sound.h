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
#define NUM_BUFFERS           	10
#define  SIZE_OF_AMR_PER_PERIOD  416
#define STEP  128  /*the unit size write to buf each time*/
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
int init_and_start_sound();

char* get_cop_sound_data(ssize_t *size);
int grab_sound_data();

char *new_get_sound_data(int sess_id, int * size);
void reset_syn_buf();
void set_syn_sound_data_clean(int sess_id);

int test_sound_tcp_transport(struct sess_ctx * sess);
int play_cop_sound_data(char *buffer,ssize_t length);
int grab_sound_thread();
#ifdef __cplusplus
}
#endif
#endif
