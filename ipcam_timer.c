#include "ipcam_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

enum 
{
    TIMER_STATE_UNSET = 0,
    TIMER_STATE_INITD,
    TIMER_STATE_CREATED,
};

long inline static
timeval_diff_ms( struct timeval *end, struct timeval *start)
{
    long diff;
    
    diff = (end->tv_sec - start->tv_sec) * 1000;
    diff += (end->tv_usec - start->tv_usec) / 1000; 
    return diff;
}

ipcam_timer_t *ipcam_timer_create(long timeout_ms)
{
    ipcam_timer_t *t = malloc(sizeof(ipcam_timer_t));
    gettimeofday( &t->start, 0);
    t->timeout_ms = timeout_ms;
    t->state = TIMER_STATE_CREATED;

    return t;
}

void ipcam_timer_init(ipcam_timer_t *t, long timeout_ms)
{
    gettimeofday( &t->start, 0);
    t->timeout_ms = timeout_ms;
    t->state = TIMER_STATE_INITD;
}

int ipcam_timer_timeout(ipcam_timer_t *t)
{
    assert(t->state == TIMER_STATE_CREATED ||
           t->state == TIMER_STATE_INITD);
    struct timeval now;

    gettimeofday(&now, 0);
    if (abs(timeval_diff_ms(&now, &t->start)) >= t->timeout_ms) {
        t->start = now;
        return 1;
    }

    return 0;
}

void ipcam_timer_restart(ipcam_timer_t *t)
{
    assert(t->state == TIMER_STATE_CREATED ||
           t->state == TIMER_STATE_INITD);
    gettimeofday(&t->start, 0);
}

void ipcam_timer_destroy(ipcam_timer_t *t)
{
    assert(t->state == TIMER_STATE_CREATED);
    free(t);
}

void ipcam_timer_reset(ipcam_timer_t *t, long timeout)
{
    //gettimeofday(&t->start, 0);
    t->timeout_ms = timeout;
}

