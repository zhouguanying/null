/*
 * Copyright 2004-2008 Freescale Semiconductor, Inc. All Rights Reserved.
 * 
 * Author Erik Anvik "Au-Zone Technologies, Inc."  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU Lesser General 
 * Public License.  You may obtain a copy of the GNU Lesser General 
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>

#include "includes.h"
#include "defines.h"

/* Debug */
#define SOCK_DBG
#ifdef SOCK_DBG
extern char *v2ipd_logfile;
#define dbg(fmt, args...)  \
    do { \
        FILE *f = fopen(v2ipd_logfile, "a+");                      \
        fprintf(f, __FILE__ ": %s: " fmt, __func__, ## args); \
        fclose(f);                                                 \
    } while (0)
#else
#define dbg(fmt, args...) do {} while (0)
#endif /* SOCK_DBG */

/**
 * socket_ctx - socket context data structure
 * @type: socket type SOCK_DGRAM or SOCK_STREAM
 * @sock: socket number
 * @to: destination socket address for INET or UNIX types
 * @from: source socket address for INET or UNIX types
 */
struct socket_ctx {
    int type;
    int sock;
    union {
        struct sockaddr_in in;
        struct sockaddr_un un;
    } to, from;
}; 
extern char curr_device[32];

static inline struct sockaddr_in * create_in_addr(u16 port)
{
    struct sockaddr_in *s;

    if ((s = malloc(sizeof(*s))) == NULL)
        return NULL;    
    memset(s, 0, sizeof(*s));

    s->sin_family = AF_INET;
    s->sin_addr.s_addr = INADDR_ANY;
    s->sin_port = htons(port);

    return s;
}

static inline struct sockaddr_un * create_un_addr(char *path)
{
    struct sockaddr_un *s;

    if (path == NULL || (s = malloc(sizeof(*s))) == NULL)
        return NULL;    
    memset(s, 0, sizeof(*s));

    s->sun_family = AF_UNIX;
    strcpy(s->sun_path, path);

    return s;
}

static inline struct sockaddr_in * bind_in_socket(int sock, u16 port)
{
    struct sockaddr_in *s;
    struct ifreq ifr;

    if (sock < 0 || (s = create_in_addr(port)) == NULL)
        return NULL;
	
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, curr_device, IFNAMSIZ-1);
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr)) == -1){
	printf("unable to bind device %s\n",curr_device);
	free(s);
	return NULL;
    }
    
    if (bind(sock, (struct sockaddr *) s, sizeof(*s)) < 0) {
        free(s);
        return NULL;
    } else
        return s;
}

static inline struct sockaddr_un * bind_un_socket(int sock, char *path)
{
    struct sockaddr_un *s;
    int slen;

    if (sock < 0 || path == NULL || (s = create_un_addr(path)) == NULL)
        return NULL;    

    slen = strlen(s->sun_path) + sizeof(s->sun_family);
    if (bind(sock, (struct sockaddr *) s, slen) < 0) {
        free(s);
        return NULL;
    } else
        return s;
}

/**
 * create_udp_socket - create a UDP based socket
 * Returns socket context on success or -1 on error
 */
#if 0
struct socket_ctx * create_udp_socket(void)
{
    struct socket_ctx *s;

    if ((s = malloc(sizeof(*s))) == NULL)
        return NULL;
    memset(s, 0, sizeof(*s));
    
    /* Create UDP (Datagram Socket) */
    if ((s->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error udp creating socket");
        return NULL;
    } else
        return s;
}
#endif
int create_udp_socket(void)
{
    int sock;

    /* Create UDP (Datagram Socket) */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error udp creating socket");
        return -1;
    } else
        return sock;
}

/**
 * create_tcp_socket - create a TCP based socket
 * Returns socket number on success or -1 on error
 */
int create_tcp_socket(void)
{
    int sock;

    /* Create TCP (Stream Socket) */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error tcp creating socket");
        return -1;
    } else
        return sock;
}

/**
 * create_unix_socket - create a UNIX domain based socket
 * Returns socket number on success or -1 on error
 */
int create_unix_socket(void)
{
    int sock;

    /* Create Unix Domain (Stream Socket) */
    if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("Error unix creating socket");
        return -1;
    } else
        return sock;
}

/**
 * create_inet_addr - create inet address
 * @port: port to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_in * create_inet_addr(u16 port)
{
   return create_in_addr(port); 
}

/**
 * create_unix_addr - create unix address
 * @sock: socket file descriptor
 * @path: path to file to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_un * create_unix_addr(char *path)
{
   return create_un_addr(path); 
}

/**
 * bind_tcp_socket - bind address to port
 * @sock: socket file descriptor
 * @port: port to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_in * bind_tcp_socket(int sock, u16 port)
{
    return bind_in_socket(sock, port);
}

/**
 * bind_udp_socket - bind address to port
 * @sock: socket file descriptor
 * @port: port to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_in * bind_udp_socket(int sock, u16 port)
{
    return bind_in_socket(sock, port);
}

/**
 * send_to_socket - send data to a socket
 * @sock_ctx: socket context
 * Returns number of bytes sent on success or -1 on error
 */
int send_to_socket(struct socket_ctx *sock_ctx, void *payload, size_t len)
{
    if (sock_ctx == NULL || payload == NULL) return 0;
    
    switch (sock_ctx->type) {
    case SOCK_STREAM:
        return send(sock_ctx->sock, payload, len, 0); 
    case SOCK_DGRAM:
    default:  
        return sendto(sock_ctx->sock, payload, len, 0,  
                      (struct sockaddr *) &sock_ctx->to.in,
                      sizeof(sock_ctx->to.in)); 
    }
}

/**
 * send_to_socket - send data to a socket
 * @sock: socket context
 * Returns socket address on success or NULL on error
 */
int recv_from_socket(struct socket_ctx *sock_ctx, void *payload,
                            size_t len)
{
#if 0    
    if (sock_ctx == NULL) return 0;
    
    switch (sock_ctx->type) {
    case STREAM:
        return send(sock_ctx->sock, payload, len); 
    case DATAGRAM:
    default:  
        return sendto(sock_ctx->sock, payload, len, 0,  
                      (struct sockaddr *) sock_ctx->to, sizeof(*sess->to)); 
    }
#endif
    return -1;
}
    

