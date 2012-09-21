#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <assert.h>
#include <termios.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <endian.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>


#include "types.h"
#include "sockets.h"
#include "utilities.h"
#include "amr.h"
#include "cli.h"
#include "sound.h"
#include "amrnb_encode.h"

#include "udttools.h"

#include "speex_echo.h"
#include "speex_preprocess.h"

#define	timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)

#define	timermsub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
	if ((result)->tv_nsec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_nsec += 1000000000L; \
	} \
} while (0)

#if 1
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt, __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif

//hw parameters
static snd_pcm_format_t   format;
static unsigned int 		  channels;
static unsigned int 		  rate;
static snd_pcm_uframes_t chunk_size;
static int 			         chunk_bytes;
static snd_pcm_uframes_t period_frames;
static snd_pcm_uframes_t buffer_frames;
static int 			  bits_per_sample;
static int    		  bits_per_frame ;
static int 			  resample = 1;
static int 			  can_pause;
static int 			  monotonic;


static snd_pcm_t     *capture_handle;
static snd_pcm_t     *playback_handle;
static char *		capture_buffer;
static char *            playback_buffer;
static char *		 delay_buffer;
static pthread_t      capture_tid , playback_tid;



static struct __syn_sound_buf syn_buf;
static struct play_sound_buf  p_sound_buf;
static echo_buf_t  echo_buf;


static SpeexEchoState *st = NULL;
static SpeexPreprocessState *den = NULL;
static pthread_mutex_t  st_lock;
static short *ref_buf;
static short *mic_buf;
//static short *out_buf;
static pthread_mutex_t drop_pcm_lock;
/*
const unsigned char amr_f_head[8]={0x04,0x0c,0x14,0x1c,0x24,0x2c,0x34,0x3c};
const unsigned char amr_f_size[]={13,14,16,18,20,21,27,32,6, 1, 1, 1, 1, 1, 1, 1};
*/
/*
int first_write = 1;
int first_aec = 1;
struct timeval writetime , aectime;
*/

#define NN  80
#define TAIL NN*4
static inline void init_speex_echo()
{
	int sampleRate = DEFAULT_SPEED;
	if(st||den){
		printf("warning: try init speex echo but already init\n");
		return;
	}
	pthread_mutex_lock(&st_lock);
	//st = speex_echo_state_init(chunk_size, chunk_size*10);
	st = speex_echo_state_init_mc(NN , TAIL, 1 ,1);
       den = speex_preprocess_state_init(NN, sampleRate);
       speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
       speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);
	   pthread_mutex_unlock(&st_lock);
}
static inline void destroy_speex_echo()
{
	if(!st||!den)
		return;
	pthread_mutex_lock(&st_lock );
	speex_echo_state_destroy( st);
	speex_preprocess_state_destroy(den);
	st = NULL;
	den = NULL;
	pthread_mutex_unlock(&st_lock);
}
void reset_speex_echo()
{
	speex_echo_state_reset(st);
}
static inline void do_speex_echo(short *mic_buf,short *ref_buf , short *out_buf , int frames)
{
	int i;
	pthread_mutex_lock(&st_lock);
	if(!st||!den)
	{
		memcpy(out_buf , mic_buf , frames*2);
		printf("warning:aec state destroy\n");
		pthread_mutex_unlock(&st_lock);
		return;
	}
	for(i = 0 ; i< frames; i+=NN)
	{
		speex_echo_cancellation(st,mic_buf +i , ref_buf+i,out_buf+i);
		speex_preprocess_run(den, out_buf+i);
	}
	pthread_mutex_unlock(&st_lock);
}

int init_syn_buffer()
{
	int i;
	 syn_buf.buffsize = NUM_BUFFERS * SIZE_OF_AMR_PER_PERIOD;
	syn_buf.absolute_start_addr = (char *)malloc(syn_buf.buffsize);
	if(!syn_buf.absolute_start_addr){
		printf("error malloc syn_sound_buffer\n");
		return -1;
	}
	syn_buf.cache = (char *)malloc(SIZE_OF_AMR_PER_PERIOD);
	if(!syn_buf.cache){
		printf("malloc syn_buf cache error\n");
		free(syn_buf.absolute_start_addr);
		return -1;
	}
	for(i=0;i<NUM_BUFFERS;i++){
		syn_buf.c_sound_array[i].buf = syn_buf.absolute_start_addr+i*SIZE_OF_AMR_PER_PERIOD;
		memset(syn_buf.c_sound_array[i].sess_clean_mask,1,sizeof(syn_buf.c_sound_array[i].sess_clean_mask));
	}
	syn_buf.start = 0;
	syn_buf.end = 0;
	pthread_rwlock_init(&syn_buf.syn_buf_lock , NULL);
	return 0;
}
 void reset_syn_buf()
{
	pthread_rwlock_wrlock(&syn_buf.syn_buf_lock);
	syn_buf.start = syn_buf.end = 0;
	pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
}
void set_syn_sound_data_clean(int sess_id)
{
	int us_pos;
	pthread_rwlock_rdlock(&syn_buf.syn_buf_lock);
	us_pos = syn_buf.start;
	while(us_pos!=syn_buf.end){
		syn_buf.c_sound_array[us_pos].sess_clean_mask[sess_id] = 1;
		us_pos = (us_pos +1)%NUM_BUFFERS;
	}
	pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
}
char *new_get_sound_data(int sess_id , int *size)
{
	int us_pos;
	char *buf;
	pthread_rwlock_rdlock(&syn_buf.syn_buf_lock);
	us_pos = syn_buf.start;
	while(us_pos!=syn_buf.end){
		if(!syn_buf.c_sound_array[us_pos].sess_clean_mask[sess_id]){
			break;
		}
		us_pos= (us_pos+1)%NUM_BUFFERS;
	}
	if(us_pos == syn_buf.end){
		pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
		return NULL;
	}
	if(syn_buf.end>us_pos){
		*size = syn_buf.c_sound_array[syn_buf.end].buf - syn_buf.c_sound_array[us_pos].buf;
		buf = (char *)malloc(*size);
		if(!buf){
			printf("malloc sound buffer fail\n");
			pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
			return NULL;
		}
		memcpy(buf,syn_buf.c_sound_array[us_pos].buf,*size);
	}else{
		*size = syn_buf.buffsize -(syn_buf.c_sound_array[us_pos].buf - syn_buf.absolute_start_addr);
		buf = (char *)malloc(*size +(syn_buf.c_sound_array[syn_buf.end].buf - syn_buf.absolute_start_addr));
		if(!buf){
			printf("malloc sound buffer fail\n");
			pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
			return NULL;
		}
		memcpy(buf , syn_buf.c_sound_array[us_pos].buf,*size);
		memcpy(buf +*size,syn_buf.absolute_start_addr,syn_buf.c_sound_array[syn_buf.end].buf - syn_buf.absolute_start_addr);
		(*size)+=(syn_buf.c_sound_array[syn_buf.end].buf - syn_buf.absolute_start_addr);
	}
	while(us_pos!=syn_buf.end){
		if(syn_buf.c_sound_array[us_pos].sess_clean_mask[sess_id]){
			printf("*************BUG we have read the newer one?************************\n");
		}
		syn_buf.c_sound_array[us_pos].sess_clean_mask[sess_id] =1;
		us_pos = (us_pos +1)%NUM_BUFFERS;
	}
	pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
	return buf;
}


int init_playback_buffer()
{
	int i;
	memset(&p_sound_buf , 0 ,sizeof(p_sound_buf));
	p_sound_buf.absolute_start_addr = (char *)malloc(SIZE_OF_AMR_PER_PERIOD*PERIOD_TO_WRITE_EACH_TIME*MAX_NUM_IDS);
	if(!p_sound_buf.absolute_start_addr)
	{
		printf("cannot malloc buffer for playback sound\n");
		return -1;
	}
	p_sound_buf.cache = (char *)malloc(SIZE_OF_AMR_PER_PERIOD*PERIOD_TO_WRITE_EACH_TIME*MAX_NUM_IDS);
	if(!p_sound_buf.cache)
	{
		printf("error malloc buffer for playback sound cache\n");
		return -1;
	}
	for(i = 0 ; i < MAX_NUM_IDS ; i++){
		p_sound_buf.p_sound_array[i].buf = p_sound_buf.absolute_start_addr + i *SIZE_OF_AMR_PER_PERIOD*PERIOD_TO_WRITE_EACH_TIME;
		p_sound_buf.p_sound_array[i].maxlen = SIZE_OF_AMR_PER_PERIOD*PERIOD_TO_WRITE_EACH_TIME;
		p_sound_buf.p_sound_array[i].datalen = 0;
	}
	p_sound_buf.curr_play_pos = 0;
	p_sound_buf.cache_data_len = 0;
	p_sound_buf.cache_play_pos = 0;
	return 0;
}
int put_play_sound_data(int sess_id , char *buf , int len)
{
	while(p_sound_buf.p_sound_array[sess_id].datalen>=p_sound_buf.p_sound_array[sess_id].maxlen)
		usleep(1000);
	if(len +p_sound_buf.p_sound_array[sess_id].datalen> p_sound_buf.p_sound_array[sess_id].maxlen)
		len = p_sound_buf.p_sound_array[sess_id].maxlen -p_sound_buf.p_sound_array[sess_id].datalen ;
	memcpy(p_sound_buf.p_sound_array[sess_id].buf +p_sound_buf.p_sound_array[sess_id].datalen , buf , len);
	p_sound_buf.p_sound_array[sess_id].datalen += len;
	return len;
}

int clear_play_sound_data(int sess_id)
{
	while(p_sound_buf.p_sound_array[sess_id].datalen>=p_sound_buf.p_sound_array[sess_id].maxlen)
		usleep(1000);
	if(p_sound_buf.p_sound_array[sess_id].datalen)
		p_sound_buf.p_sound_array[sess_id].datalen = 0;
	return 0;
}

char *get_play_sound_data(int *len)
{
	int i;
	i = p_sound_buf.curr_play_pos;
	p_sound_buf.cache_data_len = 0;
	p_sound_buf.cache_play_pos = 0;
	if(p_sound_buf.p_sound_array[i].datalen>=p_sound_buf.p_sound_array[i].maxlen)
	{
		memcpy(p_sound_buf.cache +p_sound_buf.cache_data_len, p_sound_buf.p_sound_array[i].buf , p_sound_buf.p_sound_array[i].datalen);
		p_sound_buf.cache_data_len += p_sound_buf.p_sound_array[i].datalen;
		p_sound_buf.p_sound_array[i].datalen = 0;
	}
	for( i = (i+1)%MAX_NUM_IDS; i !=p_sound_buf.curr_play_pos; i =( i+1)%MAX_NUM_IDS)
	{
		if(p_sound_buf.p_sound_array[i].datalen>=p_sound_buf.p_sound_array[i].maxlen)
		{
			memcpy(p_sound_buf.cache +p_sound_buf.cache_data_len, p_sound_buf.p_sound_array[i].buf , p_sound_buf.p_sound_array[i].datalen);
			p_sound_buf.cache_data_len += p_sound_buf.p_sound_array[i].datalen;
			p_sound_buf.p_sound_array[i].datalen = 0;
		}
	}
	if(p_sound_buf.cache_data_len)
	{
		p_sound_buf.curr_play_pos = (p_sound_buf.curr_play_pos + 1)%MAX_NUM_IDS;
		*len = p_sound_buf.cache_data_len;
		return p_sound_buf.cache;
	}
	return NULL;
}

int p_sound_buf_have_data()
{
	int i;
	for(i = 0; i<MAX_NUM_IDS; i++)
	{
		if(p_sound_buf.p_sound_array[i].datalen>=p_sound_buf.p_sound_array[i].maxlen)
			return 1;
	}
	return 0;
}


int init_echo_buffer()
{
	int i;
	char *echo_start_addr;
	char *mic_start_addr;
	memset(&echo_buf , 0 , sizeof(echo_buf));
	echo_buf.absolute_start_addr = (char *)malloc(chunk_bytes *2 *ECHO_BUFFER_NUMS);
	if(!echo_buf.absolute_start_addr)
	{
		printf("error malloc buffer for echo buffer\n");
		return -1;
	}
	echo_start_addr = echo_buf.absolute_start_addr;
	mic_start_addr = echo_buf.absolute_start_addr + chunk_bytes *ECHO_BUFFER_NUMS;
	for(i = 0 ; i < ECHO_BUFFER_NUMS ; i++)
	{
		echo_buf.echo_buf[i] = echo_start_addr + i *chunk_bytes;
		echo_buf.mic_buf[i] = mic_start_addr + i*chunk_bytes;
	}
	return 0;
}
 void put_echo_data(char *buf)
{
	int new_pos = echo_buf.echo_wrpos ;
	memcpy(echo_buf.echo_buf[echo_buf.echo_wrpos] , buf , chunk_bytes);
	new_pos ++;
	if(new_pos>=ECHO_BUFFER_NUMS)
		new_pos= 0;
	while(new_pos == echo_buf.echo_cancel_pos)
		usleep(10000);
	echo_buf.echo_wrpos = new_pos;
}
void put_mic_data(char *buf)
{
	int new_pos = echo_buf.mic_wrpos;
	memcpy(echo_buf.mic_buf[echo_buf.mic_wrpos] , buf , chunk_bytes);
	 new_pos ++;
	if( new_pos>=ECHO_BUFFER_NUMS)
		 new_pos = 0;
	while( new_pos == echo_buf.mic_cancel_pos)
		usleep(1000);
	echo_buf.mic_wrpos = new_pos;
}

#define IOCTL_GET_SPK_CTL   _IOR('x',0x01,int)
#define IOCTL_SET_SPK_CTL   _IOW('x',0x02,int)

static inline int turn_on_speaker()
{
	 int fd; 
	 fd = open ("/dev/mxs-gpio", O_RDWR);
	 if (fd<0)
	  {   
	       dbg ("file open error \n");
	        return -1; 
	  }
	 if(ioctl(fd, IOCTL_GET_SPK_CTL, 0)==0)
		  ioctl(fd, IOCTL_SET_SPK_CTL, 1);     
	 // dbg("speaker is %s\n",ioctl(fd, IOCTL_GET_SPK_CTL, 0)?"on":"off");
	  close (fd);

	   return 0;

}
static inline int turn_off_speaker()
{
	int fd; 
	 fd = open ("/dev/mxs-gpio", O_RDWR);
	 if (fd<0)
	  {   
	       dbg ("file open error \n");
	        return -1; 
	  }   
	 if(ioctl(fd, IOCTL_GET_SPK_CTL, 0)>0)
		  ioctl(fd, IOCTL_SET_SPK_CTL, 0);   
	  //dbg("speaker is %s\n",ioctl(fd, IOCTL_GET_SPK_CTL, 0)?"on":"off");
	  close (fd);
	   return 0;
}

int open_snd()
{
	int err;
	if ((err = snd_pcm_open(&playback_handle,PCM_NAME, SND_PCM_STREAM_PLAYBACK,0)) < 0) 
	{
                 printf("Playback open error: %s\n", snd_strerror(err));
                 return -1;
  	 }
    	if ((err = snd_pcm_open(&capture_handle, PCM_NAME, SND_PCM_STREAM_CAPTURE,SND_PCM_NONBLOCK)) < 0) 
	{
                 printf("Record open error: %s\n", snd_strerror(err));
                 return -1;
   	 }
	return 0;
}

static inline void TwoCHtoOne(short *s , short *d , int frames)
{
	int i , r;
	if(channels == 1)
	{
		memcpy(d ,s,frames*2);
		return;
	}
	for(i = 0 , r= 0;i<frames;i ++ , r+=2)
	{
		d[i] = s[r];
	}
}

static inline void OneCHtoTwo(short *s , short *d , int frames)
{
	int i , r;
	for(i = 0,r=0 ; i<frames;i++ , r+=2)
	{
		d[r] = s[i];
		d[r+1] = s[i];
	}
}

static inline int  is_speaker_on()
{
	int fd; 
	int ret;
	 fd = open ("/dev/mxs-gpio", O_RDWR);
	 if (fd<0)
	  {   
	       dbg ("file open error \n");
	        return -1; 
	  }   
	    
	 ret =  ioctl(fd, IOCTL_GET_SPK_CTL, 0);
	  close (fd);
	  return ret;
}

static int setparams_stream(snd_pcm_t *handle,
						  snd_pcm_hw_params_t *params,
						  const char *id)
 {
		 int err;
		 unsigned int rrate;

		format = DEFAULT_FORMAT;
		rate = DEFAULT_SPEED;
		channels = DEFAULT_CHANNELS;
	
		 err = snd_pcm_hw_params_any(handle, params);
		 if (err < 0) {
				 printf("Broken configuration for %s PCM: no configurations available: %s\n", snd_strerror(err), id);
				 return err;
		 }
		 err = snd_pcm_hw_params_set_rate_resample(handle, params,resample);
		 if (err < 0) {
				 printf("Resample setup failed for %s (val %i): %s\n", id, resample, snd_strerror(err));
				 return err;
		 }
		 err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
		 if (err < 0) {
				 printf("Access type not available for %s: %s\n", id, snd_strerror(err));
				 return err;
		 }
		 err = snd_pcm_hw_params_set_format(handle, params, DEFAULT_FORMAT);
		 if (err < 0) {
				 printf("Sample format not available for %s: %s\n", id, snd_strerror(err));
				 return err;
		 }
		 err = snd_pcm_hw_params_set_channels(handle, params, DEFAULT_CHANNELS);
		 if (err < 0) {
				 printf("Channels count (%i) not available for %s: %s\n", DEFAULT_CHANNELS, id, snd_strerror(err));
				 return err;
		 }
		 rrate =rate;
		 err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
		 if (err < 0) {
				 printf("Rate %iHz not available for %s: %s\n", rate, id, snd_strerror(err));
				 return err;
		 }
		 if ((int)rrate != rate) {
				 printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
				 return -EINVAL;
		 }
		 bits_per_sample = snd_pcm_format_physical_width(format);
		bits_per_frame =bits_per_sample* channels;
		 return 0;
 }

static int setparams_bufsize(snd_pcm_t *handle,
					  snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *tparams,
					  snd_pcm_uframes_t bufsize,
					  const char *id)
{
		int err;
		snd_pcm_uframes_t periodsize;

		snd_pcm_hw_params_copy(params, tparams);
		periodsize = bufsize * PERIODS_PER_BUFFSIZE ;
		err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
		if (err < 0) {
				printf("Unable to set buffer size %li for %s: %s\n", bufsize * PERIODS_PER_BUFFSIZE, id, snd_strerror(err));
				return err;
		}
		buffer_frames=periodsize;
		periodsize /= PERIODS_PER_BUFFSIZE;
		err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
		if (err < 0) {
				printf("Unable to set period size %li for %s: %s\n", periodsize, id, snd_strerror(err));
				return err;
		}
		period_frames=periodsize;
		chunk_size=periodsize;
		chunk_bytes =chunk_size * bits_per_frame / 8;
		return 0;
}

void check_hw_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params ,const char *id)
{
	int err;
	err = snd_pcm_hw_params_get_channels(params ,&channels);
	if(err!=0)
	{
		printf("%s: get channels error:%s\n",id,snd_strerror(err));
		exit(0);
	}
	printf("%s: channels = %d\n",id,channels);
}

static int setparams_set(snd_pcm_t *handle,
                   snd_pcm_hw_params_t *params,
                   snd_pcm_sw_params_t *swparams,
                   const char *id)
 {
         int err;
         snd_pcm_uframes_t val;
		 
	monotonic = snd_pcm_hw_params_is_monotonic(params);
	 can_pause = snd_pcm_hw_params_can_pause(params);
 
         err = snd_pcm_hw_params(handle, params);
         if (err < 0) {
                 printf("Unable to set hw params for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
	check_hw_params( handle,  params,  id);
         err = snd_pcm_sw_params_current(handle, swparams);
         if (err < 0) {
                 printf("Unable to determine current swparams for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
	  if((strncmp(id,"capture",7))==0){
	  	val=(double)(rate)*1/(double)1000000;
	  }
	  else
	  	val=buffer_frames;
	  if(val<1)
	  	val=1;
	  else if(val>buffer_frames)
	  	val=buffer_frames;
         err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 1);
         if (err < 0) {
                 printf("Unable to set start threshold mode for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
         err = snd_pcm_sw_params_set_avail_min(handle, swparams,chunk_size);
         if (err < 0) {
                 printf("Unable to set avail min for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
	 if((strncmp(id,"capture",7))==0){
	  	val=buffer_frames;
	  }
	  else
	  	val=buffer_frames<<1;
	  err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, val);
         if (err < 0) {
                 printf("Unable to set stop threshold for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
         err = snd_pcm_sw_params(handle, swparams);
         if (err < 0) {
                 printf("Unable to set sw params for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
         return 0;
 }
 

static int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize)
 {
         int err, last_bufsize = *bufsize;
         snd_pcm_hw_params_t *pt_params, *ct_params;     /* templates with rate, format and channels */
         snd_pcm_hw_params_t *p_params, *c_params;
         snd_pcm_sw_params_t *p_swparams, *c_swparams;
         snd_pcm_uframes_t  p_psize, c_psize;
         unsigned int p_time, c_time;
         //unsigned int val;
 
         snd_pcm_hw_params_alloca(&p_params);
         snd_pcm_hw_params_alloca(&c_params);
         snd_pcm_hw_params_alloca(&pt_params);
         snd_pcm_hw_params_alloca(&ct_params);
         snd_pcm_sw_params_alloca(&p_swparams);
         snd_pcm_sw_params_alloca(&c_swparams);
         if ((err = setparams_stream(phandle, pt_params, "playback")) < 0) {
                 printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
                 return err;
         }
         if ((err = setparams_stream(chandle, ct_params, "capture")) < 0) {
                 printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
                 return err;
         }
	/*
	 if((err=snd_pcm_hw_params_get_buffer_size_max(pt_params,&buffer_frames))<0){
		return err;
	 }
	*/
	 buffer_frames = PERIODS_SIZE *PERIODS_PER_BUFFSIZE ;
	 *bufsize=(int)(buffer_frames/PERIODS_PER_BUFFSIZE);
	 *bufsize=((int)(((*bufsize)+L_PCM_USE-1)/L_PCM_USE))*L_PCM_USE;//set the period size round to 160 for 8k raw sound data convert to amr (8000HZ*0.02s=160 frames)
	 *bufsize-=L_PCM_USE;
	 last_bufsize=*bufsize;
 
       __again:
         if (last_bufsize == *bufsize)
                 *bufsize += L_PCM_USE;
         last_bufsize = *bufsize;
         if (*bufsize >( CHAUNK_BYTES_MAX/(bits_per_frame/8))){
		 		 printf("chunk_size too big!\n");
                 return -1;
         }
         if ((err = setparams_bufsize(phandle, p_params, pt_params, *bufsize, "playback")) < 0) {
                 printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
                 return err;
         }
         if ((err = setparams_bufsize(chandle, c_params, ct_params, *bufsize, "capture")) < 0) {
                 printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
                 return err;
         }
 
         snd_pcm_hw_params_get_period_size(p_params, &p_psize, NULL);
         if (p_psize > (unsigned int)*bufsize)
                 *bufsize = p_psize;
         snd_pcm_hw_params_get_period_size(c_params, &c_psize, NULL);
         if (c_psize > (unsigned int)*bufsize)
                 *bufsize = c_psize;
         snd_pcm_hw_params_get_period_time(p_params, &p_time, NULL);
         snd_pcm_hw_params_get_period_time(c_params, &c_time, NULL);
         if (p_time != c_time)
                 goto __again;
		 
		 if((*bufsize)%L_PCM_USE!=0){
		       *bufsize=((int)(((*bufsize)+L_PCM_USE-1)/L_PCM_USE))*L_PCM_USE;
			last_bufsize=*bufsize;
			goto __again;
		 }

		 snd_pcm_uframes_t p_size , c_size;
		 snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
		  snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
		  dbg("############p_size = %d################\n",  p_size);
		  dbg("############c_size = %d################\n",c_size);
		  
 /*
         snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
         if (p_psize * PERIODS_PER_BUFFSIZE< p_size) {
                 snd_pcm_hw_params_get_periods_min(p_params, &val, NULL);
		   printf("minimal periods per buffer==%d\n",val);
                 if (val > PERIODS_PER_BUFFSIZE) {
                         printf("playback device does not support %d periods per buffer\n",PERIODS_PER_BUFFSIZE);
                         return -1;
                 }
                 goto __again;
         }
         snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
         if (c_psize * PERIODS_PER_BUFFSIZE < c_size) {
                 snd_pcm_hw_params_get_periods_min(c_params, &val, NULL);
		   printf("minimal periods per buffer==%d\n",val);
                 if (val > PERIODS_PER_BUFFSIZE) {
                         printf("capture device does not support %d periods per buffer\n",PERIODS_PER_BUFFSIZE);
                         return -1;
                 }
                 goto __again;
         }
   */    
	chunk_size=*bufsize; 
        chunk_bytes=(*bufsize)*bits_per_frame/8;
         if ((err = setparams_set(phandle, p_params, p_swparams, "playback")) < 0) {
                 printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
                 return err;
         }
         if ((err = setparams_set(chandle, c_params, c_swparams, "capture")) < 0) {
                 printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
                 return err;
         }
 	/*
         if ((err = snd_pcm_prepare(phandle)) < 0) {
                 printf("Prepare error: %s\n", snd_strerror(err));
				 return err;
         }
         */
         return 0;
 }


/* I/O suspend handler */
static int  xrun(snd_pcm_t*handle)
{
	snd_pcm_status_t *status;
	int res;
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status))<0) {
		printf("status error: %s\n", snd_strerror(res));
		return -1;
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		if (monotonic) {
			struct timespec now, diff, tstamp;
			clock_gettime(CLOCK_MONOTONIC, &now);
			snd_pcm_status_get_trigger_htstamp(status, &tstamp);
			timermsub(&now, &tstamp, &diff);
			fprintf(stderr, "overrun or underrun!!! (at least %.3f ms long)\n",
				diff.tv_sec * 1000 + diff.tv_nsec / 10000000.0);
		} else {
			struct timeval now, diff, tstamp;
			gettimeofday(&now, 0);
			snd_pcm_status_get_trigger_tstamp(status, &tstamp);
			timersub(&now, &tstamp, &diff);
			fprintf(stderr, "overrun or underrun!!! (at least %.3f ms long)\n",
				diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		}
		if ((res = snd_pcm_prepare(handle))<0) {
			printf("xrun: prepare error: %s\n", snd_strerror(res));
			return -1;
		}
		return 0 ;		/* ok, data should be accepted again */
	} 
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
		printf("Status(DRAINING)!\n");
		if (capture_tid==pthread_self()) {
			printf("capture stream format change? attempting recover...\n");
			if ((res = snd_pcm_prepare(handle))<0) {
				printf("xrun(DRAINING): prepare error: %s\n", snd_strerror(res));
				return -1;
			}
			return 0;
		}
	}
	printf("read/write error, state = %s\n", snd_pcm_state_name(snd_pcm_status_get_state(status)));
	return -1;
}
static inline void  reset_pcm()
{
	pthread_mutex_lock(&drop_pcm_lock);
	if(snd_pcm_drop(capture_handle )!=0||
	snd_pcm_drop(playback_handle)!=0)
	{
		printf("################drop handle error##############\n");
		exit(0);
	}
	if(snd_pcm_prepare(capture_handle)!=0||
	snd_pcm_prepare(playback_handle)!=0)
	{
		printf("################prepare handle error###############\n");
		exit(0);
	}
	pthread_mutex_unlock(&drop_pcm_lock);
	printf("====================reset pcm ok===================\n");
}
/* I/O suspend handler */
static int suspend(snd_pcm_t*handle)
{
	int res;
	while ((res = snd_pcm_resume(handle)) == -EAGAIN)
		sleep(1);	/* wait until suspend flag is released */
	if (res < 0) {
		printf("Failed. Restarting stream. \n");
		if ((res = snd_pcm_prepare(handle)) < 0) {
			printf("suspend: prepare error: %s\n", snd_strerror(res));
			return -1;
		}
	}
	printf("Done.\n");
	return 0;
}

/*
 *  write function
 * parameters:
 *
 * data   data to write
 * count  frames to write
 */

static ssize_t pcm_write(u_char *data, size_t count)
{
	ssize_t r;
	ssize_t result = 0;

	if (count <chunk_size) {
		snd_pcm_format_set_silence(format, 
			data + count *bits_per_frame / 8,
			(chunk_size - count) * channels);
		count = chunk_size;
	}
	while (count > 0) {
		r = snd_pcm_writei(playback_handle, data, count);
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			snd_pcm_wait(playback_handle, 100);
		} else if (r == -EPIPE) {
			printf("pcm_write under run\n");
			if(xrun(playback_handle)<0)
				return -1;
		} else if (r == -ESTRPIPE) {
			if(suspend(playback_handle)<0)
				return -1;
		} else if (r < 0) {
			printf("write error: %s\n", snd_strerror(r));
			return -1;
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r *(bits_per_frame / 8);
		}
	}
	return result;
}

/*
 *  read function
 *  parameters:
 *  data  buffer to store sound data
 *  rcount   frames to read
 */

static ssize_t pcm_read(u_char *data, size_t rcount)
{
	ssize_t r;
	size_t result = 0;
	size_t count = rcount;

	if (count != chunk_size) {
		printf("pcm_read rcount do not equal chunk_size\n");
		count = chunk_size;
	}
	while (count > 0) {
		r = snd_pcm_readi(capture_handle, data, count);
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			snd_pcm_wait(capture_handle, 100);
		} else if (r == -EPIPE) {
			printf("pcm_read over run\n");
			if(xrun(capture_handle)<0)
				return -1;
			//increase_video_thread_sleep_time();
		} else if (r == -ESTRPIPE) {
			if(suspend(capture_handle)<0)
				return -1;
		} else if (r < 0) {
		//for debuf
			snd_pcm_status_t *status;
			int res;
			snd_pcm_status_alloca(&status);
			printf("read error: %s\n", snd_strerror(r));
			if ((res = snd_pcm_status(capture_handle, status))<0) {
				printf("status error: %s\n", snd_strerror(res));
				return -1;
			}
			printf("state=%s\n",snd_pcm_state_name(snd_pcm_status_get_state(status)));
			return -1;
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r *(bits_per_frame / 8);
		}
	}
	return result;
}

#define TEST 0
#if TEST
int end = 0;
void *write_thread(void *arg)
{
	FILE*ref_fp;
	ref_fp = fopen("/sdcard/ref.sw","r");
	while(fread(playback_buffer, chunk_bytes, 1 , ref_fp)==1)
	{
		//OneCHtoTwo(ref_buf, playback_buffer);
		if(pcm_write(playback_buffer, chunk_size)<0)
		{
			printf("error pcm write\n");
			exit(0);
		}
	}
	fclose(ref_fp);
	end = 1;
}

void *read_thread(void *arg)
{
	FILE*mic_fp;
	mic_fp = fopen("/sdcard/mic.sw","w");
	while(!end)
	{
		if(pcm_read(capture_buffer , chunk_size)<0)
		{
			printf("error pcm read\n");
			exit(0);
		}
		fwrite(capture_buffer, 1, chunk_bytes ,mic_fp);
	}
	fclose(mic_fp);
	sync();
	exit(0);
}
#endif
int init_and_start_sound(){
	//int err;
	int latency;
	//int i;
	if(open_snd()<0)
		return -1;
	if (setparams(playback_handle, capture_handle, &latency) < 0)
        	goto __error;

	chunk_size = latency;
	chunk_bytes = latency *bits_per_frame/8;

	playback_buffer = (char *)malloc(chunk_bytes);
	capture_buffer = (char *)malloc(chunk_bytes);
	delay_buffer = (char *)malloc(chunk_bytes);
  	if(!playback_buffer || !capture_buffer||!delay_buffer)
  	{
  		printf("cannot malloc buffer for playback and capture\n");
		goto __error;
  	}

	if(init_syn_buffer()<0||init_playback_buffer()<0||init_echo_buffer()<0)
		goto __error;		
	init_amrdecoder();
	pthread_mutex_init(&st_lock , NULL);
	pthread_mutex_init(&drop_pcm_lock , NULL);
#if TEST
	printf("TEST!\n");
	printf(" the argument set correct now goto test\n");
	printf("chunk_size==%u\n",chunk_size);
	printf("buffer_frames = %u\n",buffer_frames);
	printf("chunk_bytes=%i\n",chunk_bytes);
	turn_on_speaker();
	FILE*mic_fp , *ref_fp , *out_fp;
	short *out_buf;
	struct timeval starttime , endtime;

	init_speex_echo();
	mic_fp = fopen("/sdcard/mic.sw","r");
	ref_fp = fopen("/sdcard/ref.sw","r");
	out_fp = fopen("/sdcard/out.sw","w");
	out_buf = malloc(chunk_bytes/channels);
	mic_buf = malloc(chunk_bytes/channels);
	ref_buf = malloc(chunk_bytes/channels);
	gettimeofday(&starttime , NULL);
	
	for(;;)
	{
		if(fread(playback_buffer , chunk_bytes, 1 , ref_fp)!=1 ||
			fread(capture_buffer , chunk_bytes, 1 , mic_fp)!=1)
			break;
		TwoCHtoOne((short * )playback_buffer, ref_buf, chunk_size);
		TwoCHtoOne((short * )capture_buffer, mic_buf, chunk_size);
		do_speex_echo(mic_buf, ref_buf,  out_buf, chunk_size);
		fwrite(out_buf , chunk_bytes/channels, 1 , out_fp);
	}
	gettimeofday(&endtime , NULL);
	destroy_speex_echo();
	printf("process 8s data used %d ms\n", (endtime.tv_sec - starttime.tv_sec)*1000 +(endtime.tv_usec - starttime.tv_usec)/1000);
	fclose(mic_fp);fclose(ref_fp);fclose(out_fp);
	sync();
	exit(0);
	
	/*
	pthread_t tid;
	pthread_create(&tid , NULL , write_thread , NULL);
	pthread_create(&tid , NULL , read_thread , NULL);
	for(;;)sleep(1000);
	*/
	/*
	ref_fp = fopen("/sdcard/ref.sw","w");
	printf("================start record=============\n");
	gettimeofday(&starttime , NULL);
	for(;;)
	{
		if(pcm_read(capture_buffer, chunk_size)<0)
		{
			printf("error pcm read\n");
			exit(0);
		}
		fwrite(capture_buffer , chunk_bytes , 1 , ref_fp);
		gettimeofday(&endtime, NULL);
		if(endtime.tv_sec - starttime.tv_sec>10)
			break;
	}
	printf("==============end record==============\n");
	fclose(ref_fp);
	sync();
	exit(0);
	*/
	return -1;
#else
	printf("###############chunk_size==%d##############\n",(int)chunk_size);
	printf("###############buffer_size==%d##############\n",(int)buffer_frames);
	printf("###############chunk_bytes == %d###########\n",(int)chunk_bytes);
	printf("init sound sucess\n");
	/*
	for(;;)
	{
		if(pcm_read(capture_buffer , chunk_size)<0)
		{
			printf("pcm read error\n");
			exit(0);
		}
		printf("pcm read ok\n");
	}
	*/
	return 0;
#endif	
__error:
	printf("no thread have been create ,we free all resource ourselves\n");
	if(playback_handle)
		snd_pcm_close(playback_handle);
	if(capture_handle)
		snd_pcm_close(capture_handle);
	if(playback_buffer)
		free(playback_buffer);
	if(capture_buffer)
		free(capture_buffer);
	return -1;
}

int start_play_sound_thread(struct sess_ctx * sess)
{
#define  RECV_SOUND_BUF_SIZE    SIZE_OF_AMR_PER_PERIOD *PERIOD_TO_WRITE_EACH_TIME
	char buf[RECV_SOUND_BUF_SIZE];
	int n;
	int s;
	int size;
	int sockfd = sess->sc->audio_socket;
	sess->ucount ++;
	size = 0;
	sleep(3);
	for(;;){
		if(!sess->running)
			goto __exit;
		while(size <SIZE_OF_AMR_PER_PERIOD){
			if(sess->is_tcp)
				n=recv(sockfd,buf+size,RECV_SOUND_BUF_SIZE- size,0);
			else
				n=udt_recv(sockfd , SOCK_STREAM , buf+size ,RECV_SOUND_BUF_SIZE- size,NULL,NULL);
			if(n <=0)
				goto __exit;
			size += n;
			//printf("recv sound data len = %d\n", n);
		}
		s = 0;
		while(size >=SIZE_OF_AMR_PER_PERIOD){
			n = put_play_sound_data(sess->id, buf+s, size);
			s += n;
			size -= n;
		}
		if(size){
			memcpy(buf , buf + s , size);
			//printf("after put size = %d\n",size);
		}
		usleep(1000);
	}
__exit:
	printf("exit recv sound thread\n");
	clear_play_sound_data(sess->id);
	pthread_mutex_lock(&sess->sesslock);
	sess->ucount--;
	if(sess->ucount<=0){
		pthread_mutex_unlock(&sess->sesslock);
		free_system_session(sess);
	}else
		pthread_mutex_unlock(&sess->sesslock);
	return 0;
}

int start_audio_monitor(struct sess_ctx*sess)
{
	
	//int on;
	//ssize_t ret=0;
	ssize_t s;
	ssize_t n;
	char *buf;
	ssize_t size;
	pthread_t  tid;
	char *start_buf;
	int attempts = 0;
	int sockfd = sess->sc->audio_socket;
	sess->ucount ++;
		
		start_buf = (char *)malloc(BOOT_SOUND_STORE_SIZE *2);
		if(!start_buf){
			printf("error malloc sound cache before send\n");
			goto __exit;
		}
		if (pthread_create(&tid, NULL, (void *) start_play_sound_thread, sess) < 0) {
			goto __exit;
		} 
	       set_syn_sound_data_clean(sess->id);
		 /*prepare 3sec sound data before send*/
		 s = 0;
		 while(s<BOOT_SOUND_STORE_SIZE){
		 	buf=new_get_sound_data(sess->id,&size);
			if(!buf){
				usleep(50000);
				continue;
			}
			memcpy(start_buf + s , buf , size);
			s += size;
			free(buf);
		 }
		 size = s;
		 s = 0;
		 while(size > 0){
		 	if(!sess->running){
				dbg("the sess have been closed exit now\n");
				free(start_buf);
				goto __exit;
		 	}
		 	if(size > 1024){
				if(sess->is_tcp)
					n=send(sockfd,start_buf+s,1024,0);
				else
					n=udt_send(sockfd , SOCK_STREAM , start_buf +s ,1024);
			}else{
				if(sess->is_tcp)
					n=send(sockfd,start_buf+s,size,0);
				else
					n=udt_send(sockfd , SOCK_STREAM , start_buf+s , size);
			}
			if(n <=0){
				attempts ++;
				if(attempts <=10){
					dbg("attempts send data now = %d\n",attempts);
					continue;
				}
				printf("send 3s sound data error\n");
				free(start_buf);
				goto __exit;
			}
			attempts = 0;
			size -=n;
			s +=n;
		 }
		 free(start_buf);
		 /*send sound data in normal*/
		while(1){
	__tryget:
			buf=new_get_sound_data(sess->id,&size);
			if(!buf){
				//printf("sound data not prepare\n");
				usleep(50000);
				if(!sess->running){
					goto __exit;
				}
				goto __tryget;
			}
			//if(size>416)
				//printf("tcp send sound get size==%d\n",size);
			s=0;
			while(size>0){
				if(!sess->running){
					dbg("the sess have been closed exit now\n");
					free(buf);
					goto __exit;
				}
				if(size>1024){
					if(sess->is_tcp)
						n=send(sockfd,buf+s,1024,0);
					else
						n=udt_send(sockfd ,SOCK_STREAM , buf +s , 1024);
				}else{
					if(sess->is_tcp)
						n=send(sockfd,buf+s,size,0);
					else
						n=udt_send(sockfd , SOCK_STREAM ,buf+s , size);
				}
				if(n>0){
					attempts = 0;
					s+=n;
					size-=n;
					//dbg("send sound data n==%d\n",n);
				}else{
					attempts ++;
					if(attempts <=10){
						dbg("attempts send data now = %d\n",attempts);
						continue;
					}
					printf("send sound data error\n");
					free(buf);
					goto __exit;
				}
			}
			free(buf);
			//printf("send sound data count==%d\n",i);
			usleep(1000);
	}
__exit:
	printf("exit send sound thread\n");
	pthread_mutex_lock(&sess->sesslock);
	sess->ucount--;
	if(sess->ucount<=0){
		pthread_mutex_unlock(&sess->sesslock);
		free_system_session(sess);
	}else
		pthread_mutex_unlock(&sess->sesslock);
	return 0;
}

//FILE *pcm_fp;
//#define AMR_MAGIC_NUMBER "#!AMR\n"

static inline int encode_and_syn_data(CHP_U32 bl_handle , CHP_AUD_ENC_DATA_T *p_enc_data)
{
	CHP_RTN_T error_flag;
	error_flag = amrnb_encode(bl_handle, p_enc_data);
	if(error_flag == CHP_RTN_AUD_ENC_FAIL || error_flag == CHP_RTN_AUD_ENC_NEED_MORE_DATA){
		printf("###########amr encode fail##################\n");
		return -1;
	}
	p_enc_data->used_size = 0;
	pthread_rwlock_wrlock(&syn_buf.syn_buf_lock);
	memcpy(syn_buf.c_sound_array[syn_buf.end].buf , syn_buf.cache , SIZE_OF_AMR_PER_PERIOD);
	memset(syn_buf.c_sound_array[syn_buf.end].sess_clean_mask, 0 ,sizeof(syn_buf.c_sound_array[syn_buf.end].sess_clean_mask));
	syn_buf.end = (syn_buf.end+1)%NUM_BUFFERS;
	if(syn_buf.end == syn_buf.start)
		syn_buf.start = (syn_buf.start +1)%NUM_BUFFERS;
	pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
	return 0;
}

/*
static snd_pcm_sframes_t cdelay =0 , pdelay =0 ;
static int echo_buf_num = 0;

static inline int read_sound(CHP_U32 bl_handle , CHP_AUD_ENC_DATA_T *p_enc_data)
{
	int len;
	int frames;
	int slow_count;
	pthread_mutex_lock(&echo_buf.echo_pos_lock);
	slow_count = 0;
	if(echo_buf.expect_echo_pos != echo_buf.echo_pos){
		echo_buf.echo_pos += (chunk_bytes/channels);
		echo_buf.echo_pos %=echo_buf.maxlen;
	}
	while(echo_buf.expect_echo_pos != echo_buf.echo_pos)
	{
		slow_count++;
		if(pcm_read((u_char*)capture_buffer,chunk_size)<0)
		{
			printf("grab sound data error\n");
			return -1;
		}
		TwoCHtoOne((short *) capture_buffer, ( short *)(p_enc_data->p_in_buf));
		if(encode_and_syn_data( bl_handle,  p_enc_data)<0)
			return -1;
		echo_buf.expect_echo_pos += (chunk_bytes/channels);
		if(echo_buf.expect_echo_pos>=echo_buf.maxlen)
			echo_buf.expect_echo_pos = 0;
	}
	if(slow_count)
		printf("pcm read slow %d perios\n",slow_count);
	if(pcm_read((u_char*)capture_buffer,chunk_size)<0)
	{
		printf("grab sound data error\n");
		return -1;
	}
	if(echo_buf.silent[echo_buf.echo_pos/(chunk_bytes/channels)])
	{
		TwoCHtoOne((short *) capture_buffer, ( short *)(p_enc_data->p_in_buf));
		if(encode_and_syn_data( bl_handle,  p_enc_data)<0)
			return -1;
	}
	else
	{
		if(first_aec){
			first_aec = 0;
			gettimeofday(&aectime, NULL);
			printf("##############################aec delay#######################\n");
			printf("writetime sec %d usec %d aec time sec %d usec %d diff  %d ms\n",
				writetime.tv_sec , writetime.tv_usec , aectime.tv_sec , aectime.tv_usec ,( aectime.tv_sec - writetime.tv_sec)*1000 +(aectime.tv_usec - writetime.tv_usec)/1000);
			printf("#############################end############################\n");
		}
		TwoCHtoOne((short *) capture_buffer, ( short *)mic_buf);
		speex_echo_cancellation(st,(short*)mic_buf, (short *)(echo_buf.buf+echo_buf.echo_pos),(short *)( p_enc_data->p_in_buf));
		speex_preprocess_run(den, (short*)( p_enc_data->p_in_buf));
		//memcpy(p_enc_data->p_in_buf , mic_buf , chunk_bytes/channels);
		if(encode_and_syn_data(bl_handle,  p_enc_data)<0)
			return -1;
	}
	 echo_buf.echo_pos += (chunk_bytes/channels);
	if(echo_buf.echo_pos >= echo_buf.maxlen)
		echo_buf.echo_pos = 0;
	echo_buf.expect_echo_pos = echo_buf.echo_pos;
	pthread_mutex_unlock(&echo_buf.echo_pos_lock);
	if(echo_buf.echo_pos == echo_buf.write_pos)
	{
		printf("WARNING: echo pos is faster than write pos\n");
		while(echo_buf.echo_pos == echo_buf.write_pos)
			usleep(1000);
	}
	return 0;
}
static inline int new_grab_sound_data(CHP_U32 bl_handle , CHP_AUD_ENC_DATA_T *p_enc_data )
{
	int r , i ;
	short *s , *d;
	CHP_RTN_T error_flag;
	int len;
	int frames;
	int err;
	pthread_t tid;
	if(p_sound_buf_have_data())
	{
		if((err = snd_pcm_delay(playback_handle , &pdelay))!=0)
		{
			printf("get pcm playback delay fail : %s\n", snd_strerror(err));
			return -1;
		}
		
		if(pdelay<0){
			do{
				if(pcm_write(capture_buffer, 0)<0)
					return -1;
				if(snd_pcm_delay(playback_handle , &pdelay)!=0){
					continue;
				}
			}while(pdelay<0);
		}
		
		while(pdelay>0)
		{
			r=pcm_read((u_char*)capture_buffer,chunk_size);
			if(r<0){
				printf("grab sound data error\n");
				return -1;
			}
			TwoCHtoOne((short *)capture_buffer, (short*)(p_enc_data->p_in_buf));
			if(encode_and_syn_data( bl_handle,  p_enc_data)<0)
				return -1;
			if(snd_pcm_delay(playback_handle , &pdelay)!=0){
				printf("#####in while(pdelay>160) snd_pcm_delay error#######\n");
				break;
			}
		}
		if((err = snd_pcm_delay(capture_handle , &cdelay))!=0)
		{
			printf("get pcm capture delay fail:%s\n",snd_strerror(err));
			return -1;
		}
		while(cdelay>0)
		{
			if(p_sound_buf.cache_play_pos >=p_sound_buf.cache_data_len)
				get_play_sound_data(&len);
			if(p_sound_buf.cache_data_len>0)
			{
				if(first_write){
					gettimeofday(&writetime , NULL);
					first_write = 0;
				}
				amrdecoder(p_sound_buf.cache +p_sound_buf.cache_play_pos, SIZE_OF_AMR_PER_PERIOD,playback_buffer, &frames, channels);
				p_sound_buf.cache_play_pos += SIZE_OF_AMR_PER_PERIOD;
				if(pcm_write(playback_buffer, frames)<0)
					return -1;
				TwoCHtoOne((short *)playback_buffer, (short *)(echo_buf.buf+echo_buf.write_pos));
				echo_buf.silent[echo_buf.write_pos/(chunk_bytes/channels)] = 0;
				echo_buf.write_pos += (chunk_bytes/channels);
				if(echo_buf.write_pos >=echo_buf.maxlen)
					echo_buf.write_pos = 0;
				if(echo_buf.write_pos == echo_buf.echo_pos)
				{
					printf("WARNING: write pos is faster than echo pos\n");
				}
			}
			else
			{
				if(pcm_write(playback_buffer, 0)<0)
						return -1;
				echo_buf.silent[echo_buf.write_pos/(chunk_bytes/channels)] = 1;
				echo_buf.write_pos += (chunk_bytes/channels);
				if(echo_buf.write_pos >=echo_buf.maxlen)
					echo_buf.write_pos = 0;
				if(echo_buf.write_pos == echo_buf.echo_pos)
				{
					printf("WARNING: write pos is faster than echo pos\n");
				}
			}
			r=pcm_read((u_char*)capture_buffer,chunk_size);
			if(r<0){
				printf("grab sound data error\n");
				return -1;
			}
			TwoCHtoOne((short *) capture_buffer, ( short *)(p_enc_data->p_in_buf));
			if(encode_and_syn_data( bl_handle,  p_enc_data)<0)
				return -1;
			cdelay -= chunk_size;
		}
		pthread_create(&tid , NULL , write_sound_data , NULL);
		for(;;)
		{
			if(read_sound( bl_handle,  p_enc_data)<0)
				return -1;
			usleep(100);
		}
	}
	else
	{
		if(snd_pcm_delay(playback_handle , &pdelay)!=0){
			if (pcm_write(playback_buffer, 0)<0)
				return -1;	
		}else if(pdelay<PERIODS_PER_BUFFSIZE/2){
			if (pcm_write(playback_buffer, 0)<0)
				return -1;
		}
		r=pcm_read((u_char*)capture_buffer,chunk_size);
		if(r<0){
			printf("grab sound data error\n");
			return -1;
		}
		TwoCHtoOne((short *) capture_buffer, ( short *)(p_enc_data->p_in_buf));
		if(encode_and_syn_data( bl_handle,  p_enc_data)<0)
			return -1;
		return 0;
	}
	return 0;
}

int grab_sound_thread()
{
	CHP_MEM_FUNC_T mem_func;
	CHP_AUD_ENC_INFO_T audio_info;
	CHP_AUD_ENC_DATA_T enc_data;
	CHP_U32 bl_handle;
	CHP_RTN_T error_flag;
	int sampleRate;
	int i;
	mem_func.chp_malloc = (CHP_MALLOC_FUNC)malloc;
	mem_func.chp_free = (CHP_FREE_FUNC)free;
	mem_func.chp_memset = (CHP_MEMSET)memset;
	mem_func.chp_memcpy = (CHP_MEMCPY)memcpy;
	audio_info.audio_type = CHP_DRI_CODEC_AMRNB;
	audio_info.bit_rate = 12200;
	//audio_info.sample_rate = 8000;
	//audio_info.sample_size = 16;
	//audio_info.channel_mode = 1;
	error_flag = amrnb_encoder_init( &mem_func, &audio_info, & bl_handle);
	if(error_flag!=CHP_RTN_SUCCESS){
		printf("error init new amr encoder\n");
		return -1;
	}
	enc_data.p_in_buf = malloc(chunk_bytes/channels);

	ref_buf = (short *)malloc(chunk_bytes/channels);
	mic_buf = (short *)malloc(chunk_bytes/channels);
	out_buf = (short *)malloc(chunk_bytes/channels);
	if(!enc_data.p_in_buf||!ref_buf||!mic_buf){
		printf("error malloc buff for new encoder\n");
		return -1;
	}
	enc_data.p_out_buf = syn_buf.cache;
	enc_data.frame_cnt = SIZE_OF_AMR_PER_PERIOD /32;
	enc_data.in_buf_len = chunk_bytes/channels;
	enc_data.out_buf_len = SIZE_OF_AMR_PER_PERIOD;
	enc_data.used_size = 0;
	enc_data.enc_data_len = 0;

	sampleRate = 8000;
	st = speex_echo_state_init(chunk_size, chunk_size*10);
       den = speex_preprocess_state_init(chunk_size, sampleRate);
       speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
       speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);


	pcm_fp = fopen("/sdcard/linrizeng.pcm", "w");
	if(!pcm_fp){
		printf("error open pcm file\n");
		exit(0);
	}
	
	turn_on_speaker();
	while(1){
		if(is_do_update()){
			dbg("is do update exit now\n");
			return 0;
		}
		if(new_grab_sound_data(bl_handle , &enc_data)<0){
			printf("grab sound data error\n");
			exit(-1);
		}
		usleep(1000);
	}
	return 0;
}

*/

void *read_sound_data(void *arg)
{
	for(;;)
	{
		pthread_mutex_lock(&drop_pcm_lock );
		if(pcm_read((u_char*)capture_buffer , chunk_size)<0)
		{
			printf("pcm read error\n");
			exit(0);
		}
		pthread_mutex_unlock(&drop_pcm_lock);
		put_mic_data(capture_buffer);
	}
}
static int first_write = 1;
void *write_sound_data(void *arg)
{
	int len;
	int frames;
	snd_pcm_sframes_t   pdelay;
	turn_on_speaker();
	for(;;)
	{
		if(p_sound_buf.cache_play_pos >=p_sound_buf.cache_data_len)
			get_play_sound_data(&len);
		if(p_sound_buf.cache_data_len>0)
		{
			if(first_write == 1)
			{
				first_write = 0;
				turn_off_speaker();
				reset_pcm();
				turn_on_speaker();
				init_speex_echo();
			}
			amrdecoder(p_sound_buf.cache +p_sound_buf.cache_play_pos, SIZE_OF_AMR_PER_PERIOD,playback_buffer, &frames, channels);
			p_sound_buf.cache_play_pos += SIZE_OF_AMR_PER_PERIOD;
			if(pcm_write((u_char*)playback_buffer, frames)<0){
				printf("pcm write error\n");
				exit(0);
			}
			put_echo_data(playback_buffer);
		}
		else
		{
			if(snd_pcm_delay(playback_handle , &pdelay)!=0||pdelay <=160)
			{
				if(pcm_write((u_char*)playback_buffer, 0)<0)
				{
					printf("pcm write error\n");
					exit(0);
				}
				destroy_speex_echo();
				first_write = 1;
			}
			usleep(10000);
		}
	}
	return NULL;
}

void *handle_echo_buf_thead(void *arg)
{
	CHP_MEM_FUNC_T mem_func;
	CHP_AUD_ENC_INFO_T audio_info;
	CHP_AUD_ENC_DATA_T enc_data;
	CHP_U32 bl_handle;
	CHP_RTN_T error_flag;
	//int i;
	mem_func.chp_malloc = (CHP_MALLOC_FUNC)malloc;
	mem_func.chp_free = (CHP_FREE_FUNC)free;
	mem_func.chp_memset = (CHP_MEMSET)memset;
	mem_func.chp_memcpy = (CHP_MEMCPY)memcpy;
	audio_info.audio_type = CHP_DRI_CODEC_AMRNB;
	audio_info.bit_rate = 12200;
	//audio_info.sample_rate = 8000;
	//audio_info.sample_size = 16;
	//audio_info.channel_mode = 1;
	error_flag = amrnb_encoder_init( &mem_func, &audio_info, & bl_handle);
	if(error_flag!=CHP_RTN_SUCCESS){
		printf("error init new amr encoder\n");
		exit(0);
	}
	enc_data.p_in_buf = malloc(chunk_bytes/channels);
	enc_data.p_out_buf = syn_buf.cache;
	enc_data.frame_cnt = SIZE_OF_AMR_PER_PERIOD /32;
	enc_data.in_buf_len = chunk_bytes/channels;
	enc_data.out_buf_len = SIZE_OF_AMR_PER_PERIOD;
	enc_data.used_size = 0;
	enc_data.enc_data_len = 0;


	ref_buf = (short *)malloc(chunk_bytes/channels);
	mic_buf = (short *)malloc(chunk_bytes/channels);
	//out_buf = (short *)malloc(chunk_bytes);
	
	if(!ref_buf||!mic_buf/*||!out_buf*/)
	{
		printf("error malloc buff for new encoder\n");
		exit(0);
	}
	
	init_speex_echo();

	FILE*mic_fp , *ref_fp , *out_fp;
	mic_fp = fopen("/sdcard/mic.sw","w");
	ref_fp = fopen("/sdcard/ref.sw","w");
	out_fp = fopen("/sdcard/out.sw","w");
	for(;;)
	{
		while(echo_buf.mic_cancel_pos != echo_buf.mic_wrpos)
		{
			if(echo_buf.echo_cancel_pos == echo_buf.echo_wrpos)
			{
				TwoCHtoOne((short *)(echo_buf.mic_buf[echo_buf.mic_cancel_pos]), (short *)(enc_data.p_in_buf) , chunk_size);
				//printf("don't do aec\n");
			}
			else
			{
				fwrite(echo_buf.mic_buf[echo_buf.mic_cancel_pos] , chunk_bytes ,1 , mic_fp);
				fwrite(echo_buf.echo_buf[echo_buf.echo_cancel_pos] , chunk_bytes , 1 , ref_fp);
				TwoCHtoOne((short *)(echo_buf.mic_buf[echo_buf.mic_cancel_pos]), mic_buf , chunk_size);
				TwoCHtoOne((short *)(echo_buf.echo_buf[echo_buf.echo_cancel_pos]), ref_buf , chunk_size);
				do_speex_echo( mic_buf,  ref_buf, (short*)(enc_data.p_in_buf), chunk_size);
				fwrite(enc_data.p_in_buf , chunk_bytes/channels , 1 , out_fp);
				echo_buf.echo_cancel_pos++;
				if(echo_buf.echo_cancel_pos>=ECHO_BUFFER_NUMS)
					echo_buf.echo_cancel_pos = 0;
				//printf("do aec\n");
			}
			if(encode_and_syn_data( bl_handle, &enc_data)<0)
			{
				printf("encode and syn data error\n");
				exit(0);
			}
			echo_buf.mic_cancel_pos ++;
			if(echo_buf.mic_cancel_pos >= ECHO_BUFFER_NUMS)
				echo_buf.mic_cancel_pos = 0;
		}
		usleep(1000);
	}
	return NULL;
}

int start_sound_thread()
{
	pthread_attr_t attr;
	pthread_t tid;
	struct sched_param param;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr , SCHED_RR);
	if((param.sched_priority = sched_get_priority_max(SCHED_RR))<0)
	{
		return -1;
	}
	pthread_attr_setschedparam(&attr , &param);
	pthread_create(&capture_tid, NULL , read_sound_data , NULL);
	pthread_create(&playback_tid ,NULL , write_sound_data , NULL);
	if (pthread_create(&tid, &attr, (void *) handle_echo_buf_thead, NULL) < 0) {
		return -1;
	} 
	return 0;
}


