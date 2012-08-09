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

#include "speex_echo.h"
#include "speex_preprocess.h"

#define L_PCM_8K			160
#define L_PCM_16K			320

/*pcm frames per 0.02s(a frame of amr),if it is change,you should change DEFAULT_SPEED to corresponding speed*/
#define L_PCM_USE			L_PCM_8K

#define DEFAULT_FORMAT		 SND_PCM_FORMAT_S16_LE   /*don't change it */
#define DEFAULT_SPEED 		 8000  /*change it if the L_PCM_USE is changed*/
#define DEFAULT_CHANNELS	 2	/*don't change it*/
#define CHAUNK_BYTES_MAX	 (4096*100)  /*change it to match your sound card*/
#define PERIODS_PER_BUFFSIZE  8/*change it to match your sound card*/
#define PCM_NAME			 "plughw:0,0"  
#define SOUND_PORT			5000
#define AMR_MODE			7     /* 0-7 if it is not in this range the program will use 7 as default*/

/*next two define just for debug */
#define DEFAULT_IP			"192.168.1.151"
#define AUTO_SEARCH_IPCAM   0

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
struct audioparams{
	snd_pcm_format_t   format;
	unsigned int 		  channels;
	unsigned int 		  rate;
	snd_pcm_uframes_t chunk_size;
	snd_pcm_uframes_t period_frames;
	snd_pcm_uframes_t buffer_frames;
	int 			  can_pause;
	int 			  monotonic;
	int 			  bits_per_sample;
	int    			  bits_per_frame ;
	int 			  chunk_bytes;
	int 			  resample;				
};

//read thread and write thread parameters
struct   threadparams{
	pthread_mutex_t audiothreadlock;
	struct audioparams *params;
	struct{
		char 		  *audiobuf;
		snd_pcm_t	  *handle;
		int 			   socket;
		struct sockaddr_in *saddr;
		pthread_t			  tid;
	}rdthread,wrthread;
	int 				  ucount;
	u16 				  port;
	int 				  running;
	pthread_rwlock_t 	 buf_stat_lock;
	int 				 start;
	int 				 end;
	sound_buf_t		c_soud_buf[NUM_BUFFERS];
	pthread_mutex_t     cop_data_lock;
	char 			  *cop_data_buf;
	int 				   cop_buf_size;
	ssize_t 			   cop_data_length;
	pthread_t			  readed_t[MAX_CONNECTIONS];
	struct sockaddr_in  from;
};


struct __syn_sound_buf syn_buf;
static struct threadparams audiothreadparams;
const unsigned char amr_f_head[8]={0x04,0x0c,0x14,0x1c,0x24,0x2c,0x34,0x3c};
const unsigned char amr_f_size[]={13,14,16,18,20,21,27,32,6, 1, 1, 1, 1, 1, 1, 1};

#define thread_exit()  do{}while(0)

/*the next thread_exit function just for test , i am lazy to delete all refernces in the file now,do it later*/
/*
static void thread_exit() 
{
	pthread_mutex_lock(&audiothreadparams.audiothreadlock);
	audiothreadparams.running=0;
	pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
}
*/
/*
static void signal_handler(int sig)
{
	pthread_mutex_lock(&audiothreadparams.audiothreadlock);
	if (audiothreadparams.rdthread.handle){
		snd_pcm_close(audiothreadparams.rdthread.handle);
		audiothreadparams.rdthread.handle=NULL;
	}
	if(audiothreadparams.wrthread.handle){
		snd_pcm_close(audiothreadparams.wrthread.handle);
		audiothreadparams.wrthread.handle=NULL;
	}
	if(audiothreadparams.wrthread.socket>=0){
		close(audiothreadparams.wrthread.socket);
		audiothreadparams.wrthread.socket=-1;
	}
	if(audiothreadparams.rdthread.socket>=0){
		close(audiothreadparams.rdthread.socket);
		audiothreadparams.rdthread.socket=-1;
	}
	pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
	exit(-1);
}
*/

/*
* I am not sure if we need to set params at each open mode(read or write) 
*/

#define IOCTL_GET_SPK_CTL   _IOR('x',0x01,int)
#define IOCTL_SET_SPK_CTL   _IOW('x',0x02,int)

static inline int turn_on_speaker()
{
	 int fd; 

	// printf("test program\n");
	        
	 fd = open ("/dev/mxs-gpio", O_RDWR);
	 if (fd<0)
	  {   
	       dbg ("file open error \n");
	        return -1; 
	  }   
	  ioctl(fd, IOCTL_SET_SPK_CTL, 1); 
	    
	  dbg("speaker is %s\n",ioctl(fd, IOCTL_GET_SPK_CTL, 0)?"on":"off");

	  close (fd);

	   return 0;

}
static inline int turn_off_speaker()
{
	int fd; 

	// printf("test program\n");
	        
	 fd = open ("/dev/mxs-gpio", O_RDWR);
	 if (fd<0)
	  {   
	       dbg ("file open error \n");
	        return -1; 
	  }   
	  ioctl(fd, IOCTL_SET_SPK_CTL, 0); 
	    
	  dbg("speaker is %s\n",ioctl(fd, IOCTL_GET_SPK_CTL, 0)?"on":"off");

	  close (fd);

	   return 0;

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

		 audiothreadparams.params->format    = DEFAULT_FORMAT;
		 audiothreadparams.params->rate        = DEFAULT_SPEED;
		 audiothreadparams.params->channels = DEFAULT_CHANNELS;
	
		 err = snd_pcm_hw_params_any(handle, params);
		 if (err < 0) {
				 printf("Broken configuration for %s PCM: no configurations available: %s\n", snd_strerror(err), id);
				 return err;
		 }
		 err = snd_pcm_hw_params_set_rate_resample(handle, params,audiothreadparams.params->resample);
		 if (err < 0) {
				 printf("Resample setup failed for %s (val %i): %s\n", id, audiothreadparams.params->resample, snd_strerror(err));
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
		 rrate =audiothreadparams.params->rate;
		 err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
		 if (err < 0) {
				 printf("Rate %iHz not available for %s: %s\n", audiothreadparams.params->rate, id, snd_strerror(err));
				 return err;
		 }
		 if ((int)rrate != audiothreadparams.params->rate) {
				 printf("Rate doesn't match (requested %iHz, get %iHz)\n", audiothreadparams.params->rate, err);
				 return -EINVAL;
		 }
		 audiothreadparams.params->bits_per_sample = snd_pcm_format_physical_width(audiothreadparams.params->format);
		 audiothreadparams.params->bits_per_frame =audiothreadparams.params->bits_per_sample * audiothreadparams.params->channels;
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
		periodsize = bufsize * PERIODS_PER_BUFFSIZE;
		err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
		if (err < 0) {
				printf("Unable to set buffer size %li for %s: %s\n", bufsize * PERIODS_PER_BUFFSIZE, id, snd_strerror(err));
				return err;
		}
		audiothreadparams.params->buffer_frames=periodsize;
		periodsize /= PERIODS_PER_BUFFSIZE;
		err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
		if (err < 0) {
				printf("Unable to set period size %li for %s: %s\n", periodsize, id, snd_strerror(err));
				return err;
		}
		audiothreadparams.params->period_frames=periodsize;
		audiothreadparams.params->chunk_size=periodsize;
		audiothreadparams.params->chunk_bytes =audiothreadparams.params->chunk_size * audiothreadparams.params->bits_per_frame / 8;
		return 0;
}

static int setparams_set(snd_pcm_t *handle,
                   snd_pcm_hw_params_t *params,
                   snd_pcm_sw_params_t *swparams,
                   const char *id)
 {
         int err;
         snd_pcm_uframes_t val;
		 
		 audiothreadparams.params->monotonic = snd_pcm_hw_params_is_monotonic(params);
		 audiothreadparams.params->can_pause = snd_pcm_hw_params_can_pause(params);
 
         err = snd_pcm_hw_params(handle, params);
         if (err < 0) {
                 printf("Unable to set hw params for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
         err = snd_pcm_sw_params_current(handle, swparams);
         if (err < 0) {
                 printf("Unable to determine current swparams for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
	  if((strncmp(id,"capture",7))==0){
	  	val=(double)(audiothreadparams.params->rate)*1/(double)1000000;
	  }
	  else
	  	val=audiothreadparams.params->buffer_frames;
	  if(val<1)
	  	val=1;
	  else if(val>audiothreadparams.params->buffer_frames)
	  	val=audiothreadparams.params->buffer_frames;
         err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 1);
         if (err < 0) {
                 printf("Unable to set start threshold mode for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
         err = snd_pcm_sw_params_set_avail_min(handle, swparams, audiothreadparams.params->chunk_size);
         if (err < 0) {
                 printf("Unable to set avail min for %s: %s\n", id, snd_strerror(err));
                 return err;
         }
	 if((strncmp(id,"capture",7))==0){
	  	val=audiothreadparams.params->buffer_frames;
	  }
	  else
	  	val=audiothreadparams.params->buffer_frames<<1;
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
         snd_pcm_uframes_t p_size, c_size, p_psize, c_psize;
         unsigned int p_time, c_time;
         unsigned int val;
 
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
	 if((err=snd_pcm_hw_params_get_buffer_size_max(pt_params,&audiothreadparams.params->buffer_frames))<0){
		return err;
	 }
	 *bufsize=(int)(audiothreadparams.params->buffer_frames/PERIODS_PER_BUFFSIZE);
	 *bufsize=((int)(((*bufsize)+L_PCM_USE-1)/L_PCM_USE))*L_PCM_USE;//set the period size round to 160 for 8k raw sound data convert to amr (8000HZ*0.02s=160 frames)
	 *bufsize-=L_PCM_USE;
	 last_bufsize=*bufsize;
 
       __again:
         if (last_bufsize == *bufsize)
                 *bufsize += L_PCM_USE;
         last_bufsize = *bufsize;
         if (*bufsize >( CHAUNK_BYTES_MAX/(audiothreadparams.params->bits_per_frame/8))){
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
 
         snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
         if (p_psize * PERIODS_PER_BUFFSIZE< p_size) {
                 snd_pcm_hw_params_get_periods_min(p_params, &val, NULL);
		   printf("minimal periods per buffer==%d",val);
                 if (val > PERIODS_PER_BUFFSIZE) {
                         printf("playback device does not support %d periods per buffer\n",PERIODS_PER_BUFFSIZE);
                         return -1;
                 }
                 goto __again;
         }
         snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
         if (c_psize * PERIODS_PER_BUFFSIZE < c_size) {
                 snd_pcm_hw_params_get_periods_min(c_params, &val, NULL);
		   printf("minimal periods per buffer==%d",val);
                 if (val > PERIODS_PER_BUFFSIZE) {
                         printf("capture device does not support %d periods per buffer\n",PERIODS_PER_BUFFSIZE);
                         return -1;
                 }
                 goto __again;
         }
	 audiothreadparams.params->chunk_size=*bufsize; 
        audiothreadparams.params->chunk_bytes=(*bufsize)*audiothreadparams.params->bits_per_frame/8;
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
		thread_exit();
		return -1;
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		if (audiothreadparams.params->monotonic) {
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
			thread_exit();
			return -1;
		}
		return 0 ;		/* ok, data should be accepted again */
	} 
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
		printf("Status(DRAINING)!\n");
		if (audiothreadparams.rdthread.tid==pthread_self()) {
			printf("capture stream format change? attempting recover...\n");
			if ((res = snd_pcm_prepare(handle))<0) {
				printf("xrun(DRAINING): prepare error: %s\n", snd_strerror(res));
				thread_exit();
				return -1;
			}
			return 0;
		}
	}
	printf("read/write error, state = %s\n", snd_pcm_state_name(snd_pcm_status_get_state(status)));
	thread_exit();
	return -1;
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
			thread_exit();
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

	if (count <audiothreadparams.params->chunk_size) {
		snd_pcm_format_set_silence(audiothreadparams.params->format, 
			data + count *audiothreadparams.params->bits_per_frame / 8,
			(audiothreadparams.params->chunk_size - count) * audiothreadparams.params->channels);
		count = audiothreadparams.params->chunk_size;
	}
	while (count > 0) {
		r = snd_pcm_writei(audiothreadparams.wrthread.handle, data, count);
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			snd_pcm_wait(audiothreadparams.wrthread.handle, 100);
		} else if (r == -EPIPE) {
			printf("pcm_write under run\n");
			if(xrun(audiothreadparams.wrthread.handle)<0)
				return -1;
		} else if (r == -ESTRPIPE) {
			if(suspend(audiothreadparams.wrthread.handle)<0)
				return -1;
		} else if (r < 0) {
			printf("write error: %s\n", snd_strerror(r));
			thread_exit();
			return -1;
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r *(audiothreadparams.params->bits_per_frame / 8);
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

	if (count != audiothreadparams.params->chunk_size) {
		printf("pcm_read rcount do not equal chunk_size\n");
		count = audiothreadparams.params->chunk_size;
	}
	
	while (count > 0) {
		r = snd_pcm_readi(audiothreadparams.rdthread.handle, data, count);
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			snd_pcm_wait(audiothreadparams.rdthread.handle, 100);
		} else if (r == -EPIPE) {
			printf("pcm_read over run\n");
			if(xrun(audiothreadparams.rdthread.handle)<0)
				return -1;
			increase_video_thread_sleep_time();
		} else if (r == -ESTRPIPE) {
			if(suspend(audiothreadparams.rdthread.handle)<0)
				return -1;
		} else if (r < 0) {
		//for debuf
			snd_pcm_status_t *status;
			int res;
			snd_pcm_status_alloca(&status);
			printf("read error: %s\n", snd_strerror(r));
			if ((res = snd_pcm_status(audiothreadparams.rdthread.handle, status))<0) {
				printf("status error: %s\n", snd_strerror(res));
				thread_exit();
				return -1;
			}
			printf("state=%s\n",snd_pcm_state_name(snd_pcm_status_get_state(status)));
			thread_exit();
			return -1;
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r *(audiothreadparams.params->bits_per_frame / 8);
		}
	}
	return result;
}
/*
static int writethread(){
	socklen_t fromlen;
    	ssize_t data_len=0;
	ssize_t r;
	ssize_t n;
	int on;
	struct sockaddr_in  from;
    if ((audiothreadparams.wrthread.socket= create_udp_socket()) < 0) {
     	 	perror("In write thread error creating recv socket\n");
		audiothreadparams.wrthread.socket=-1;
        	goto __exit;
  	 }

	 on = 1;
     if (setsockopt(audiothreadparams.wrthread.socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
                    	 printf("Error enabling socket address reuse\n");
     }
	 
	 audiothreadparams.wrthread.saddr= (struct sockaddr_in*)bind_udp_socket(audiothreadparams.wrthread.socket, audiothreadparams.port);

     if (audiothreadparams.wrthread.saddr == NULL) {
          printf("****************************In write thread error binding udp socket****************************\n");
	      goto __exit;
     }
    //dbg("sock %d bound to port %d\n", sess->sock, sess->port);  

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);
    fromlen = sizeof(struct sockaddr_in);
	printf("all ok now goto write data to sound card\n");
    while(1){
		if(!audiothreadparams.wrthread.audiobuf)
			goto __exit;
		pthread_mutex_lock(&audiothreadparams.audiothreadlock);
		if(!audiothreadparams.running){
			pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
			printf("something wrong now kill myself\n");
			goto __exit;
		}
		pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
		r=audiothreadparams.params->chunk_bytes;
		data_len=0;
		while(data_len < audiothreadparams.params->chunk_bytes){
			n= recvfrom(audiothreadparams.wrthread.socket, 
				audiothreadparams.wrthread.audiobuf+data_len,r, 0, (struct sockaddr *) &from, &fromlen);
			if(n>0){
#if AUTO_SEARCH_IPCAM
				if(from.sin_addr.s_addr!=audiothreadparams.from.sin_addr.s_addr)
					continue;
#endif
				data_len+=n;
				r-=n;
				printf("recv data==%d\n",n);
			}else{
				printf("recvfrom error sleep and continue\n");
				usleep(10);
			}
		}
#if AUTO_SEARCH_IPCAM
#else
		pthread_mutex_lock(&audiothreadparams.audiothreadlock);
		memcpy(&audiothreadparams.from,&from,fromlen);
		pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
#endif
		pcm_write((u_char *)(audiothreadparams.wrthread.audiobuf), (size_t)(audiothreadparams.params->chunk_size));
		usleep(10);
    	}

__exit:
	pthread_mutex_lock(&audiothreadparams.audiothreadlock);
	audiothreadparams.ucount--;
	if(audiothreadparams.running)
		audiothreadparams.running=0;
	if(audiothreadparams.ucount==0&&audiothreadparams.params)
		free(audiothreadparams.params);
	pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
	if(audiothreadparams.wrthread.socket>=0){
		close(audiothreadparams.wrthread.socket);
		audiothreadparams.wrthread.socket=-1;
	}
	if(audiothreadparams.wrthread.handle){
		snd_pcm_close(audiothreadparams.wrthread.handle);
		audiothreadparams.wrthread.handle=NULL;
	}
	if(audiothreadparams.wrthread.audiobuf)
		free(audiothreadparams.wrthread.audiobuf);
	return 0;
}
static int readthread(){
	socklen_t fromlen;
    	ssize_t ret=0;
	ssize_t s;
	ssize_t n;
	int on;
	struct sockaddr_in  from;
    if ((audiothreadparams.rdthread.socket= create_udp_socket()) < 0) {
      	perror("In read thread error creating recv socket\n");
		audiothreadparams.rdthread.socket=-1;
        goto __exit;
  	}

	on = 1;
    if (setsockopt(audiothreadparams.rdthread.socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
        printf("Error enabling socket address reuse\n");
    }

	audiothreadparams.rdthread.saddr= (struct sockaddr_in*)bind_udp_socket(audiothreadparams.rdthread.socket, audiothreadparams.port);
				   
    if (audiothreadparams.rdthread.saddr == NULL) {
        printf("****************************In read thread error binding udp socket****************************\n");
	    goto __exit;
   	 }  

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);
    	fromlen = sizeof(struct sockaddr_in);
#if AUTO_SEARCH_IPCAM
	pthread_mutex_lock(&audiothreadparams.audiothreadlock);
	memcpy(&from,&audiothreadparams.from,sizeof(struct sockaddr));
	pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
#endif
	printf("all ok now goto read data from sound card\n");
    while(1){
		if(!audiothreadparams.rdthread.audiobuf)
			goto __exit;
		pthread_mutex_lock(&audiothreadparams.audiothreadlock);
		if(!audiothreadparams.running){
			pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
			printf("something wrong now kill myself\n");
			goto __exit;
		}
		pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
		if(pcm_read((u_char *)(audiothreadparams.rdthread.audiobuf), (size_t)(audiothreadparams.params->chunk_size))<0)
			continue;
#if AUTO_SEARCH_IPCAM
#else
		pthread_mutex_lock(&audiothreadparams.audiothreadlock);
		memcpy(&from,&audiothreadparams.from,sizeof(struct sockaddr));
		pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
#endif
		ret=audiothreadparams.params->chunk_bytes;
		s=0;
		while(ret>0){
			if(ret>1000){
				n=sendto(audiothreadparams.rdthread.socket,audiothreadparams.rdthread.audiobuf+s,1000,0,(struct sockaddr*)&from,sizeof(struct sockaddr));
			}else{
				n=sendto(audiothreadparams.rdthread.socket,audiothreadparams.rdthread.audiobuf+s,ret,0,(struct sockaddr*)&from,sizeof(struct sockaddr));
			}
			if(n>0){
				s+=n;
				ret-=n;
			}else{
				printf("send data error,sleep and try again");
				usleep(1000);
			}
		}
		usleep(10);
    }

__exit:
	pthread_mutex_lock(&audiothreadparams.audiothreadlock);
	audiothreadparams.ucount--;
	if(audiothreadparams.running)
		audiothreadparams.running=0;
	if(audiothreadparams.ucount==0&&audiothreadparams.params)
		free(audiothreadparams.params);
	pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
	if(audiothreadparams.rdthread.socket>=0){
		close(audiothreadparams.rdthread.socket);
		audiothreadparams.rdthread.socket=-1;
	}
	if(audiothreadparams.rdthread.handle){
		snd_pcm_close(audiothreadparams.rdthread.handle);
		audiothreadparams.rdthread.handle=NULL;
	}
	if(audiothreadparams.rdthread.audiobuf)
		free(audiothreadparams.rdthread.audiobuf);
	return 0;
}
*/
static int test_sound_tcp_read_data(struct sess_ctx*sess){
	ssize_t data_len=0;
	ssize_t r;
	ssize_t ret;
	ssize_t n;
	char *src;
	//unsigned char *psrc;
	ssize_t pcm_frames;
	struct timeval recvtime;
	//int waitdata=0;  /*If we recv data length not round to a amr frame size,recv again till it be*/
	//int i=0;
	const ssize_t rd=(audiothreadparams.params->chunk_size+L_PCM_USE-1)/L_PCM_USE*amr_f_size[AMR_MODE];
	int sockfd=sess->s3;
	recvtime.tv_sec=1;
	recvtime.tv_usec=0;
	//if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char *)&recvtime,sizeof(struct timeval))<0){
		//printf("set sound tcp socket recv time out error\n");
	//}
	src=malloc(rd);
	if(!src){
		printf("malloc buffer to recv amr data error\n");
		goto __exit;
	}
	//dbg("##############sess->id=%d################\n",sess->id);
	while(1){
		r=rd;
		data_len=0;
		pthread_mutex_lock(&sess->sesslock);
		if(!sess->running){
			pthread_mutex_unlock(&sess->sesslock);
			goto __exit;
		}
		pthread_mutex_unlock(&sess->sesslock);
		//n=recv(sockfd,src+data_len,r,0);
		
		while(r>0){
			n=recv(sockfd,src+data_len,r,0);
			if(n>0){
				r-=n;
				data_len+=n;
			}else{
				printf("******************recv error ********************\n");
				goto __exit;
			}
		}
		printf("recv sound data n==%d\n",data_len);
		//r=amrdecoder(src,data_len  ,audiothreadparams.wrthread.audiobuf,&pcm_frames,2);
		printf("after decode pcm_frames==%d\n",pcm_frames);
		if(r>=0){
			ret=pcm_write((u_char *)(audiothreadparams.wrthread.audiobuf), pcm_frames);
			if (ret<0){
				printf("pcm write silent error something wrong!\n");
				free(src);
				goto __exit;
			}
		}else{
			printf("amr decoder error\n");
			goto __exit;
		}
		/*
		if(n<=0){
			pthread_mutex_lock(&audiothreadparams.audiothreadlock);
			ret=pcm_write((u_char *)(audiothreadparams.wrthread.audiobuf), 0);
			pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
			if (ret<0){
				printf("pcm write silent error something wrong!\n");
				goto __exit;
			}
			//printf("write silent n==%d\n",n);
			if(!waitdata)
				continue;
			else
				goto __again;
		}
		data_len+=n;
		if(data_len%amr_f_size[AMR_MODE]!=0){
			if(data_len>rd){
				printf("the sound socket do not run well ,can't control it,I will exit!\n");
				goto __exit;
			}
			r=rd-data_len;
			waitdata=1;
			goto __again;
		}
		waitdata=0;
		//printf("recv sound data n==%d\n",n);
		psrc=(unsigned char *)src;
		while((psrc-(unsigned char *)src)<data_len&&(*psrc!=amr_f_head[AMR_MODE]))psrc+=amr_f_size[AMR_MODE];
		if((psrc-(unsigned char *)src)>=data_len){
			printf("recv bad amr data\n");
			continue;
		}
		pthread_mutex_lock(&audiothreadparams.audiothreadlock);
		r=amrdecoder((char *)psrc,data_len-((char *)psrc-src),audiothreadparams.wrthread.audiobuf,&pcm_frames,2);
		if(r<0){
			pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
			printf("amr decoder error\n");
			goto __exit;
		}
		//printf("pcm_frames==%d\n",pcm_frames);
		r=pcm_write((u_char *)(audiothreadparams.wrthread.audiobuf), (size_t)pcm_frames);
		pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
		if (r<0){
			printf("pcm write  error something wrong!\n");
			goto __exit;
		}
		i++;
		//printf("recv data count==%d\n",i);
		*/
		usleep(1000);
	}
__exit:	
	free(src);
	printf("exit tcp read data thread\n");
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
	
	int on;
	ssize_t ret=0;
	ssize_t s;
	ssize_t n;
	char *buf;
	ssize_t size;
	char *start_buf;
	int attempts = 0;
	int sockfd = sess->sc->audio_socket;
	sess->ucount ++;
		
		start_buf = (char *)malloc(BOOT_SOUND_STORE_SIZE *2);
		if(!start_buf){
			printf("error malloc sound cache before send\n");
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

int test_sound_tcp_transport(struct sess_ctx* sess){
	int lsockfd=-1;
	int sockfd=-1;
	int on;
	socklen_t fromlen;
	ssize_t ret=0;
	ssize_t s;
	ssize_t n;
	struct sockaddr_in address;
	struct sockaddr_in *myaddr;
	struct timeval selecttv;
	fd_set acceptfds;
	int tryaccept = MAX_CONNECTIONS;
	char *buf;
	ssize_t size;
	char *start_buf;
	if ((lsockfd= create_tcp_socket()) < 0) {
             printf("Error creating sound listen socket");
		pthread_mutex_lock(&sess->sesslock);
		sess->ucount--;
		if(sess->ucount<=0){
			pthread_mutex_unlock(&sess->sesslock);
			free_system_session(sess);
		}else
			pthread_mutex_unlock(&sess->sesslock);
             return -1;
       }
		
            /* Allow address reuse */
       on = 1;
       if (setsockopt(lsockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
               printf("Error enabling listen sound socket address reuse");
       }

       if ((myaddr = bind_tcp_socket(lsockfd, SOUND_PORT)) == NULL){
             	  printf("Error binding sound listen socket\n");
                goto __exit;
       }
	  free(myaddr);

       if (listen(lsockfd, MAX_CONNECTIONS) < 0){
               printf("Error listening for sound connection");
               goto __exit;
       }
	  // dbg("##############sess->id=%d################\n",sess->id);
	while(1){
		pthread_mutex_lock(&sess->sesslock);
		if(!sess->running){
			pthread_mutex_unlock(&sess->sesslock);
			goto __exit;
		}
		pthread_mutex_unlock(&sess->sesslock);
		printf("sound thread is running ,ready to connect\n");
__tryaccept:
		selecttv.tv_sec = 5;
		selecttv.tv_usec = 0;
		fromlen = sizeof(struct sockaddr_in);
		FD_ZERO(&acceptfds);
		FD_SET(lsockfd, &acceptfds);
		do{
			ret = select(lsockfd+ 1, &acceptfds, NULL, NULL, &selecttv);
		}while(ret == -1);
		if(ret == 0){
			tryaccept -- ;
			if(tryaccept<=0){
				goto __exit;
			}
			if(sess->running)
				goto __tryaccept;
			else
				goto __exit;
		}
			
		sockfd = accept(lsockfd,(struct sockaddr *) &address,&fromlen);
		if(sockfd<0){
			printf("accept sound tcp error\n");
			goto __exit;
		}
		if(sess->from.sin_addr.s_addr!=address.sin_addr.s_addr){
			tryaccept --;
			close(sockfd);
			sockfd = -1;
			if(tryaccept <=0){
				goto __exit;
			}
			if(sess->running)
				goto __tryaccept;
			else
				goto __exit;
		}
		
		printf("sound tcp connection in, addr=0x%x, port=0x%d\n",address.sin_addr.s_addr, address.sin_port);
		close(lsockfd);
		sess->s3=sockfd;
		lsockfd=-1;
		selecttv.tv_sec  = 10;
		selecttv.tv_usec = 0;
		 if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &selecttv, sizeof(selecttv)) < 0){
		            printf("Error enabling sockfd snd time out \n");
		  }

		/*
		if (pthread_create(&sess->swtid, NULL, (void *) test_sound_tcp_read_data, sess) < 0) {
			printf("create write sound thread error\n");
			goto __exit;
		} 
		pthread_mutex_lock(&sess->sesslock);
		sess->ucount++;
		pthread_mutex_unlock(&sess->sesslock);
		printf("create write sound thread sucess\n");
		*/
		//pthread_join(sess->swtid , NULL);
		//goto __exit;
		start_buf = (char *)malloc(BOOT_SOUND_STORE_SIZE *2);
		if(!start_buf){
			printf("error malloc sound cache before send\n");
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
		 	if(size > 1024){
				n=send(sockfd,start_buf+s,1024,0);
			}else{
				n=send(sockfd,start_buf+s,size,0);
			}
			if(n <=0){
				printf("send 3s sound data error\n");
				free(start_buf);
				goto __exit;
			}
			size -=n;
			s +=n;
		 }
		 free(start_buf);
		 /*send sound data in normal*/
		while(1){
			pthread_mutex_lock(&sess->sesslock);
			if(!sess->running){
				pthread_mutex_unlock(&sess->sesslock);
				goto __exit;
			}
			pthread_mutex_unlock(&sess->sesslock);
			/*
			if(pcm_read((u_char *)(audiothreadparams.rdthread.audiobuf), (size_t)(audiothreadparams.params->chunk_size))<0){
				printf("pcm read error\n");
				goto __exit;
			}
			buf=malloc ((audiothreadparams.params->chunk_size+L_PCM_USE-1)/L_PCM_USE*amr_f_size[AMR_MODE]);
			if(!buf){
				printf("malloc buffer for amr coder fail\n");
				goto __exit;
			}
			ret=amrcoder(audiothreadparams.rdthread.audiobuf, audiothreadparams.params->chunk_bytes, buf, & size,AMR_MODE,2);
			if(ret<0){
				printf("amr coder error\n");
				free(buf);
				goto __exit;
			}
			*/
	__tryget:
			buf=new_get_sound_data(sess->id,&size);
			if(!buf){
				//printf("sound data not prepare\n");
				usleep(50000);
				pthread_mutex_lock(&sess->sesslock);
				if(!sess->running){
					pthread_mutex_unlock(&sess->sesslock);
					goto __exit;
				}
				pthread_mutex_unlock(&sess->sesslock);
				goto __tryget;
			}
			//if(size>416)
				//printf("tcp send sound get size==%d\n",size);
			s=0;
			while(size>0){
				if(size>1024){
					n=send(sockfd,buf+s,1024,0);
				}else{
					n=send(sockfd,buf+s,size,0);
				}
				if(n>0){
					s+=n;
					size-=n;
					//dbg("send sound data n==%d\n",n);
				}else{
					printf("send sound data error\n");
					free(buf);
					goto __exit;
				}
			}
			free(buf);
			//printf("send sound data count==%d\n",i);
			usleep(1000);
		}
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
	if(lsockfd>=0)
		close(lsockfd);
	if(sockfd>=0)
		close(sockfd);
	/*
	snd_pcm_close(audiothreadparams.wrthread.handle);
	snd_pcm_close(audiothreadparams.rdthread.handle);
	free(audiothreadparams.wrthread.audiobuf);
	free(audiothreadparams.rdthread.audiobuf);
	free(audiothreadparams.params);
	*/
	return 0;
}

#define TEST 0
int init_and_start_sound(){
	int err;
	int latency;
	int i;
#if TEST
	//int n=1;
	//ssize_t dst_size;
	ssize_t pcm_frames;
	//int ret;
#endif
	memset(&audiothreadparams,0,sizeof(audiothreadparams));
	pthread_mutex_init(&audiothreadparams.audiothreadlock,NULL);
	pthread_mutex_init(&audiothreadparams.cop_data_lock,NULL);
	
	audiothreadparams.params=malloc(sizeof(struct audioparams));
	if(!audiothreadparams.params)
		return -1;
	memset(audiothreadparams.params,0,sizeof(struct audioparams));
	audiothreadparams.params->resample=1;
	audiothreadparams.port=SOUND_PORT;
	audiothreadparams.running=1;

	if ((err = snd_pcm_open(&(audiothreadparams.wrthread.handle),PCM_NAME, SND_PCM_STREAM_PLAYBACK,0)) < 0) {
                 printf("Playback open error: %s\n", snd_strerror(err));
				 audiothreadparams.running=0;
                 return -1;
  	 }
    	if ((err = snd_pcm_open(&(audiothreadparams.rdthread.handle), PCM_NAME, SND_PCM_STREAM_CAPTURE,0)) < 0) {
                 printf("Record open error: %s\n", snd_strerror(err));
	          audiothreadparams.running=0;
                 goto __error;
   	 }

	
	if (setparams(audiothreadparams.wrthread.handle, audiothreadparams.rdthread.handle, &latency) < 0)
        	goto __error;
	/* i donnt know why i link the playback and capture handle then the capture handle will fail to restart the sound card after recover from xrun
   	 if ((err = snd_pcm_link(audiothreadparams.rdthread.handle,audiothreadparams.wrthread.handle)) < 0) {
        	 printf("Streams link error: %s\n", snd_strerror(err));
         	goto __error;
   	 }
   	 */
    	 audiothreadparams.params->chunk_size=latency;
        audiothreadparams.params->chunk_bytes=latency*audiothreadparams.params->bits_per_frame/8;
		
  	 audiothreadparams.wrthread.audiobuf=malloc(audiothreadparams.params->chunk_bytes);
	if(!audiothreadparams.wrthread.audiobuf){
	 	printf("cannot malloc wrthread buff\n");
		goto __error;
	}
	audiothreadparams.rdthread.audiobuf=malloc(audiothreadparams.params->chunk_bytes);
	if(!audiothreadparams.rdthread.audiobuf){
		printf("cannot malloc rdthread buff\n");
		goto __error;
	}

	if(is_speaker_on()==1)
		dbg("#############speaker is on################\n");
	else
		dbg("#############speaker is off################\n");
	//dbg("############try turn on speaker################\n");
	//turn_on_speaker();
	
	for(i=0;i<PERIODS_PER_BUFFSIZE;i++){
     		 if (pcm_write((u_char*)(audiothreadparams.wrthread.audiobuf),(size_t)0) < 0) {
           		 printf("write error\n");
            		 goto __error;
         	 }
	}
	//sleep(3);
	//turn_off_speaker();
	/*
	if((init_amrcoder(0))<0){
		printf("init amrcoder error\n");
		goto __error;
	}
	if((init_amrdecoder())<0){
		printf("init amrdecoder error\n");
		goto __error;
	}
	*/
	audiothreadparams.cop_buf_size = ((audiothreadparams.params->chunk_size+L_PCM_USE-1)/L_PCM_USE*amr_f_size[AMR_MODE]);
	audiothreadparams.cop_data_buf=malloc(audiothreadparams.cop_buf_size);
	if(!audiothreadparams.cop_data_buf){
		printf("unable to malloc cop_data_buf\n");
		goto __error;
	}
	/*
  	 if ((err = snd_pcm_start(audiothreadparams.rdthread.handle)) < 0) {
          	printf("start sound card Go error: %s\n", snd_strerror(err));
          	goto __error;
   	 }	
   	 */
   	 
   	 syn_buf.buffsize = NUM_BUFFERS * SIZE_OF_AMR_PER_PERIOD;
	syn_buf.absolute_start_addr = (char *)malloc(syn_buf.buffsize);
	if(!syn_buf.absolute_start_addr){
		printf("error malloc syn_sound_buffer\n");
		goto __error;
	}
	syn_buf.cache = (char *)malloc(SIZE_OF_AMR_PER_PERIOD);
	if(!syn_buf.cache){
		printf("malloc syn_buf cache error\n");
		free(syn_buf.absolute_start_addr);
		goto __error;
	}
	for(i=0;i<NUM_BUFFERS;i++){
		syn_buf.c_sound_array[i].buf = syn_buf.absolute_start_addr+i*SIZE_OF_AMR_PER_PERIOD;
		memset(syn_buf.c_sound_array[i].sess_clean_mask,1,sizeof(syn_buf.c_sound_array[i].sess_clean_mask));
	}
	syn_buf.start = 0;
	syn_buf.end = 0;
	pthread_rwlock_init(&syn_buf.syn_buf_lock , NULL);
	
   	 /*
	 syn_buf.buffsize =((int)((32*50*2+STEP-1)/STEP))*STEP;
	syn_buf.buffsize+=STEP;
	pthread_mutex_init(&syn_buf.syn_buf_lock,NULL);
	syn_buf.start = 0;
	syn_buf.end = 0;
	syn_buf.buf = (char *)malloc(syn_buf.buffsize);
	if(!syn_buf.buf){
		printf("***************unable to malloc syn_buf\n*************************");
		goto __error;
	}
	printf("############ok malloc syn_buf  buf size=%d###################\n",syn_buf.buffsize);
	memset(syn_buf.buf,0,syn_buf.buffsize);
	*/
	audiothreadparams.ucount=1;
#if TEST
	printf("TEST!\n");
	printf(" the argument set correct now goto test\n");
	printf("chunk_size==%d\n",audiothreadparams.params->chunk_size);
	printf("chunk_bytes=%d\n",audiothreadparams.params->chunk_bytes);
	turn_on_speaker();
	int r;
	int not_write = 1;
	FILE*fp;
	FILE*fp1;
	int dn;
	int noiseSuppress = -25; 
	//int i;
	//spx_int16_t*p2cmic;
	//spx_int16_t*p2cecho;
	spx_int16_t *mic_buf;
	spx_int16_t *echo_buf;
	spx_int16_t *out_buf;
	//char *dst;
	//char *src;
	//char *tmp;
	ssize_t src_size=0;
	SpeexEchoState *st = NULL;
	SpeexPreprocessState *den = NULL;
	st = speex_echo_state_init_mc(audiothreadparams.params->chunk_size, 2400, 2 ,2);
	den = speex_preprocess_state_init(audiothreadparams.params->chunk_size, 8000);
	int tmp = 8000;
	speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &tmp);
	//speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);
	i = 1;
	 speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_VAD, &i);
	i = 1;
	speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_AGC, &i);
	dn = 1;
	speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_DENOISE, &dn);
	//noiseSuppress = -25;  
	//speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);
	//mic_buf = (spx_uint16_t*)malloc(audiothreadparams.params->chunk_bytes/2);
	//echo_buf = (spx_uint16_t*)malloc(audiothreadparams.params->chunk_bytes/2);
	out_buf =(spx_uint16_t*)malloc(audiothreadparams.params->chunk_bytes);
	if(!out_buf/*||!mic_buf||!echo_buf*/){
		printf("malloc buf for out_buf error\n");
		return -1;
	}
	pcm_frames = audiothreadparams.params->chunk_size;
	while(1){
		r=pcm_read((u_char*)audiothreadparams.rdthread.audiobuf,audiothreadparams.params->chunk_size);
		if(r<0){
			printf("grab sound data error\n");
			return -1;
		}
		if(not_write){
			not_write = 0;
			memcpy(audiothreadparams.wrthread.audiobuf , audiothreadparams.rdthread.audiobuf ,audiothreadparams.params->chunk_bytes);
		}else{
		/*
			p2cmic =(spx_int16_t *) audiothreadparams.rdthread.audiobuf;
			p2cecho =(spx_int16_t*)audiothreadparams.wrthread.audiobuf;
			for(i = 0; i<audiothreadparams.params->chunk_size;i++){
				mic_buf[i] = p2cmic[i*2];
				echo_buf[i] = p2cecho[i*2];
			}
			*/
			mic_buf = (spx_int16_t *) audiothreadparams.rdthread.audiobuf;
			echo_buf = (spx_int16_t*)audiothreadparams.wrthread.audiobuf;
			speex_echo_cancellation( st, mic_buf, echo_buf, out_buf);
			speex_preprocess_run(den, out_buf);
			memcpy(audiothreadparams.wrthread.audiobuf, (char *)out_buf , audiothreadparams.params->chunk_bytes);
			/*
			p2cecho =(spx_int16_t*)audiothreadparams.wrthread.audiobuf;
			for(i = 0; i<audiothreadparams.params->chunk_size;i++){
				*p2cecho = out_buf[i];
				p2cecho++;
				*p2cecho = out_buf[i];
				p2cecho++;
			}
			*/
		}
		r=pcm_write(audiothreadparams.wrthread.audiobuf, pcm_frames);
		if(r<0){
			printf("pcm write error\n");
			return -1;
		}
		usleep(1000);
	}
	audiothreadparams.running=0;
	snd_pcm_close(audiothreadparams.wrthread.handle);
	snd_pcm_close(audiothreadparams.rdthread.handle);
	free(audiothreadparams.wrthread.audiobuf);
	free(audiothreadparams.rdthread.audiobuf);
	free(audiothreadparams.params);
	return -1;
#else
	printf("chunk_size==%d\n",(int)audiothreadparams.params->chunk_size);
	printf("init sound sucess\n");
	return 0;
#endif	
__error:
	printf("no thread have been create ,we free all resource ourselves\n");
	audiothreadparams.running=0;
	if(audiothreadparams.wrthread.handle)
		snd_pcm_close(audiothreadparams.wrthread.handle);
	if(audiothreadparams.rdthread.handle)
		snd_pcm_close(audiothreadparams.rdthread.handle);
	if(audiothreadparams.wrthread.audiobuf)
		free(audiothreadparams.wrthread.audiobuf);
	if(audiothreadparams.rdthread.audiobuf)
		free(audiothreadparams.rdthread.audiobuf);
	if(audiothreadparams.params)
		free(audiothreadparams.params);
	return -1;
}
/*
char* get_cop_sound_data(ssize_t *size)
{
	char *buffer;
	int i;
	int canuse=0;
	*size=0;
__retry:
	pthread_mutex_lock(&audiothreadparams.cop_data_lock);
	for(i=0;i<MAX_CONNECTIONS;i++){
		if(audiothreadparams.readed_t[i]==0)
			canuse=i;
		else if(audiothreadparams.readed_t[i]==pthread_self()){
			pthread_mutex_unlock(&audiothreadparams.cop_data_lock);
			goto __readed;
		}
	}
	buffer=malloc(audiothreadparams.cop_data_length);
	if(!buffer){
		pthread_mutex_unlock(&audiothreadparams.cop_data_lock);
		return NULL;
	}
	memcpy(buffer,audiothreadparams.cop_data_buf,audiothreadparams.cop_data_length);
	*size=audiothreadparams.cop_data_length;
	audiothreadparams.readed_t[canuse]=pthread_self();
	pthread_mutex_unlock(&audiothreadparams.cop_data_lock);
	return buffer;
__readed:
	usleep(50);
	goto __retry;
}
*/
int play_cop_sound_data(char *buffer,ssize_t length)
{/*
	ssize_t r=0;
	ssize_t pcm_frames;
	int p;
	if(buffer[0]!=amr_f_head[AMR_MODE]){
		printf("error recv sound data ,data head not match the head of amr frame\n");
		printf("we want the head is %02x , but we recv %02x\n",amr_f_head[AMR_MODE] , buffer[0]);
		return 0;
	}
	if(length%amr_f_size[AMR_MODE]){
		printf("error recv sound data length not round to amr_f_size[%d]==%d\n",AMR_MODE , (int)amr_f_size[AMR_MODE]);
		printf("the length of sound data is %d\n",length);
		return 0;
	}
	pthread_mutex_lock(&audiothreadparams.audiothreadlock);
	p=0;
	while(length>0){
		if(length>SIZE_OF_AMR_PER_PERIOD){
			if(amrdecoder(buffer+p, SIZE_OF_AMR_PER_PERIOD, audiothreadparams.wrthread.audiobuf, &pcm_frames, 2)<0){
				printf("amrdecoder error\n");
				pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
				return -1;
			}
			r=pcm_write((u_char *)audiothreadparams.wrthread.audiobuf, pcm_frames);
			length-=SIZE_OF_AMR_PER_PERIOD;
			p+=SIZE_OF_AMR_PER_PERIOD;
		}else{
			if(amrdecoder(buffer+p, length, audiothreadparams.wrthread.audiobuf, &pcm_frames, 2)<0){
				printf("amrdecoder error\n");
				pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
				return -1;
			}
			r=pcm_write((u_char *)audiothreadparams.wrthread.audiobuf, pcm_frames);
			length = 0;
		}
	}
	pthread_mutex_unlock(&audiothreadparams.audiothreadlock);
	if(r<0){
		printf("pcm write error\n");
		return -1;
	}
	*/
	return 0;
}
/*
int grab_sound_data()
{
	int r;
	r=pcm_read((u_char*)audiothreadparams.rdthread.audiobuf,audiothreadparams.params->chunk_size);
	if(r<0){
		printf("grab sound data error\n");
		return -1;
	}
	pthread_mutex_lock(&audiothreadparams.cop_data_lock);
	r=amrcoder(audiothreadparams.rdthread.audiobuf, audiothreadparams.params->chunk_bytes, audiothreadparams.cop_data_buf, & audiothreadparams.cop_data_length,AMR_MODE,2);
	memset(audiothreadparams.readed_t,0,sizeof(audiothreadparams.readed_t));
	pthread_mutex_unlock(&audiothreadparams.cop_data_lock);
	if(r<0)
		return -1;
	return 0;
}
*/
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
//FILE *amr_fp;
//#define AMR_MAGIC_NUMBER "#!AMR\n"
static inline int new_grab_sound_data(CHP_U32 bl_handle , CHP_AUD_ENC_DATA_T *p_enc_data )
{
	int r , i;
	int size;
	unsigned short *s , *d;
	CHP_RTN_T error_flag;
	r=pcm_read((u_char*)audiothreadparams.rdthread.audiobuf,audiothreadparams.params->chunk_size);
	if(r<0){
		printf("grab sound data error\n");
		return -1;
	}
	s =(unsigned short *) audiothreadparams.rdthread.audiobuf;
	d =(unsigned short *)p_enc_data->p_in_buf;
	for(i = 0 , r = 0 ; r <audiothreadparams.params->chunk_bytes/2 ; i++ , r+=2)
	{
		d[i] = s[r];
	}
	error_flag = amrnb_encode(bl_handle, p_enc_data);
	if(error_flag == CHP_RTN_AUD_ENC_FAIL || error_flag == CHP_RTN_AUD_ENC_NEED_MORE_DATA){
		printf("###########amr encode fail##################\n");
		return -1;
	}
	//fwrite(syn_buf.cache , 1 , SIZE_OF_AMR_PER_PERIOD , amr_fp);
	//printf("p_enc_data->used_size = %u\n" , p_enc_data->used_size);
	p_enc_data->used_size = 0;
	//printf("p_enc_data->enc_data_len = %u\n" , p_enc_data->enc_data_len);
	/*
	r=amrcoder(audiothreadparams.rdthread.audiobuf, audiothreadparams.params->chunk_bytes, syn_buf.cache, &size,AMR_MODE,2);
	if(size!=SIZE_OF_AMR_PER_PERIOD){
		printf("BUG the compression size of amr data != SIZE_OF_AMR_PER_PERIOD\n");
		return -1;
	}
	*/
	//printf("amr coder size=%d\n",size);
	pthread_rwlock_wrlock(&syn_buf.syn_buf_lock);
	memcpy(syn_buf.c_sound_array[syn_buf.end].buf , syn_buf.cache , SIZE_OF_AMR_PER_PERIOD);
	memset(syn_buf.c_sound_array[syn_buf.end].sess_clean_mask, 0 ,sizeof(syn_buf.c_sound_array[syn_buf.end].sess_clean_mask));
	syn_buf.end = (syn_buf.end+1)%NUM_BUFFERS;
	if(syn_buf.end == syn_buf.start)
		syn_buf.start = (syn_buf.start +1)%NUM_BUFFERS;
	pthread_rwlock_unlock(&syn_buf.syn_buf_lock);
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
int grab_sound_thread()
{
	CHP_MEM_FUNC_T mem_func;
	CHP_AUD_ENC_INFO_T audio_info;
	CHP_AUD_ENC_DATA_T enc_data;
	CHP_U32 bl_handle;
	CHP_RTN_T error_flag;
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
	enc_data.p_in_buf = malloc(audiothreadparams.params->chunk_bytes/2);
	if(!enc_data.p_in_buf){
		printf("error malloc buff for new encoder\n");
		return -1;
	}
	enc_data.p_out_buf = syn_buf.cache;
	enc_data.frame_cnt = SIZE_OF_AMR_PER_PERIOD /32;
	enc_data.in_buf_len = audiothreadparams.params->chunk_bytes/2;
	enc_data.out_buf_len = SIZE_OF_AMR_PER_PERIOD;
	enc_data.used_size = 0;
	enc_data.enc_data_len = 0;
	/*
	amr_fp = fopen("/sdcard/linrizeng.amr", "w");
	if(!amr_fp){
		printf("error open amr file\n");
		exit(0);
	}
	fwrite(AMR_MAGIC_NUMBER , 1 , strlen(AMR_MAGIC_NUMBER),amr_fp);
	*/
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

