#include "mediaEncode.h"
#include <akmedialib/media_recorder_lib.h>
#include <alsa/asoundlib.h>


snd_pcm_t *g_hSndPcm;
snd_pcm_hw_params_t *g_rec_params;
snd_pcm_uframes_t g_frames;
unsigned char g_bWavBuf[1024*2];

int openSndPcm(void)
{
    int rc, dir;
    unsigned int val;

    // Open PCM device for recording (capture).
    rc = snd_pcm_open(&g_hSndPcm, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        printf("unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    // Allocate a hardware parameters object. 
    snd_pcm_hw_params_alloca(&g_rec_params);

    // Fill it in with default values.
    snd_pcm_hw_params_any(g_hSndPcm, g_rec_params);

    // Set the desired hardware parameters.
    // Interleaved mode
    snd_pcm_hw_params_set_access(g_hSndPcm, g_rec_params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // Signed 16-bit little-endian format
    snd_pcm_hw_params_set_format(g_hSndPcm, g_rec_params, SND_PCM_FORMAT_S16_LE);

    // Two channels (stereo)
    snd_pcm_hw_params_set_channels(g_hSndPcm, g_rec_params, 1);

    // 44100 bits/second sampling rate (CD quality)
    val = 8000;
    snd_pcm_hw_params_set_rate_near(g_hSndPcm, g_rec_params, &val, &dir);

    // Set period size to 32 frames
    g_frames = 32;
    snd_pcm_hw_params_set_period_size_near(g_hSndPcm, g_rec_params, &g_frames, &dir);

    // Write the parameters to the driver
    rc = snd_pcm_hw_params(g_hSndPcm, g_rec_params);
    if (rc < 0)  {
        printf("unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    // Use a g_pWavBuf large enough to hold one period
    snd_pcm_hw_params_get_period_size(g_rec_params,  &g_frames, &dir);

    // We want to loop for 5 seconds
    snd_pcm_hw_params_get_period_time(g_rec_params, &val, &dir);

	printf("Opening alsa_snd_pcm ok\n");
}

int closeSndPcm(void)
{
	snd_pcm_drain(g_hSndPcm);
	snd_pcm_close(g_hSndPcm);
}

int getSndPcmData(T_U8 **pAudioData, T_U32 *pulAudioSize)
{
	int rc;

    rc = snd_pcm_readi(g_hSndPcm, g_bWavBuf, g_frames);
	if (rc == -EPIPE) {
		// EPIPE means overrun
		printf("overrun occurred\n");
		snd_pcm_prepare(g_hSndPcm);
		return -1;
	}
	else if (rc < 0)  {
		printf("error from read: %s\n", snd_strerror(rc));
		return -1;
	}

	*pulAudioSize = g_frames * 2;
	*pAudioData = &g_bWavBuf;

	return 0;
}

//    wav_stop_write(fp_rec, &wavheader, total_len);
