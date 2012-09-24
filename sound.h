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

#define L_PCM_8K			160
#define L_PCM_16K			320
#define L_PCM_USE			L_PCM_8K

#define DEFAULT_FORMAT		 SND_PCM_FORMAT_S16_LE  
#define DEFAULT_SPEED 		 8000 
#define DEFAULT_CHANNELS	 2	
#define CHAUNK_BYTES_MAX	 (4096*100)  
#define PERIODS_SIZE 		160
#define PERIODS_PER_BUFFSIZE  128
#define PCM_NAME			 "plughw:0,0"  
#define AMR_MODE			7 

#define ECHO_BUFFER_NUMS     128

/*
* buffer to store sound for syn video and sound record 
* also send sound thread will get sound from this buffer
*/
#define NUM_BUFFERS           	128
#define  SIZE_OF_AMR_PER_PERIOD  32
#define  PERIOD_TO_WRITE_EACH_TIME	13
/*before we send data we store sound data in 3 sec at first*/
#define BOOT_SOUND_STORE_SIZE 8000
typedef struct __sound_buf
{
	char sess_clean_mask[MAX_NUM_IDS];
	char *buf;
}sound_buf_t;
struct __syn_sound_buf
{
	pthread_rwlock_t	syn_buf_lock;
	sound_buf_t c_sound_array[NUM_BUFFERS];
	int start; /*where start to read >=0 <NUM_BUFERS*/
	int end;/*where end to read,also where to write  >=0 <NUM_BUFERS*/
	int buffsize; /*NUM_BUFFERS * SIZE_OF_AMR_PER_PERIOD*/
	char *cache; /*for the grab sound thread to use*/
	char *absolute_start_addr; /*the absolute start address of buffer  */
};

/*
*buffer to store sound from pc to write to the sound card
*we have a thread to get sound data to write sound to sound card
*recv sound thread only need to write sound to this buffer
*/
typedef struct __play_sound_unit_buf
{
	int datalen;
	int maxlen;
	char *buf;
}play_sound_unit_buf_t;
struct play_sound_buf
{
	char *absolute_start_addr;
	play_sound_unit_buf_t  p_sound_array[MAX_NUM_IDS];
	int curr_play_pos;
	char *cache;
	int cache_data_len;
	int cache_play_pos;
};

/*
*buffer to store sound that we have writed to sound card
*for echo cacel
*/
typedef struct __echo_buf
{
	char *absolute_start_addr;
	char *echo_buf[ECHO_BUFFER_NUMS];
	char *mic_buf[ECHO_BUFFER_NUMS];
	int echo_wrpos;
	int mic_wrpos;
	int echo_cancel_pos;
	int mic_cancel_pos;
}echo_buf_t;

int init_and_start_sound();
char *new_get_sound_data(int sess_id, int * size);
void reset_syn_buf();
void set_syn_sound_data_clean(int sess_id);

int put_play_sound_data(int sess_id , char *buf , int len);
char *get_play_sound_data(int * len);

//int grab_sound_thread();
int start_audio_monitor(struct sess_ctx*sess);

void *read_sound_data(void *arg);
void *write_sound_data(void *arg);
void *handle_echo_buf_thead(void *arg);
int start_sound_thread();
#ifdef __cplusplus
}
#endif
#endif

