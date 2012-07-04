#ifndef __INET_TYPE_H__
#define __INET_TYPE_H__
#include<sys/time.h>
#include<time.h>
#include <pthread.h>


#ifdef __cplusplus
extern "C"
{
#endif


#ifndef uint32_t
typedef unsigned int  uint32_t;
#endif

#ifndef int32_t
typedef int  int32_t;
#endif

#ifndef uint16_t
typedef unsigned short  uint16_t;
#endif

#define CAMERA_WATCH_PORT   4000  //发送camera id端口
#define CLIENT_WATCH_PORT   4200  //发送monitor id 端口
#define INTERACTIVE_PORT    4300  //命令交互端口

#define SERVER_IP        "192.168.1.1"
enum
{
    /** 
     * 提示对方接收到的数据不完整， 或其他错误
     */
    NET_CAMERA_FAILED,

    /** 
     * 提示接收方受到完整数据
     */
    NET_CAMERA_OK,

    /** 
     * 客户端发送给服务器的命令
     * 客户端收到NET_CAMERA_SEND_PORTS 后，在用到的端口创建socket
     * 向服务器发送这个命令
     */
    NET_CAMERA_PORT,    

    /** 
     * 服务器发送给客户端的命令
     * 要求客户端发送 将用到的端口都发送一个NET_CAMERA_PORT 命令过来
     */
    NET_CAMERA_SEND_PORTS,   

    /** 
     * 客户端发送给服务器的命令
     * camera 发送注册id， monitor发送要求链接到指定id的camera
     */
    NET_CAMERA_ID,    
    /**
     * 服务器发送给客户端的命令.
     * 说明这个数据包是你要链接的远程ip 和 端口
     * 数据格式 byte1 : NET_CAMERA_PEER_PORTS; byte2,3: size
     * byte3，size-2: 远程的ip和所有端口号
     */
    NET_CAMERA_PEER_PORTS,
    NET_CAMERA_INTERACTIVE_PORT,
} ;

#define PORT_COUNT		 3
#define NAT_CLI_PORT	 0
#define NAT_V_PORT		 1
#define NAT_A_PORT		 2
struct udp_transfer{
	uint32_t ip;
	uint16_t port[PORT_COUNT];
}__attribute__((packed));

/*use as local management*/
struct mapping{
	struct timeval aged;
	pthread_mutex_t mapping_lock;
	int ucount;
	int connected;
	int udpsock[PORT_COUNT];
	int udtsock[PORT_COUNT];
	int local_port[PORT_COUNT];
	uint32_t ip;
	uint16_t dst_port[PORT_COUNT];
	struct sess_ctx *sess;
	struct mapping *next;
};
/*use to get our public addr*/
struct public_port{
	uint32_t id;
	uint32_t ip;
	uint16_t port;
}__attribute__((packed));

/*
* id :the cmd id
* who : <0 is the server otherwise the id of monitor
*/
 typedef struct __cmd
{
	uint32_t id;
	int32_t who;
}__attribute__((packed)) cmd_t;
/*
* this structure used to record who have occupy the local playback port
* who :the monitor id  who occupy the playback port
*local_pb_port  the local playback port
*monitor_pb_port and monitor_ip is the monitor's public port in NAT
*if the structure is in the keep alive list,a thread would use the local
*port call the monitor every one minute to keep the port alive 
*the data that use to call  is not important
*/
struct pb_port_connet_management{
	int32_t who;
	uint16_t local_pb_port;
	uint32_t monitor_ip;
	uint16_t monitor_pb_port;
	struct pb_port_connet_management *next;
};

#ifdef __cplusplus
}
#endif

#endif

