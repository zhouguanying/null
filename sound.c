#include <alsa/asoundlib.h>
#include <sys/ioctl.h>
#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"
#include "amixer.h"
#include "amr.h"
#include "amrnb_encode.h"
#include "cli.h"
#include "sound.h"
#include "server.h"
#include "udttools.h"
#include "data_chunk.h"

// #define SOUND_ENABLE_AEC_PREPROCESS

#define PERIOD_FRAMES             160
#define AMR_PERIOD_BYTES          32 // 160 * 2 / 10 (1:10)
#define AMR_PERIODS               256
#define AEC_DELAY                 1
#define PLAYBACK_DELAY            4

typedef struct _SoundAmrBuffer
{
    int              size;   // AMR_PERIODS * AMR_PERIOD_BYTES
    char            *data;   // start address of buffer
    char            *cache;
    int              start;  // circular start position
    int              end;    // circular end position

    struct
    {
        char *p;
        char  clean[MAX_NUM_IDS];
    } periods[AMR_PERIODS];

    pthread_rwlock_t lock;
} SoundAmrBuffer;

typedef struct _CBuffer
{
    char *buffer;
    char *start;
    char *end;
    char *first;
    int   step;
} CBuffer;

// extern
int                          sound_talking;

// speex aec
static SpeexEchoState       *echo_state;
#ifdef SOUND_ENABLE_AEC_PREPROCESS
static SpeexPreprocessState *echo_pp;
#endif

// circular buffers
static int                   circular_size;
static pthread_mutex_t       circular_mutex;
static CBuffer              *capture_buffer;

// TODO: playback and capture periods are not unity
static data_chunk_t *_playback_buffer;
static data_chunk_t *_echo_buffer;
static int          _period_bytes_pb;
static int          _period_frames_pb;
static char        *_tmp_buf_pb = NULL;

#define SAFE_FREE(p) do { if ((p)) free((p)); (p) = NULL; } while(0)
// playback & capture
static snd_pcm_t            *playback_handle;
static snd_pcm_t            *capture_handle;

// period size & buffer size
static snd_pcm_uframes_t     period_frames;
static int                   period_bytes;

// amr buffer
static SoundAmrBuffer        amr_buf;

// bidirectional talk is limited to only one session
static int                   playback_start;
static int                   underrun_occurred;
static int                   aec_start;
static pthread_t             receive_thread;
static pthread_t             receive_thread_running;
static int                   receive_thread_exit;

void sound_amr_buffer_init()
{
    int i;

    amr_buf.size  = AMR_PERIODS * AMR_PERIOD_BYTES;
    amr_buf.data  = malloc(amr_buf.size);
    amr_buf.cache = malloc(AMR_PERIOD_BYTES);
    amr_buf.start = 0;
    amr_buf.end   = 0;

    for (i = 0; i < AMR_PERIODS; i++)
    {
        amr_buf.periods[i].p = amr_buf.data + i * AMR_PERIOD_BYTES;
        memset(amr_buf.periods[i].clean, 1,
               sizeof(amr_buf.periods[i].clean));
    }

    pthread_rwlock_init(&amr_buf.lock , NULL);
}

void sound_amr_buffer_reset()
{
    pthread_rwlock_wrlock(&amr_buf.lock);
    amr_buf.start = amr_buf.end = 0;
    pthread_rwlock_unlock(&amr_buf.lock);
}

void sound_amr_buffer_clean(int sess_id)
{
    int i;
    pthread_rwlock_wrlock(&amr_buf.lock);
    for (i = amr_buf.start; i != amr_buf.end; i = (i + 1) % AMR_PERIODS)
        amr_buf.periods[i].clean[sess_id] = 1;
    pthread_rwlock_unlock(&amr_buf.lock);
}

char *sound_amr_buffer_fetch(int sess_id, int *size)
{
    int   i;
    char *buf;

    pthread_rwlock_rdlock(&amr_buf.lock);

    for (i = amr_buf.start; i != amr_buf.end; i = (i + 1) % AMR_PERIODS)
    {
        if (!amr_buf.periods[i].clean[sess_id])
            break;
    }

    if (i == amr_buf.end)
    {
        pthread_rwlock_unlock(&amr_buf.lock);
        return NULL;
    }

    if (amr_buf.end > i)
    {
        *size = amr_buf.periods[amr_buf.end].p - amr_buf.periods[i].p;
        buf   = malloc(*size);

        memcpy(buf, amr_buf.periods[i].p, *size);
    }
    else
    {
        *size = amr_buf.size - (amr_buf.periods[i].p - amr_buf.data);
        buf   = malloc(*size + (amr_buf.periods[amr_buf.end].p -
                                amr_buf.data));

        memcpy(buf, amr_buf.periods[i].p, *size);
        memcpy(buf + *size, amr_buf.data,
               amr_buf.periods[amr_buf.end].p - amr_buf.data);

        *size += amr_buf.periods[amr_buf.end].p - amr_buf.data;
    }

    // clean the periods that has been read for this session
    for (; i != amr_buf.end; i = (i + 1) % AMR_PERIODS)
        amr_buf.periods[i].clean[sess_id] = 1;

    pthread_rwlock_unlock(&amr_buf.lock);
    return buf;
}

#define IOCTL_GET_SPK_CTL _IOR('x',0x01,int)
#define IOCTL_SET_SPK_CTL _IOW('x',0x02,int)

static int speaker_on()
{
    int fd = open("/dev/mxs-gpio", O_RDWR);

    if (fd < 0)
        return -1;
    if (ioctl(fd, IOCTL_GET_SPK_CTL, 0) == 0)
        ioctl(fd, IOCTL_SET_SPK_CTL, 1);
    close(fd);

    return 0;
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
        if (buffer == capture_buffer)
            printf("capture_buffer overrun\n");

        if (buffer->first + buffer->step == buffer->end)
            buffer->first = buffer->buffer;
        else
            buffer->first += buffer->step;
    }
}

static void circular_reset(CBuffer *buffer)
{
    buffer->start = buffer->buffer;
    buffer->first = buffer->buffer;
}

static void *receive(void *arg)
{
    char            *pcm  = malloc(period_bytes);
    struct sess_ctx *sess = arg;
    int              sock = sess->sc->audio_socket;
    char             buf[1024];
    int              i, r, rs;
    int              try;
    ssize_t          dst_size;
    int              n = 0;

    // FIXME: no need to lock here?
    sess->ucount++;

    while (!receive_thread_exit)
    {
        if (!sess->running)
            goto end;

        rs  = 0;
        try = 0;
        while (rs < 1024)
        {
start:
            if (sess->is_tcp)
                r = recv(sock, buf + rs, 1024 - rs, 0);
            else
            {
                r = udt_recv(sock, SOCK_STREAM, buf + rs,
                             1024 - rs, NULL, NULL);
            }
            if (r <= 0)
            {
                printf("%s: recv < 0, try again\n", __func__);

                try++;
                if (receive_thread_exit || try == 10)
                    goto end;
                else
                {
                    usleep(1000000);
                    goto start;
                }
            }

            rs += r;
        }

        pthread_mutex_lock(&circular_mutex);
        for (i = 0; i < 32; i++)
        {
            amrdecoder(buf + i * 32,
                       AMR_PERIOD_BYTES,
                       pcm,
                       &dst_size,
                       1);
            data_chunk_pushback(_playback_buffer, (unsigned char *)pcm, 320);
        }
        pthread_mutex_unlock(&circular_mutex);

        if (n < PLAYBACK_DELAY)
        {
            n++;
            if (n == PLAYBACK_DELAY)
                playback_start = 1;
        }
    }

end:
    pthread_mutex_lock(&sess->sesslock);
    sess->ucount--;
    if (sess->ucount <= 0)
    {
        pthread_mutex_unlock(&sess->sesslock);
        free_system_session(sess);
    }
    else
        pthread_mutex_unlock(&sess->sesslock);

    free(pcm);

    // reset
    alsa_set_hp_volume(0, 0);

    aec_start      = 0;
    playback_start = 0;

    receive_thread_running = 0;

    pthread_mutex_lock(&circular_mutex);
    snd_pcm_drop(playback_handle);
    data_chunk_clear(_playback_buffer);
    data_chunk_clear(_echo_buffer);
    speex_echo_state_reset(echo_state);
    pthread_mutex_unlock(&circular_mutex);

    sound_talking  = 0;

    printf("receive thread stopped\n");

    return NULL;
}

static void *capture(void *arg)
{
    snd_pcm_sframes_t  r;
    char              *buffer = malloc(period_bytes);

    while (1)
    {
        // This is being done continuously: "Once the audio interface
        // starts running, it continues to do until told to stop."
        r = snd_pcm_readi(capture_handle, buffer, period_frames);

        if (r == -EPIPE)
        {
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(capture_handle);
        }
        else if (r < 0)
        {
            fprintf(stderr, "snd_pcm_readi(): %s\n",
                    snd_strerror(r));
        }
        else if (r != period_frames)
        {
            fprintf(stderr, "short read: %i -> %i\n",
                    (int) period_frames, (int) r);
        }
        else
        {
            pthread_mutex_lock(&circular_mutex);
            circular_write(capture_buffer, buffer);
#if 0
            printf("capture_buffer len %i\n",
                    capture_buffer->start >= capture_buffer->first ?
                        capture_buffer->start - capture_buffer->first :
                        capture_buffer->end - capture_buffer->first +
                        capture_buffer->start - capture_buffer->buffer);
#endif
            pthread_mutex_unlock(&circular_mutex);
        }
    }

    free(buffer);
    return NULL;
}

static void *playback(void *arg)
{
    int const delay_packet_count   = 32;
    int                n           = 0;
    int                sleep_start = 0;
    char               zero[period_bytes];
    snd_pcm_sframes_t  r;

    memset(zero, 0, sizeof(zero));

    while (1)
    {
        if (playback_start)
        {
            pthread_mutex_lock(&circular_mutex);
            if (data_chunk_size(_playback_buffer) >= _period_bytes_pb)
            {
                data_chunk_popfront(_playback_buffer, (unsigned char *)_tmp_buf_pb, _period_bytes_pb);
                r = snd_pcm_writei(playback_handle,
                                   _tmp_buf_pb,
                                   _period_frames_pb);
                if (r == -EPIPE)
                {
                    fprintf(stderr, "underrun occurred\n");

                    underrun_occurred = 1;
                    aec_start         = 0;
                    playback_start    = 0;

                    // let receive() do the sound_stop_talk() thing
                    receive_thread_exit = 1;
                    pthread_mutex_unlock(&circular_mutex);
                    continue;
                }
                else if (r < 0)
                {
                    fprintf(stderr, "snd_pcm_writei() failed: %s\n",
                            snd_strerror(r));
                }
                else if (r != _period_frames_pb)
                {
                    fprintf(stderr, "short write: %i -> %i\n",
                            (int) period_frames, (int) r);
                }
                else
                {
                    if (n < delay_packet_count)
                    {
                        n++;
                        if (n == AEC_DELAY)
                        {
                            aec_start = 1;
                            usleep(10000); // add some determinacy
                        }
                        else if (n == delay_packet_count)
                            sleep_start = 1;
                    }

                    printf("====> push echo buffer\n");
                    data_chunk_pushback(_echo_buffer,
                                        (unsigned char *)_tmp_buf_pb,
                                        _period_bytes_pb);
#if 0
                    printf("echo_buffer len %i, aec_start %i, n %i\n",
                        echo_buffer->start >= echo_buffer->first ?
                            echo_buffer->start - echo_buffer->first :
                            echo_buffer->end - echo_buffer->first +
                            echo_buffer->start - echo_buffer->buffer,
                        aec_start, n);
#endif
                }

                pthread_mutex_unlock(&circular_mutex);
                if (sleep_start)
                    usleep(100000); // crucial
                else
                    usleep(10);
            }
            else
            {
                pthread_mutex_unlock(&circular_mutex);
                usleep(100000);
            }
        }
        else
        {
            n           = 0;
            sleep_start = 0;
            usleep(100000);
        }
    }

    return NULL;
}

static inline void amr_encode(CHP_U32 handle, CHP_AUD_ENC_DATA_T *data)
{
    amrnb_encode(handle, data);

    if (underrun_occurred)
    {
        underrun_occurred = 0;
        memset(amr_buf.cache, 0, AMR_PERIOD_BYTES);
    }

    data->used_size = 0;

    pthread_rwlock_wrlock(&amr_buf.lock);
    memcpy(amr_buf.periods[amr_buf.end].p, amr_buf.cache,
           AMR_PERIOD_BYTES);
    memset(amr_buf.periods[amr_buf.end].clean, 0,
           sizeof(amr_buf.periods[amr_buf.end].clean));

    amr_buf.end = (amr_buf.end + 1) % AMR_PERIODS;
    if (amr_buf.end == amr_buf.start)
        amr_buf.start = (amr_buf.start + 1) % AMR_PERIODS;
    pthread_rwlock_unlock(&amr_buf.lock);
}

static void *aec(void *arg)
{
    CHP_MEM_FUNC_T     func;
    CHP_AUD_ENC_INFO_T info;
    CHP_AUD_ENC_DATA_T data;
    CHP_U32            handle;

    func.chp_malloc   = (CHP_MALLOC_FUNC)malloc;
    func.chp_free     = (CHP_FREE_FUNC)free;
    func.chp_memset   = (CHP_MEMSET)memset;
    func.chp_memcpy   = (CHP_MEMCPY)memcpy;
    info.audio_type   = CHP_DRI_CODEC_AMRNB;
    info.bit_rate     = 12200;

    amrnb_encoder_init(&func, &info, &handle);

    data.p_in_buf     = malloc(period_bytes);
    data.p_out_buf    = amr_buf.cache;
    data.frame_cnt    = 1;
    data.in_buf_len   = period_bytes;
    data.out_buf_len  = AMR_PERIOD_BYTES;
    data.used_size    = 0;
    data.enc_data_len = 0;

    char *echo_buf = malloc(period_bytes);

    while (1)
    {
        if (aec_start)
        {
            pthread_mutex_lock(&circular_mutex);
#if 0
            if (!circular_empty(capture_buffer) &&
                !circular_empty(echo_buffer))
#endif
            if (!circular_empty(capture_buffer) &&
                data_chunk_size(_echo_buffer) >= period_bytes)
            {
                data_chunk_popfront(_echo_buffer, (unsigned char *)echo_buf, period_bytes);
#if 1
                speex_echo_cancellation(echo_state,
                    (spx_int16_t *)capture_buffer->first,
                    //(spx_int16_t *)echo_buffer->first,
                    (spx_int16_t *)echo_buf,
                    (spx_int16_t *)data.p_in_buf);
#ifdef SOUND_ENABLE_AEC_PREPROCESS
                speex_preprocess_run(echo_pp,
                    (spx_int16_t *)data.p_in_buf);
#endif
#else
                memcpy(data.p_in_buf, capture_buffer->first,
                       period_bytes);
#endif
                circular_consume(capture_buffer);
                pthread_mutex_unlock(&circular_mutex);

                amr_encode(handle, &data);
            }
            else
            {
                pthread_mutex_unlock(&circular_mutex);
                usleep(5000);
            }
        }
        else
        {
            pthread_mutex_lock(&circular_mutex);
            if (!circular_empty(capture_buffer))
            {
                memcpy(data.p_in_buf, capture_buffer->first,
                       period_bytes);
                circular_consume(capture_buffer);
                pthread_mutex_unlock(&circular_mutex);

                amr_encode(handle, &data);
            }
            else
            {
                pthread_mutex_unlock(&circular_mutex);
                usleep(5000);
            }
        }
    }

    return NULL;
}

static snd_pcm_t *handle_init(snd_pcm_stream_t stream)
{
    snd_pcm_t           *handle = NULL;
    snd_pcm_hw_params_t *params = NULL;
    unsigned int         rate   = 8000;
    int                  r;
    snd_pcm_uframes_t    buffer_frames;
    unsigned int         buffer_time;
    unsigned int         period_time;

    if (stream == SND_PCM_STREAM_PLAYBACK)
        printf("@ playback stream:\n");
    else if (stream == SND_PCM_STREAM_CAPTURE)
        printf("@ capture stream:\n");

#define err() do {         \
    snd_pcm_close(handle); \
    handle = NULL;         \
    goto end;              \
} while (0)

    if ((r = snd_pcm_open(&handle, "plughw:0,0", stream, 0)) < 0)
    {
        handle = NULL;
        goto end;
    }

    if ((r = snd_pcm_hw_params_malloc(&params)) < 0)
        err();

    if ((r = snd_pcm_hw_params_any(handle, params)) < 0)
        err();

    if ((r = snd_pcm_hw_params_set_access(handle, params,
                 SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        err();

    if ((r = snd_pcm_hw_params_set_format(handle, params,
                 SND_PCM_FORMAT_S16_LE)) < 0)
        err();

    if ((r = snd_pcm_hw_params_set_rate_near(handle, params,
                 &rate, NULL)) < 0)
        err();

    if ((r = snd_pcm_hw_params_set_channels(handle, params, 1)) < 0)
        err();

    // get parameters

    // If we were to xxx_get_period_size() here, it would return negative
    // error code due to "the configuration space does not contain a
    // single value". However, if we call xxx_get_period_size() after
    // snd_pcm_hw_params(), we can get the actual value set into the
    // handler. The same is true for other 3 functions that follow.
    if ((r = snd_pcm_hw_params_get_period_size_min(params,
                 &period_frames, NULL)) < 0)
        err();

    printf("==> min period frames: %li\n", period_frames);

    if ((r = snd_pcm_hw_params_get_period_size_max(params,
                 &period_frames, NULL)) < 0)
        err();

    printf("==> max period frames: %li\n", period_frames);

    if ((r = snd_pcm_hw_params_get_buffer_size_min(params,
                 &buffer_frames)) < 0)
        err();

    printf("==> min buffer frames: %li\n", buffer_frames);

    if ((r = snd_pcm_hw_params_get_buffer_size_max(params,
                 &buffer_frames)) < 0)
        err();

    printf("==> max buffer frames: %li\n", buffer_frames);

    // It is recommended to use a frame size in the order of 20 ms
    // (or equal to the codec frame size) and make sure it is easy
    // to perform an FFT of that size (powers of two are better than
    // prime sizes).
    period_frames = PERIOD_FRAMES;
    snd_pcm_hw_params_set_period_size(handle, params, period_frames, 0);

    // the max buffer frames
    snd_pcm_hw_params_set_buffer_size(handle, params, buffer_frames);

    // After this call, snd_pcm_prepare() is called automatically and
    // the stream is brought to SND_PCM_STATE_PREPARED state.
    if ((r = snd_pcm_hw_params(handle, params)) < 0)
        err();

    if (stream == SND_PCM_STREAM_CAPTURE)
    {
        // period is a group of frames
        snd_pcm_hw_params_get_period_size(params, &period_frames, NULL);
        snd_pcm_hw_params_get_period_time(params, &period_time, NULL);
        printf("period_frames: %li, period_time %i us\n",
               period_frames, period_time);

        snd_pcm_hw_params_get_buffer_size(params, &buffer_frames);
        snd_pcm_hw_params_get_buffer_time(params, &buffer_time, NULL);
        printf("buffer_frames: %li, buffer_time %i us\n",
               buffer_frames, buffer_time);

        snd_pcm_hw_params_get_rate(params, &rate, NULL);
        printf("capture rate: %i\n", rate);

        period_bytes    = period_frames * 2;
        circular_size   = period_bytes * 1024; // 320k buffer

        capture_buffer  = circular_init(circular_size, period_bytes);

        _playback_buffer = data_chunk_new(circular_size);
        _echo_buffer = data_chunk_new(circular_size);
    }
    else if (stream == SND_PCM_STREAM_PLAYBACK)
    {
        snd_pcm_hw_params_get_period_size(params, &_period_frames_pb, NULL);
        printf("playback period frames: %i\n", _period_frames_pb);
        _period_bytes_pb = _period_frames_pb * 2;
        _tmp_buf_pb = malloc(_period_bytes_pb);

        snd_pcm_hw_params_get_rate(params, &rate, NULL);
        printf("playback rate: %i\n", rate);
    }

#undef err

end:
    if (params)
        snd_pcm_hw_params_free(params);
    return handle;
}

int sound_init()
{
    int sample_rate = 8000;

    if ((playback_handle = handle_init(SND_PCM_STREAM_PLAYBACK)) == NULL)
    {
        fprintf(stderr, "failed to init playback handle\n");
        goto end;
    }

    if ((capture_handle = handle_init(SND_PCM_STREAM_CAPTURE)) == NULL)
    {
        fprintf(stderr, "failed to init capture handle\n");
        goto end;
    }

    speaker_on();
    alsa_set_mic_volume(70, 1);
    alsa_set_hp_volume(0, 0);
    init_amrdecoder();
    sound_amr_buffer_init();
    pthread_mutex_init(&circular_mutex, NULL);

    echo_state = speex_echo_state_init_mc(period_frames,
                                          period_frames * 10, 1, 1);
    speex_echo_ctl(echo_state,
        SPEEX_ECHO_SET_SAMPLING_RATE, &sample_rate);

#ifdef SOUND_ENABLE_AEC_PREPROCESS
    echo_pp = speex_preprocess_state_init(period_frames, sample_rate);
    speex_preprocess_ctl(echo_pp,
        SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);
#endif

    return 0;

end:
    if (playback_handle)
        snd_pcm_close(playback_handle);
    if (capture_handle)
        snd_pcm_close(capture_handle);

    circular_free(capture_buffer);

    data_chunk_free(_playback_buffer);
    data_chunk_free(_echo_buffer);
    SAFE_FREE(_tmp_buf_pb);

    speex_echo_state_destroy(echo_state);
#ifdef SOUND_ENABLE_AEC_PREPROCESS
    speex_preprocess_state_destroy(echo_pp);
#endif
    pthread_mutex_destroy(&circular_mutex);

    return -1;
}

void sound_start_thread(void)
{
    pthread_t thread;
    pthread_create(&thread, NULL, capture, NULL);
    pthread_detach(thread);
    pthread_create(&thread, NULL, playback, NULL);
    pthread_detach(thread);
    pthread_create(&thread, NULL, aec, NULL);
    pthread_detach(thread);
}

void *sound_start_session(void *arg)
{
    struct sess_ctx *sess     = arg;
    int              sock     = sess->sc->audio_socket;
    ssize_t          size, s, n;
    char            *buf;

    // FIXME: no need to lock here?
    sess->ucount++;
    sound_amr_buffer_clean(sess->id);

    while (1)
    {
        buf = sound_amr_buffer_fetch(sess->id, &size);
        if (!buf)
        {
            usleep(50000);

            if (!sess->running)
                break;
            else
                continue;
        }

        s = 0;
        while (size > 0)
        {
            if (!sess->running)
            {
                free(buf);
                goto end;
            }

            if (size > 1024)
            {
                if (sess->is_tcp)
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
                s    += n;
                size -= n;
            }
            else
            {
                free(buf);
                goto end;
            }
        }
        free(buf);
    }

end:
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

int sound_start_talk(struct sess_ctx *sess)
{
    if (sound_talking)
        return -1;
    if (session_number > 1)
        return -2;

    snd_pcm_prepare(playback_handle);
    sound_talking          = 1;
    receive_thread_exit    = 0;
    receive_thread_running = 1;
    pthread_create(&receive_thread, NULL, receive, sess);

    return 0;
}

void sound_stop_talk()
{
    if (receive_thread_running)
    {
        alsa_set_hp_volume(0, 0);

        aec_start      = 0;
        playback_start = 0;

        receive_thread_exit = 1;
        usleep(20000);
        pthread_join(receive_thread, NULL);
        receive_thread_running = 0;

        pthread_mutex_lock(&circular_mutex);
        snd_pcm_drop(playback_handle);
        pthread_mutex_unlock(&circular_mutex);

        sound_talking  = 0;
    }
}

