#ifndef __SOUND__H__
#define __SOUND__H__

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    *this struct is used to synchronize
    *the video and audio record
    */
#include "cli.h"

#define L_PCM_8K            160
#define L_PCM_16K            320
#define L_PCM_USE            L_PCM_8K

#define DEFAULT_FORMAT         SND_PCM_FORMAT_S16_LE
#define DEFAULT_SPEED          8000
#define DEFAULT_CHANNELS     1
#define CHAUNK_BYTES_MAX     (4096*100)
#define PERIODS_SIZE         160
#define PERIODS_PER_BUFFSIZE  128
#define PCM_NAME             "plughw:0,0"
#define AMR_MODE            7

#define ECHO_BUFFER_NUMS     128

    /*
    * buffer to store sound for syn video and sound record
    * also send sound thread will get sound from this buffer
    */
#define NUM_BUFFERS               128
#define  SIZE_OF_AMR_PER_PERIOD  32
#define  PERIOD_TO_WRITE_EACH_TIME    13

    /*before we send data we store sound data in 3 sec at first*/
#define BOOT_SOUND_STORE_SIZE 8000

    /*
    *buffer to store sound that we have writed to sound card
    *for echo cacel
    typedef struct __echo_buf
    {
        char *absolute_start_addr;
        char *echo_buf[ECHO_BUFFER_NUMS];
        char *mic_buf[ECHO_BUFFER_NUMS];
        int echo_wrpos;
        int mic_wrpos;
        int echo_cancel_pos;
        int mic_cancel_pos;
    } echo_buf_t;
    */

    int   init_and_start_sound();
    char *new_get_sound_data(int sess_id, int *size);
    void  reset_syn_buf();
    void  set_syn_sound_data_clean(int sess_id);

    int   start_audio_monitor(struct sess_ctx *sess);
    void  start_sound_thread();

#ifdef __cplusplus
}
#endif

#endif

