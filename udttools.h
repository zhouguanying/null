#ifndef __UDTTOOLS_H__
#define __UDTTOOLS_H__
#ifdef __cplusplus
extern "C"
{
#endif
#define UDT_SELECT_TIMEOUT 		  -1
#define UDT_SELECT_ERROR	 		  -2
#define UDT_SELECT_UNKONW_ERROR     -3
    void start_udt_lib();
    int udt_socket(int af ,int type , int protocol);
    int udt_bind_in_udp(int udtsocket , int udpsocket);
    int udt_bind_in_addr(int udtsocket , struct sockaddr *inaddr);
    int udt_listen(int udtsocket , int len);
    int udt_accept(int lsocket , struct sockaddr *addr  , int * addrlen);
    int udt_send(int udtsocket ,int sock_t, char *buf , int len);
    int udt_recv(int udtsocket , int sock_t , char *buf , int len , struct sockaddr *from , int *fromlen);
    int udt_get_readable_socket(int *sockfds , int sockfdnums , const struct timeval *tv);
    int udt_socket_ok(int sockfd);
    void udt_close(int sockfd);

#ifdef __cplusplus
}
#endif
#endif
