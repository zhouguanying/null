#ifndef CLI_H
#define CLI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "server.h"

/**
 * cli_handler - cli handler structure
 * @cmds: list of supported commands
 * @handler: command handler
 */
struct cli_handler
{
    char *cmds;
    char * (*handler)(void *sess, char *cmd, char *param, int size, int* rsp_len);
    char * (*handler_bin)(void *sess, char *cmd, int cmd_len, int size, int* rsp_len);
};

#define CLI_PORT 60000 /* Use unassigned port for remote support */
#define SERVER_PORT     ((u16)1234)
#define MAX_CONNECTIONS 5
#define RESP_BUF_SZ     80


#define MAX_NUM_IDS  (MAX_CONNECTIONS<<1)
#define RECORD_PAR_FILE  "/data/video.cfg"
#define MONITOR_PAR_FILE "/video.cfg"


/**
 * cli_sess_ctx - cli session context data structure
 * @sock: cli server socket number
 * @port: cli server port number
 * @cmd: cli command/handler structure
 * @tid: cli server thread id
 * @running: keeps cli running
 * @saddr: socket address structure
 * @from: destination address
 * @arg: optional argument used in command callback
 * @sock_ctx: socket context
 */
struct cli_sess_ctx
{
    int sock;
    int port;
    struct cli_handler *cmd;
    pthread_t tid;
    int running;
    struct sockaddr_in *saddr;
    struct sockaddr_in from;
    void *arg;
    struct sock_ctx *sock_ctx;
};

extern struct cli_sess_ctx *g_cli_ctx;

struct configstruct
{
    char name[64];
    char value[64];
};

struct cli_sess_ctx * start_cli(void* arg);
int cli_deinit(struct cli_sess_ctx *sess);
int start_vid(struct sess_ctx *sess, char *arg);
int get_sdcard_size();

int extract_value(struct configstruct *allconfig, int elements, char *name, int is_string, void *dst);
int set_value(struct configstruct *allconfig, int elements, char *name, int is_string, void *value);
//****user should free the returned pointer
struct configstruct * extract_config_file(int* element_numbers);
int write_config_value(struct configstruct *allconfig, int elements);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */

