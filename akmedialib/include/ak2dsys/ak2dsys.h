#ifndef _LIBSYS_
#define _LIBSYS_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum {
    PARTIAL_WAKE_LOCK = 1,  // the cpu stays on, but the screen is off
    FULL_WAKE_LOCK = 2      // the screen is also on
};

int acquire_wake_lock(int lock, const char* id);
int release_wake_lock(const char* id);
int get_battery_capacity(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //#ifndef _LIBSYS_

