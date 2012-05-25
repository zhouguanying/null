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
#define  UNIT_SIZE_OF_AMR_TO_WRITE 128
#define STEP  128  /*the unit size write to buf each time*/
struct __syn_sound_buf{
	pthread_mutex_t syn_buf_lock;
	int start; /*where start to read*/
	int end;/*where end to read,also where to write*/
	int buffsize; /*it should round to step size*/
	char *buf;
};
int init_and_start_sound();
char* get_cop_sound_data(ssize_t *size);
int grab_sound_data();
int test_sound_tcp_transport(struct sess_ctx * sess);
int play_cop_sound_data(char *buffer,ssize_t length);
int grab_sound_thread();
#ifdef __cplusplus
}
#endif
#endif
