#ifndef HEADERS_H
#define HEADERS_H

/* turn off assert debugging */
#define NDEBUG

/* standard C headers */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define HAVE_POSIX_THREAD

/* required on AIX for FD_SET (requires bzero).
 * often this is the same as <string.h> */
#ifdef HAVE_STRINGS_H
    #include <strings.h>
#endif // HAVE_STRINGS_H

/* unix headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include <syslog.h>

#ifdef HAVE_POSIX_THREAD
    #include <pthread.h>
#endif // HAVE_POSIX_THREAD

#include "anyka_types.h"

#ifndef UNUSED
#define UNUSED(x)	((void)(x))	/* to avoid warnings */
#endif

//#define MAKE_FAKE_TIME_STAMP

#define bzero(a, b) memset((a), 0, (b))

#endif /* HEADERS_H */

