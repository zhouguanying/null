#ifndef __STUN_H__
#define __STUN_H__


#ifndef WIN32
#ifdef __cplusplus
extern "C" {
#endif
#include <netdb.h>
#endif

#define STUN_BUFSIZE      4096
#define STUN_SERVER_0     "udp.iped.com.cn"
#define STUN_SERVER_1     "kejialiu.net"
#define STUN_REG_PORT     "6000"

#ifdef WIN32
#include <udt.h>
#include "F:\Test_Surveillance\ffplay\common.h"
#define __func__ ""
typedef int socklen_t;
#pragma comment(lib, "ws2_32.lib")
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
    STUN_ERR_INTERNAL   = -1,   // internal error
    STUN_ERR_SERVER     = -2,   // server can't be reached (not in WAN)
    STUN_ERR_TIMEOUT    = -3,   // timeout (camera unreachable)
    STUN_ERR_INVALIDID  = -4,   // invalid camera id (not registered)
    STUN_ERR_CONNECT    = -5,   // connect failure (a.k.a STUN failure)
    STUN_ERR_BIND       = -6,   // bind failure (UDT on UDP)
    STUN_ERR_UNKNOWN    = -100, // unknown error
};

#ifdef WIN32
typedef  CRITICAL_SECTION stun_lock_t;
#else
typedef  pthread_mutex_t  stun_lock_t;
#endif

typedef struct _StunSocket
{
    int sock; // fd
    int type; // type, TCP or UDT
    int lan;  // 1 for in LAN, 0 for in WAN
} StunSocket;

typedef struct _StunRelay
{
    char sid[32];
    char addr[32];
    char port[8];
    int  sock;
} StunRelay;

typedef struct _SocketFusion
{
    int         ref;
    int         channels;
    int         multiplexed;
    int         lan;
    stun_lock_t lock;

    struct
    {
        int sock;
        int relay;
    } tcp_sock[10];

    struct
    {
        int sock;
    } udt_sock[3];
} SocketFusion;

// ----- for monitor

extern char             cam_lan_id[64][32];
extern stun_lock_t      cam_search_mutex;

void             monitor_init       (void);
int              monitor_initialized(void);
void             monitor_quit       (void);
void             monitor_resume     (void);
int              monitor_search_lan (const char         *id,
                                     struct sockaddr_in *addr_in,
                                     socklen_t          *addrlen,
                                     char               *tcp_port);
void             monitor_search_stop(void);

int              monitor_socket_cva (const char         *id,
                                     StunSocket        **socks);
void             monitor_socket_cancel(void);

// ----- for camera

void            *stun_camera        (void               *id);
void             camera_set_port    (int                 port);
void             camera_sigchld     (int                 signal);

// ----- for both

extern char      stun_port[8];

int              stun_sendmsg       (int                 sock,
                                     const char         *buf,
                                     int                 len);
int              stun_recvmsg       (int                 sock,
                                     char               *buf,
                                     int                 len,
                                     struct sockaddr    *addr,
                                     int                *addrlen);
int              stun_tcp_sendmsg   (int                 sock,
                                     const char         *buf,
                                     int                 len);
int              stun_tcp_recvmsg   (int                 sock,
                                     char               *buf,
                                     int                 len,
                                     struct sockaddr    *addr,
                                     int                *addrlen);
int              stun_udt_sendn     (int                 sock,
                                     const char         *buf,
                                     int                 n);
int              stun_udt_recvn     (int                 sock,
                                     char               *buf,
                                     int                 n);
int              stun_tcp_sendn     (int                 sock,
                                     const char         *buf,
                                     int                 n);
int              stun_tcp_recvn     (int                 sock,
                                     char               *buf,
                                     int                 n);
int              stun_sendn         (StunSocket         *stun,
                                     const char         *buf,
                                     int                 len);
int              stun_socket_recv   (StunSocket         *stun,
                                     char               *buf,
                                     int                 len);

// ----- SocketFusion

SocketFusion *socket_fusion_new           (int           channels);
void          socket_fusion_ref           (SocketFusion *sf);
void          socket_fusion_unref         (SocketFusion *sf);
int           socket_fusion_add_socket    (SocketFusion *sf,
                                           int           sock,
                                           int           type,
                                           int           relay);
int           socket_fusion_send          (SocketFusion *sf,
                                           int           channel,
                                           const void   *buf,
                                           int           len);
int           socket_fusion_send_n        (SocketFusion *sf,
                                           int           channel,
                                           const void   *buf,
                                           int           n);
int           socket_fusion_recv          (SocketFusion *sf,
                                           int           channel,
                                           void         *buf,
                                           int           len);
int           socket_fusion_recv_n        (SocketFusion *sf,
                                           int           channel,
                                           void         *buf,
                                           int           n);
// ----- private

int              stun_socket        (const char         *host,
                                     const char         *port,
                                     struct addrinfo   **ai,
                                     int                 passive);
UDTSOCKET        stun_udt_socket    (int                 udp_sock,
                                     const char         *addr,
                                     const char         *port,
                                     const char         *who);
int              stun_sendto        (int                 sock,
                                     const char         *msg,
                                     struct addrinfo    *p,
                                     char               *recv,
                                     struct sockaddr_in *addr,
                                     socklen_t          *addrlen,
                                     int                 addrcmp);
int              stun_recvfrom      (int                 sock,
                                     char               *buf,
                                     size_t              len,
                                     struct sockaddr_in *src_addr,
                                     socklen_t          *addrlen,
                                     int                 usec);
int              stun_addrcmp       (struct sockaddr_in *a1,
                                     struct sockaddr_in *a2);
int              stun_addrcmp_without_port
                                    (struct sockaddr_in *a1,
                                     struct sockaddr_in *a2);
void             stun_port_scan     (int                 sock);
char *           stun_local_addr    (int                 sock,
                                     struct addrinfo    *p);
uint16_t         stun_local_port    (int                 sock,
                                     struct addrinfo    *p);

// ----- utilities

int              stun_close         (int                 fd);
void             stun_socket_free   (StunSocket         *s);
void             stun_sleep_ms      (unsigned int        ms);
void             stun_thread_create (void *(*routine)(void *),
                                     void               *arg);
int              stun_random_port   (void);
void             stun_parse         (char               *msg,
                                     char              **prv_addr,
                                     char              **prv_port,
                                     char              **pub_addr,
                                     char              **pub_port,
                                     char              **tcp_port,
                                     char              **sid,
                                     int                *tcp_appended);
int              stun_parse_relays  (char               *msg,
                                     StunRelay          *relays,
                                     int                 max);

void             stun_lock_init     (stun_lock_t        *lock);
void             stun_lock_destroy  (stun_lock_t        *lock);
void             stun_lock_enter    (stun_lock_t        *lock);
void             stun_lock_leave    (stun_lock_t        *lock);

#ifdef WIN32
struct timezone
{
    int  tz_minuteswest; /* minutes W of Greenwich */
    int  tz_dsttime;     /* type of dst correction */
};

int              gettimeofday       (struct timeval     *tv,
                                     struct timezone    *tz);
#endif

#ifndef WIN32
#ifdef __cplusplus
}
#endif
#endif

#endif
