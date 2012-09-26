#include "includes.h"

#include <sys/un.h>

#include "wpa_ctrl.h"



struct wpa_ctrl
{
    int s;
    struct sockaddr_un local;
    struct sockaddr_un dest;
};


struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path)
{
    struct wpa_ctrl *ctrl;
    static int counter = 0;
    int ret;
    //size_t res;
    int tries = 0;

    ctrl =(struct wpa_ctrl*)malloc(sizeof(*ctrl));
    if (ctrl == NULL)
        return NULL;
    memset(ctrl, 0, sizeof(*ctrl));

    ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (ctrl->s < 0)
    {
        free(ctrl);
        return NULL;
    }

    ctrl->local.sun_family = AF_UNIX;
    counter++;
try_again:
    ret = snprintf(ctrl->local.sun_path, sizeof(ctrl->local.sun_path),
                   "/tmp/wpa_ctrl_%d-%d", getpid(), counter);
    if (ret < 0 || (size_t) ret >= sizeof(ctrl->local.sun_path))
    {
        close(ctrl->s);
        free(ctrl);
        return NULL;
    }
    tries++;
    if (bind(ctrl->s, (struct sockaddr *) &ctrl->local,
             sizeof(ctrl->local)) < 0)
    {
        if (errno == EADDRINUSE && tries < 2)
        {
            /*
             * getpid() returns unique identifier for this instance
             * of wpa_ctrl, so the existing socket file must have
             * been left by unclean termination of an earlier run.
             * Remove the file and try again.
             */
            unlink(ctrl->local.sun_path);
            goto try_again;
        }
        close(ctrl->s);
        free(ctrl);
        return NULL;
    }

    ctrl->dest.sun_family = AF_UNIX;
    if (strlen(ctrl_path)>=sizeof(ctrl->dest.sun_path))
    {
        close(ctrl->s);
        free(ctrl);
        printf("ctrl_path tool long\n");
        return NULL;
    }
    strncpy(ctrl->dest.sun_path, ctrl_path,sizeof(ctrl->dest.sun_path));
    if (connect(ctrl->s, (struct sockaddr *) &ctrl->dest,sizeof(ctrl->dest)) < 0)
    {
        close(ctrl->s);
        unlink(ctrl->local.sun_path);
        free(ctrl);
        printf("cannot connect target\n");
        return NULL;
    }

    return ctrl;
}


void wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
    unlink(ctrl->local.sun_path);
    close(ctrl->s);
    free(ctrl);
}
int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,
                     char *reply, size_t *reply_len,
                     void (*msg_cb)(char *msg, size_t len))
{
    struct timeval tv;
    int res;
    fd_set rfds;
    const char *_cmd;
    char *cmd_buf = NULL;
    size_t _cmd_len;


    _cmd = cmd;
    _cmd_len = cmd_len;

    if (send(ctrl->s, _cmd, _cmd_len, 0) < 0)
    {
        free(cmd_buf);
        return -1;
    }
    free(cmd_buf);

    for (;;)
    {
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(ctrl->s, &rfds);
        res = select(ctrl->s + 1, &rfds, NULL, NULL, &tv);
        if (FD_ISSET(ctrl->s, &rfds))
        {
            res = recv(ctrl->s, reply, *reply_len, 0);
            if (res < 0)
                return res;
            if (res > 0 && reply[0] == '<')
            {
                /* This is an unsolicited message from
                 * wpa_supplicant, not the reply to the
                 * request. Use msg_cb to report this to the
                 * caller. */
                if (msg_cb)
                {
                    /* Make sure the message is nul
                     * terminated. */
                    if ((size_t) res == *reply_len)
                        res = (*reply_len) - 1;
                    reply[res] = '\0';
                    msg_cb(reply, res);
                }
                continue;
            }
            *reply_len = res;
            break;
        }
        else
        {
            return -2;
        }
    }
    return 0;
}
