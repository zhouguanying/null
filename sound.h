#ifndef __SOUND__H__
#define __SOUND__H__

#ifdef __cplusplus
extern "C"
{
#endif

void  sound_amr_buffer_init();
void  sound_amr_buffer_reset();
void  sound_amr_buffer_clean(int sess_id);
char *sound_amr_buffer_fetch(int sess_id, int *size);

int   sound_init();
void  sound_start_thread();
void *sound_start_session(void *arg);

int   sound_start_talk(struct sess_ctx *sess);
void  sound_stop_talk();

#ifdef __cplusplus
}
#endif

#endif

