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
#include "utilities.h"
#include "amr.h"
#include "cli.h"
#include "sound.h"
#include "amrnb_encode.h"
#include "udttools.h"
#include "speex_echo.h"
#include "speex_preprocess.h"

#define timersub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) { \
        --(result)->tv_sec; \
        (result)->tv_usec += 1000000; \
    } \
} while (0)

#define timermsub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
    if ((result)->tv_nsec < 0) { \
        --(result)->tv_sec; \
        (result)->tv_nsec += 1000000000L; \
    } \
} while (0)

#define dbg(fmt, args...) \
do { \
printf(__FILE__ ": %s: " fmt, __func__, ## args); \
} while (0)


typedef struct __sound_buf
{
    char sess_clean_mask[MAX_NUM_IDS];
    char *buf;
} sound_buf_t;

struct __syn_sound_buf
{
    pthread_rwlock_t    syn_buf_lock;
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
} play_sound_unit_buf_t;

struct play_sound_buf
{
    char *absolute_start_addr;
    play_sound_unit_buf_t  p_sound_array[MAX_NUM_IDS];
    int curr_play_pos;
    char *cache;
    int cache_data_len;
    int cache_play_pos;
};

typedef struct _CBuffer {
    char *buffer;
    char *start;
    char *end;
    char *first;
    int   step;
} CBuffer;

static snd_pcm_format_t       format;
static unsigned int           channels;
static unsigned int           rate;
static snd_pcm_uframes_t      chunk_size;
static int                    chunk_bytes;
static snd_pcm_uframes_t      period_frames;
static snd_pcm_uframes_t      buffer_frames;
static int                    bits_per_sample;
static int                    bits_per_frame ;
static int                    resample = 1;
static int                    can_pause;
static int                    monotonic;

static snd_pcm_t             *capture_handle;
static snd_pcm_t             *playback_handle;
static char                  *capture_buffer;
static char                  *playback_buffer;
static CBuffer               *near_end_buffer;
static CBuffer               *echo_buffer;
static pthread_t              capture_tid;

static SpeexEchoState       *echo_state;
static SpeexPreprocessState *echo_pp;

static struct __syn_sound_buf syn_buf;
static struct play_sound_buf  p_sound_buf;
//static echo_buf_t             echo_buf;

static pthread_mutex_t        st_lock;
static pthread_mutex_t        drop_pcm_lock;

#define AEC_DELAY 3
#define NN  160
#define TAIL NN*4

static int init_syn_buffer()
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

static int init_playback_buffer()
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

static int clear_play_sound_data(int sess_id)
{
    while(p_sound_buf.p_sound_array[sess_id].datalen>=p_sound_buf.p_sound_array[sess_id].maxlen)
        usleep(1000);
    if(p_sound_buf.p_sound_array[sess_id].datalen)
        p_sound_buf.p_sound_array[sess_id].datalen = 0;
    return 0;
}

static char *get_play_sound_data(int *len)
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

#define IOCTL_GET_SPK_CTL   _IOR('x',0x01,int)
#define IOCTL_SET_SPK_CTL   _IOW('x',0x02,int)

static int turn_on_speaker()
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

static int turn_off_speaker()
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
      close (fd);
       return 0;
}

static int open_snd()
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

static void check_hw_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params ,const char *id)
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
        return 0 ;        /* ok, data should be accepted again */
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

static void reset_pcm()
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
        sleep(1);    /* wait until suspend flag is released */
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

static CBuffer *circular_init(int size, int step)
{
    CBuffer *cb = malloc(sizeof(CBuffer));

    cb->buffer  = malloc(size);
    cb->start   = cb->buffer;
    cb->end     = cb->buffer + size;
    cb->first   = cb->buffer;
    cb->step    = step;
    return cb;
}

static void circular_free(CBuffer *buffer)
{
    if (buffer)
    {
        free(buffer->buffer);
        free(buffer);
    }
}

static int circular_empty(CBuffer *buffer)
{
    return buffer->start == buffer->first;
}

static void circular_consume(CBuffer *buffer)
{
    if (buffer->start != buffer->first)
    {
        if (buffer->first + buffer->step == buffer->end)
            buffer->first = buffer->buffer;
        else
            buffer->first += buffer->step;
    }
}

static void circular_write(CBuffer *buffer, char *data)
{
    memcpy(buffer->start, data, buffer->step);
    if (buffer->start + buffer->step == buffer->end)
        buffer->start = buffer->buffer;
    else
        buffer->start += buffer->step;

    if (buffer->start == buffer->first)
    {
        printf("circular buffer overrun\n");

        if (buffer->first + buffer->step == buffer->end)
            buffer->first = buffer->buffer;
        else
            buffer->first += buffer->step;
    }
}

int init_and_start_sound()
{
    //int err;
    int latency;
    //int i;
    if(open_snd()<0)
        return -1;
    if (setparams(playback_handle, capture_handle, &latency) < 0)
            goto __error;

    chunk_size = latency;
    chunk_bytes = latency * bits_per_frame / 8;

    playback_buffer = (char *)malloc(chunk_bytes);
    capture_buffer = (char *)malloc(chunk_bytes);
    near_end_buffer = circular_init(chunk_bytes * 256, chunk_bytes);
    echo_buffer = circular_init(chunk_bytes * 256, chunk_bytes);
      if(!playback_buffer || !capture_buffer||!near_end_buffer)
      {
          printf("cannot malloc buffer for playback and capture\n");
        goto __error;
      }

    if(init_syn_buffer()<0||init_playback_buffer()<0)
        goto __error;        

    int sample_rate = 8000;
    printf("### period_frames %li\n", period_frames);
    echo_state = speex_echo_state_init_mc(period_frames,
                                          period_frames * 10, 1, 1);
    speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE,
                   &sample_rate);

    echo_pp = speex_preprocess_state_init(period_frames, sample_rate);
    speex_preprocess_ctl(echo_pp, SPEEX_PREPROCESS_SET_ECHO_STATE,
                         echo_state);

    init_amrdecoder();
    pthread_mutex_init(&st_lock , NULL);
    pthread_mutex_init(&drop_pcm_lock , NULL);
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

#define RECV_SOUND_BUF_SIZE \
    SIZE_OF_AMR_PER_PERIOD * PERIOD_TO_WRITE_EACH_TIME

// ok
static int put_play_sound_data(int sess_id, char *buf, int len)
{
    while (p_sound_buf.p_sound_array[sess_id].datalen >=
           p_sound_buf.p_sound_array[sess_id].maxlen)
    {
        usleep(1000);
    }

    if (len + p_sound_buf.p_sound_array[sess_id].datalen >
        p_sound_buf.p_sound_array[sess_id].maxlen)
    {
        len = p_sound_buf.p_sound_array[sess_id].maxlen -
              p_sound_buf.p_sound_array[sess_id].datalen;
    }

    memcpy(p_sound_buf.p_sound_array[sess_id].buf +
           p_sound_buf.p_sound_array[sess_id].datalen, buf, len);

    p_sound_buf.p_sound_array[sess_id].datalen += len;

    return len;
}

// ok
static void *receive(void *arg)
{
    struct sess_ctx *sess = arg;
    int              sock = sess->sc->audio_socket;
    char             buf[RECV_SOUND_BUF_SIZE];
    int              n, s, size;

    sess->ucount++;
    size = 0;

    sleep(2);

    while (1)
    {
        if(!sess->running)
            goto end;

        while (size < SIZE_OF_AMR_PER_PERIOD)
        {
            if(sess->is_tcp)
                n = recv(sock, buf + size, RECV_SOUND_BUF_SIZE - size, 0);
            else
            {
                n = udt_recv(sock, SOCK_STREAM, buf + size,
                             RECV_SOUND_BUF_SIZE - size, NULL, NULL);
            }
            if(n <= 0)
                goto end;

            size += n;
        }

        s = 0;
        while (size >= SIZE_OF_AMR_PER_PERIOD)
        {
            n     = put_play_sound_data(sess->id, buf + s, size);
            s    += n;
            size -= n;
        }

        if(size)
            memcpy(buf , buf + s , size);

        usleep(1000);
    }

end:
    clear_play_sound_data(sess->id);
    pthread_mutex_lock(&sess->sesslock);
    sess->ucount--;

    if (sess->ucount <= 0)
    {
        pthread_mutex_unlock(&sess->sesslock);
        free_system_session(sess);
    }
    else
        pthread_mutex_unlock(&sess->sesslock);

    return NULL;
}

// ok
int start_audio_monitor(struct sess_ctx*sess)
{
    ssize_t s;
    ssize_t n;
    char *buf;
    ssize_t size;
    pthread_t  tid;
    char *start_buf;
    int attempts = 0;
    int sock = sess->sc->audio_socket;
    sess->ucount ++;
        
    start_buf = (char *)malloc(BOOT_SOUND_STORE_SIZE *2);
    if (!start_buf)
    {
        printf("error malloc sound cache before send\n");
        goto end;
    }

    if (pthread_create(&tid, NULL, (void *)receive, sess) < 0)
        goto end;

    set_syn_sound_data_clean(sess->id);

     /*prepare 3sec sound data before send*/
    s = 0;
    while (s < BOOT_SOUND_STORE_SIZE)
    {
        buf = new_get_sound_data(sess->id,&size);
        if (!buf)
        {
            usleep(50000);
            continue;
        }
        memcpy(start_buf + s , buf , size);
        s += size;
        free(buf);
    }

    size = s;
    s    = 0;
    while (size > 0)
    {
        if (!sess->running)
        {
            dbg("the sess have been closed exit now\n");
            free(start_buf);
            goto end;
        }

        if (size > 1024)
        {
            if(sess->is_tcp)
                n = send(sock, start_buf + s,1024,0);
            else
                n = udt_send(sock, SOCK_STREAM, start_buf + s, 1024);
        }
        else
        {
            if (sess->is_tcp)
                n = send(sock, start_buf + s, size, 0);
            else
                n = udt_send(sock, SOCK_STREAM, start_buf + s, size);
        }

        if (n <= 0)
        {
            attempts ++;
            if (attempts <= 10)
            {
                dbg("attempts send data now = %d\n",attempts);
                continue;
            }
            printf("send 3s sound data error\n");
            free(start_buf);
            goto end;
        }
        attempts = 0;
        size    -= n;
        s       += n;
    }
    free(start_buf);

    /*send sound data in normal*/
    while(1)
    {
tryget:
        buf = new_get_sound_data(sess->id, &size);
        if(!buf)
        {
            usleep(50000);
            if(!sess->running)
                goto end;

            goto tryget;
        }
        s = 0;
        while (size > 0)
        {
            if (!sess->running)
            {
                dbg("the sess have been closed exit now\n");
                free(buf);
                goto end;
            }
            if (size>1024)
            {
                if(sess->is_tcp)
                    n = send(sock, buf + s, 1024, 0);
                else
                    n = udt_send(sock, SOCK_STREAM, buf + s, 1024);
            }
            else
            {
                if (sess->is_tcp)
                    n = send(sock, buf + s, size, 0);
                else
                    n = udt_send(sock, SOCK_STREAM, buf + s, size);
            }
            if (n > 0)
            {
                attempts = 0;
                s       += n;
                size    -= n;
            }
            else
            {
                attempts++;
                if (attempts <= 10)
                {
                    dbg("attempts send data now = %d\n",attempts);
                    continue;
                }
                printf("send sound data error\n");
                free(buf);
                goto end;
            }
        }
        free(buf);
        usleep(1000);
    }

end:
    printf("exit send sound thread\n");
    pthread_mutex_lock(&sess->sesslock);
    sess->ucount--;
    if (sess->ucount <= 0)
    {
        pthread_mutex_unlock(&sess->sesslock);
        free_system_session(sess);
    }
    else
        pthread_mutex_unlock(&sess->sesslock);
    return 0;
}

// ok
static inline int encode_and_syn_data(CHP_U32 bl_handle,
                                      CHP_AUD_ENC_DATA_T *p_enc_data)
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

static void *capture(void *arg)
{
    while (1)
    {
        if (pcm_read((u_char*)capture_buffer, chunk_size) < 0)
        {
            printf("pcm read error\n");
            exit(0);
        }

        pthread_mutex_lock(&drop_pcm_lock );
        circular_write(near_end_buffer, capture_buffer);
        pthread_mutex_unlock(&drop_pcm_lock);
    }
    return NULL;
}

static void *aec(void *arg)
{
    int len;
    int frames;

    CHP_MEM_FUNC_T     mem_func;
    CHP_AUD_ENC_INFO_T audio_info;
    CHP_AUD_ENC_DATA_T enc_data;
    CHP_U32            bl_handle;
    CHP_RTN_T          error_flag;

    mem_func.chp_malloc     = (CHP_MALLOC_FUNC)malloc;
    mem_func.chp_free       = (CHP_FREE_FUNC)free;
    mem_func.chp_memset     = (CHP_MEMSET)memset;
    mem_func.chp_memcpy     = (CHP_MEMCPY)memcpy;

    audio_info.audio_type   = CHP_DRI_CODEC_AMRNB;
    audio_info.bit_rate     = 12200;

    error_flag = amrnb_encoder_init(&mem_func, &audio_info, &bl_handle);

    if(error_flag != CHP_RTN_SUCCESS)
    {
        printf("error init new amr encoder\n");
        exit(0);
    }

    enc_data.p_in_buf       = malloc(chunk_bytes / channels);
    enc_data.p_out_buf      = syn_buf.cache;
    enc_data.frame_cnt      = SIZE_OF_AMR_PER_PERIOD /32;
    enc_data.in_buf_len     = chunk_bytes / channels;
    enc_data.out_buf_len    = SIZE_OF_AMR_PER_PERIOD;
    enc_data.used_size      = 0;
    enc_data.enc_data_len   = 0;


    turn_on_speaker();

    int aec_start = 0;
    int n         = 0;

    while (1)
    {
        if (p_sound_buf.cache_play_pos >= p_sound_buf.cache_data_len)
            get_play_sound_data(&len);

        if (p_sound_buf.cache_data_len > 0)
        {
            amrdecoder(p_sound_buf.cache + p_sound_buf.cache_play_pos,
                       SIZE_OF_AMR_PER_PERIOD,
                       playback_buffer,
                       &frames,
                       channels);
            p_sound_buf.cache_play_pos += SIZE_OF_AMR_PER_PERIOD;

            if (pcm_write((u_char*)playback_buffer, frames) < 0)
            {
                printf("pcm write error\n");
                exit(0);
            }

            if (n < 3)
                n++;
            if (n == 3)
                aec_start = 1;

            pthread_mutex_lock(&drop_pcm_lock );
            circular_write(echo_buffer, playback_buffer);

            if (aec_start)
            {
                if (!circular_empty(near_end_buffer) &&
                    !circular_empty(echo_buffer))
                {
                    speex_echo_cancellation(echo_state,
                        (spx_int16_t *)near_end_buffer->first,
                        (spx_int16_t *)echo_buffer->first,
                        (spx_int16_t *)enc_data.p_in_buf);
                    speex_preprocess_run(echo_pp,
                        (spx_int16_t *)enc_data.p_in_buf);

                    circular_consume(near_end_buffer);
                    pthread_mutex_unlock(&drop_pcm_lock );

                    if (encode_and_syn_data(bl_handle, &enc_data) < 0)
                    {
                        printf("encode and syn data error\n");
                        exit(0);
                    }
                }
                else
                {
                    pthread_mutex_unlock(&drop_pcm_lock );
                    usleep(10000);
                }
            }
            else
            {
                pthread_mutex_unlock(&drop_pcm_lock );
                goto no_aec;
            }
        }
        else
        {
no_aec:
            pthread_mutex_lock(&drop_pcm_lock );
            if (!circular_empty(near_end_buffer))
            {
                memcpy(enc_data.p_in_buf, near_end_buffer->first,
                       chunk_bytes);
                if (encode_and_syn_data(bl_handle, &enc_data) < 0)
                {
                    printf("encode and syn data error\n");
                    exit(0);
                }

                circular_consume(near_end_buffer);
                pthread_mutex_unlock(&drop_pcm_lock );
            }
            else
            {
                pthread_mutex_unlock(&drop_pcm_lock );
                usleep(10000);
            }
        }
    }
    return NULL;
}

void start_sound_thread(void)
{
    pthread_t thread;
    pthread_create(&capture_tid, NULL, capture, NULL);
    pthread_create(&thread, NULL, aec, NULL);
}

