#ifndef _IPCAM_TIMER_H_
#define _IPCAM_TIMER_H_
#include <sys/time.h>

typedef struct timer
{
    struct timeval  start;
    long            timeout_ms;
    int             state;
} ipcam_timer_t;

ipcam_timer_t * ipcam_timer_create(long timeout_ms);
void            ipcam_timer_init(ipcam_timer_t *t, long timeout_ms);
int             ipcam_timer_timeout(ipcam_timer_t *t);
void            ipcam_timer_restart(ipcam_timer_t *t);
void            ipcam_timer_reset(ipcam_timer_t *t, long timeout);

#endif
