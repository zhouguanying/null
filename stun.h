#ifndef __STUN_H__
#define __STUN_H__

#include <netdb.h>

#ifndef WIN32
#ifdef __cplusplus
extern "C" {
#endif
#endif

#define STUN_BUFSIZE      1024
#define STUN_SERVER_0     "udp.iped.com.cn"
#define STUN_SERVER_1     "kejialiu.net"
#define STUN_STN_PORT     "3478"
#define STUN_CAM_PORT     "6000"

#ifdef WIN32
#include <udt.h>
#include "F:\Test_Surveillance\ffplay\common.h"
#define __func__ ""
    typedef int socklen_t;
#endif

#ifndef UDTSOCKET
    typedef int UDTSOCKET;
#endif

    enum
    {
        STUN_SOCK_AUTO = 0,
        STUN_SOCK_TCP,
        STUN_SOCK_UDT,
    };

    enum
    {
        STUN_ERR_INTERNAL   = -1, // internal error
        STUN_ERR_SERVER     = -2, // server can't be reached (not in WAN)
        STUN_ERR_TIMEOUT    = -3, // timeout (camera unreachable)
        STUN_ERR_INVALIDID  = -4, // invalid camera id (not registered)
        STUN_ERR_CONNECT    = -5, // connect failure (a.k.a STUN failure)
        STUN_ERR_BIND       = -6, // bind failure (UDT on UDP)
    };

// ------ for monitor
#ifdef WIN32
    typedef  CRITICAL_SECTION stun_lock_t;
#else
    typedef  pthread_mutex_t  stun_lock_t;
#endif

    void stun_lock_init(stun_lock_t *lock);
    void stun_lock_destory(stun_lock_t *lock);
    void stun_lock_enter(stun_lock_t *lock);
    void stun_lock_leave(stun_lock_t *lock);

    typedef struct _StunSocket
    {
        int sock; // fd
        int type; // type, TCP or UDT
        int lan;  // 1 for in LAN, 0 for in WAN
    } StunSocket;

// camera IDs in LAN
    extern char             cam_lan_id[64][32];
    extern stun_lock_t      cam_search_mutex;

// pass (NULL, NULL, NULL); cam_lan_id will be cleaned each time
    int              monitor_search_lan(const char         *id,
                                        struct sockaddr_in *addr_in,
                                        socklen_t          *addrlen,
                                        char               *tcp_port ,
                                        char               *udp_port);
    void             monitor_search_stop(void);

// call monitor_local_addr() before consecutive calls to monitor_socket()
    void             monitor_local_addr(void);
    StunSocket      *monitor_socket(const char         *id,
                                    int                 sock_type);

// ------ for camera

    void            *stun_camera(void               *id);

// ------ for both
    int              stun_sendmsg(int                 sock,
                                  char               *buf,
                                  int                 len);
    int              stun_recvmsg(int                 sock,
                                  char               *buf,
                                  int                 len,
                                  struct sockaddr    *addr,
                                  int                *addrlen);
    int              stun_tcp_sendmsg(int                 sock,
                                      char               *buf,
                                      int                 len);
    int              stun_tcp_recvmsg(int                 sock,
                                      char               *buf,
                                      int                 len,
                                      struct sockaddr    *addr,
                                      int                *addrlen);
    int              stun_udt_sendn(int                 sock,
                                    char               *buf,
                                    int                 n);
    int              stun_udt_recvn(int                 sock,
                                    char               *buf,
                                    int                 n);
    int              stun_tcp_sendn(int                 sock,
                                    char               *buf,
                                    int                 n);
    int              stun_tcp_recvn(int                 sock,
                                    char               *buf,
                                    int                 n);

// ------ private

    int              stun_socket(const char         *host,
                                 const char         *port,
                                 struct addrinfo   **ai,
                                 struct addrinfo   **p,
                                 int                 passive);
    UDTSOCKET        stun_udt_socket(int                 udp_sock,
                                     const char         *addr,
                                     const char         *port);
    void            *stun_nat_test(void               *arg);
    int              stun_sendto(int                 sock,
                                 const char         *msg,
                                 struct addrinfo    *p,
                                 char               *recv,
                                 struct sockaddr_in *addr,
                                 socklen_t          *addrlen,
                                 int                 addrcmp);
    int              stun_recvfrom(int                 sock,
                                   char               *buf,
                                   size_t              len,
                                   struct sockaddr_in *src_addr,
                                   socklen_t          *addrlen,
                                   int                 usec);
    int              stun_addrcmp(struct sockaddr_in *a1,
                                  struct sockaddr_in *a2);
    char *           stun_local_addr(int                 sock,
                                     struct addrinfo    *p);
    uint16_t         stun_local_port(int                 sock,
                                     struct addrinfo    *p);

// ------ utilities

    int              stun_close(int                 fd);
    void             stun_sleep_ms(unsigned int        ms);
    void             stun_thread_create(void * (*routine)(void *),
                                        void               *arg);

#ifdef WIN32
    struct timezone
    {
        int  tz_minuteswest; /* minutes W of Greenwich */
        int  tz_dsttime;     /* type of dst correction */
    };

    int              gettimeofday(struct timeval     *tv,
                                  struct timezone    *tz);
#endif

#ifndef WIN32
#ifdef __cplusplus
}
#endif
#endif

#endif
