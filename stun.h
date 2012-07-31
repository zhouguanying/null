#ifndef __STUN_H__
#define __STUN_H__

#ifdef __cplusplus
extern "C"
{
#endif
#define STUN_BUFSIZE 1024
typedef int UDTSOCKET;
enum
{
    STUN_SOCK_AUTO = 0,
    STUN_SOCK_TCP,
    STUN_SOCK_UDP,
    STUN_SOCK_UDT,
};

enum
{
    STUN_ERR_INTERNAL   = -1,
    STUN_ERR_MAXSESSION = -2,
    STUN_ERR_SERVER     = -3,
    STUN_ERR_TIMEOUT    = -4,
    STUN_ERR_INVALIDID  = -5,
    STUN_ERR_CONNECT    = -6,
};

// ----- public

int              monitor_socket     (const char         *id,
                                     const char         *cmd,
                                     int                 sock_type,
                                     int                *result_type);
void            *stun_camera        (void               *arg);

// ----- private

int              stun_socket        (const char         *host,
                                     const char         *port,
                                     struct addrinfo   **ai,
                                     struct addrinfo   **p,
                                     int                 passive);
UDTSOCKET        stun_udt_socket    (int                 udp_sock,
                                     const char         *addr,
                                     const char         *port);
int              stun_sendto        (int                 sock,
                                     const char         *msg,
                                     struct addrinfo    *p,
                                     char               *recv,
                                     int                 addrcmp);
int              stun_recvfrom      (int                 sock,
                                     char               *buf,
                                     size_t              len,
                                     struct sockaddr_in *src_addr,
                                     socklen_t          *addrlen,
                                     int                 usec);
int              stun_addrcmp       (struct sockaddr_in *a1,
                                     struct sockaddr_in *a2);
char *           stun_local_addr    (int                 sock,
                                     struct addrinfo    *p);
uint16_t         stun_local_port    (int                 sock,
                                     struct addrinfo    *p);
#ifdef __cplusplus
}
#endif
#endif

