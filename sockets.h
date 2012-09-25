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

#ifndef SOCKETS_H
#define SOCKETS_H

/* Forward declarations */
struct sockaddr_un;
struct sockaddr_in;
struct socket_ctx;

/**
 * create_udp_socket - create a UDP based socket
 * Returns socket number on success or -1 on error
 */
int create_udp_socket(void);

/**
 * create_tcp_socket - create a TCP based socket
 * Returns socket number on success or -1 on error
 */
int create_tcp_socket(void);

/**
 * create_unix_socket - create a UNIX domain based socket
 * Returns socket number on success or -1 on error
 */
int create_unix_socket(void);

/**
 * create_inet_addr - create inet address
 * @port: port to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_in * create_inet_addr(u16 port);

/**
 * create_unix_addr - create unix address
 * @path: path to file to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_un * create_unix_addr(char *path);

/**
 * bind_tcp_socket - bind address to port
 * @sock: socket file descriptor
 * @port: port to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_in * bind_tcp_socket(int sock, u16 port);

/**
 * bind_udp_socket - bind address to port
 * @sock: socket file descriptor
 * @port: port to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_in * bind_udp_socket(int sock, u16 port);

/**
 * bind_unix_socket - bind unix address
 * @sock: socket file descriptor
 * @path: path to file to listen on or send data over
 * Returns socket address on success or NULL on error
 */
struct sockaddr_un * bind_unix_socket(int sock, char *path);

/**
 * send_to_socket - send data to a socket
 * @sock_ctx: socket context
 * Returns number of bytes sent on success or -1 on error
 */
int send_to_socket(struct socket_ctx *sock_ctx, void *payload, size_t len);

#endif /* SOCKETS_H */

