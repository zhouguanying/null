#ifndef __CUDT_H__
#define __CUDT_H__
#ifdef __cplusplus
extern "C"
{
#endif
int udt_sess_thread(void *arg);
int udt_do_update(void *arg);
void start_udt_lib();
void stop_udt_lib();
#ifdef __cplusplus
}
#endif
#endif



