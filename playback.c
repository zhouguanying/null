//#define DEBUG
#include <signal.h>
#include "playback.h"
#include "nand_file.h"
#include "record_file.h"
#include "utilities.h"
#include "cli.h"
#include "udttools.h"
#include "socket_container.h"

#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: %d: " fmt , __func__, __LINE__,## args); \
    } while (0)

static LIST_HEAD(playback_list);
pthread_mutex_t list_lock;

void* playback_thread(void * arg);

int playback_init()
{
    pthread_mutex_init(&list_lock, NULL);
    return 0;
}
playback_t* playback_find(struct sockaddr_in address)
{
    struct list_head* p;
    playback_t* pb;

    PRINTF("%s: enter\n", __func__);

    list_for_each(p, &playback_list)
    {
        pb = list_entry(p, playback_t, list);
        if (pb->address.sin_addr.s_addr ==
                address.sin_addr.s_addr&&pb->address.sin_port==address.sin_port)
        {
            printf("%s: pb address=0x%x, address to find=0x%x\n",
                   __func__, pb->address.sin_addr.s_addr, address.sin_addr.s_addr);
            printf("pb port=%d , address port=%d\n",ntohs(pb->address.sin_port),ntohs(address.sin_port));
            return pb;
        }
    }

    PRINTF("%s: leave\n", __func__);

    return 0;
}

static int playback_is_dead(playback_t* pb)
{
    int ret;

    pthread_mutex_lock(&pb->lock);
    ret = pb->dead;
    pthread_mutex_unlock(&pb->lock);

    return ret;
}

void playback_set_dead(playback_t* pb)
{
    pthread_mutex_lock(&pb->lock);
    pb->dead = 1;
    pthread_mutex_unlock(&pb->lock);
}

static int playback_destroy(playback_t* pb)
{
    list_del(&pb->list);
    free(pb);
    return 0;
}

void playback_remove_dead()
{
    struct list_head* p;
    struct list_head* n;
    playback_t* pb;

    PRINTF("%s: enter\n", __func__);

    pthread_mutex_lock(&list_lock);

    list_for_each_safe(p, n, &playback_list)
    {
        pb = list_entry(p, playback_t, list);
        if (playback_is_dead(pb))
        {
            playback_destroy(pb);
        }
    }

    pthread_mutex_unlock(&list_lock);

    PRINTF("%s: leave\n", __func__);
}

int playback_new(struct sockaddr_in address, int file, int seek_percent)
{
    int ret = 0;
    playback_t* pb;
    //int i;

    playback_remove_dead();

    pthread_mutex_lock(&list_lock);
    pb = playback_find(address);
    if (pb)
    {
        //alread have a playback instance? kill it.
        if (playback_get_status(pb) != PLAYBACK_STATUS_OFFLINE)
        {
            if (!playback_is_dead(pb))
            {
                playback_set_status(pb, PLAYBACK_STATUS_EXIT);
                if (pb->thread_id)
                    pthread_join(pb->thread_id, NULL);
            }
        }

        playback_destroy(pb);
    }

    pb = (playback_t *)malloc(sizeof(playback_t));
    if (pb)
    {
        pb->address = address;
        pb->file = file;
        pb->socket = -1;
        pb->status = PLAYBACK_STATUS_OFFLINE;
        pb->thread_id = 0;
        pb->dead = 0;
        pb->seek = seek_percent;
        pthread_mutex_init(&pb->lock, NULL);
        list_add_tail(&pb->list, &playback_list);
        pthread_mutex_unlock(&list_lock);
    }
    else
    {
        ret = -1;
        pthread_mutex_unlock(&list_lock);
    }
    return ret;
}


int playback_connect(struct sockaddr_in address, int socket)
{
    int ret = 0;
    playback_t* pb;

    playback_remove_dead();

    pthread_mutex_lock(&list_lock);

    pb = playback_find(address);

    if (pb && (playback_get_status(pb) == PLAYBACK_STATUS_OFFLINE))
    {
        socket_set_nonblcok(socket);
        pb->socket = socket;
        pb->status = PLAYBACK_STATUS_RUNNING;
        pthread_create(
            &pb->thread_id,
            NULL,
            playback_thread,
            (void*)pb);
        pthread_mutex_unlock(&list_lock);
        pthread_join(pb->thread_id,NULL);
    }
    else
    {
        pthread_mutex_unlock(&list_lock);
        ret = -1;
    }

    return ret;
}

int playback_set_status(playback_t* pb, int status)
{
    pthread_mutex_lock(&pb->lock);
    pb->status = status;
    pthread_mutex_unlock(&pb->lock);

    return 0;
}

int cmd_playback_set_status(struct sockaddr_in address ,  int status , void *value)
{
    int ret = 0;
    playback_t* pb;

    if (status == PLAYBACK_STATUS_SEEK &&!value)
        return -1;

    pthread_mutex_lock(&list_lock);

    pb = playback_find(address);
    if (pb)
    {
        if (status == PLAYBACK_STATUS_SEEK)
        {
            pb->seek = *(int *)value;
        }
        playback_set_status(pb, status);
    }
    else
    {
        ret = -1;
        dbg("playback not found\n");
    }
    pthread_mutex_unlock(&list_lock);
    return ret;
}

int playback_get_status(playback_t* pb)
{
    int ret;

    pthread_mutex_lock(&pb->lock);
    ret = pb->status;
    pthread_mutex_unlock(&pb->lock);

    return ret;
}

int playback_seekto(struct sockaddr_in address, int percent)
{
    playback_t* pb;

    pthread_mutex_lock(&list_lock);
    pb = playback_find(address);
    pthread_mutex_unlock(&list_lock);

    if (pb)
    {
        pthread_mutex_lock(&pb->lock);
        pb->seek = percent;
        pb->status = PLAYBACK_STATUS_SEEK;
        pthread_mutex_unlock(&pb->lock);
    }
    return 0;
}

int playback_exit(struct sockaddr_in address)
{
    int ret = 0;
    playback_t* pb;

    pthread_mutex_lock(&list_lock);

    pb = playback_find(address);
    if (pb)
    {
        playback_set_status(pb, PLAYBACK_STATUS_EXIT);
        //if(pb->thread_id)
        //pthread_join(pb->thread_id, &status);
        //playback_destroy(pb);
        dbg("tell playback thread exit!\n");
    }
    else
    {
        ret = -1;
        printf("playback not found\n");
    }

    pthread_mutex_unlock(&list_lock);
    return ret;

}

static int playback_send_data(int socket ,playback_t* pb, char* buf, int len)
{
    int  ret = -1 , s;
    int status;
    int attempts= 0;
    s = 0;
    //dbg("#################begin send data#####################\n");
    while (len > 0)
    {
        //printf("%s: running.\n", __func__);
        status = playback_get_status(pb);
        switch (status)
        {
        case PLAYBACK_STATUS_PAUSED:
        case PLAYBACK_STATUS_SEEK:
            ret = 0;
            goto END;
            break;
        case PLAYBACK_STATUS_EXIT:
            ret = -1;
            goto END;
            break;
        default:
            break;
        }
        if (len >1000)
        {
            if (pb->sess->is_tcp)
                ret = send(socket ,buf+s , 1000 , 0);
            else
                ret = udt_send(socket, SOCK_STREAM,  buf+s, 1000);
        }
        else
        {
            if (pb->sess->is_tcp)
                ret = send(socket ,buf+s , len , 0);
            else
                ret = udt_send(socket, SOCK_STREAM,  buf+s, len);
        }

        if (ret <=0)
        {
            attempts ++;
            if (attempts <=10)
            {
                dbg("attempts to send playback data now = %d\n",attempts);
                continue;
            }
            ret = -1;
            break;
        }
        attempts = 0;
        s += ret;
        len -= ret;
    }
    //dbg("##############################send data ok######################\n");
END:
    return ret;
}
/*
|----------------------------------------------------------------------------------------------------------------------------------------------|
|table1 data size 4B |table2 data size 4B|       table1 （max size..k）         |           table2   （max size..k）                                                     |
|----------------------------------------------------------------------------------------------------------------------------------------------|
|<-----------------------------------------------total size <=32*1024 bytes---------------------------------------------------------------------->|
第一个表为属性改变时插入的内部头结构的位置，第二个表为每15秒图像写入文件
的位置，两个表的表项为下列结构体
typedef struct __record_item{
   unsigned int locatetion;
}record_item_t;
*/

static  int playback_send_raw_data(int socket ,playback_t* pb, char* buf, int len)
{
    int  ret = -1 , s;
    int status;
    int attempts= 0;
    s = 0;
    while (len > 0)
    {
        //printf("%s: running.\n", __func__);
        status = playback_get_status(pb);
        switch (status)
        {
        case PLAYBACK_STATUS_EXIT:
            ret = -1;
            goto END;
            break;
        default:
            break;
        }
        if (len >1000)
        {
            if (pb->sess->is_tcp)
                ret = send(socket ,buf+s , 1000 , 0);
            else
                ret = udt_send(socket, SOCK_STREAM,  buf+s, 1000);
        }
        else
        {
            if (pb->sess->is_tcp)
                ret = send(socket ,buf+s , len , 0);
            else
                ret = udt_send(socket, SOCK_STREAM,  buf+s, len);
        }

        if (ret <=0)
        {
            attempts ++;
            if (attempts <=10)
            {
                dbg("attempts to send playback data now = %d\n",attempts);
                continue;
            }
            ret = -1;
            break;
        }
        attempts = 0;
        s += ret;
        len -= ret;
    }
END:
    return ret;
}

void send_raw_record_file(playback_t *pb)
{
    int socket;
    char *buf;
    int size;
    int fd;
    socket = pb->sess->sc->video_socket;
    buf = (char *)malloc(200*1024);
    if (!buf)
    {
        dbg("##########malloc buf for playback error#########\n");
        return;
    }
    fd = open_download_file(pb->file);
    if (fd <0)
    {
        dbg("#############open download file error#############\n");
        free(buf);
        return;
    }
    while ((size = read(fd, buf, 200*1024))>0)
    {
        if (playback_send_raw_data(socket,  pb, buf, size)<0)
            goto out ;
    }
    /*
    for(start_sector = pb->file , seek = 0; seek < NAND_RECORD_FILE_SECTOR_SIZE ; seek += PLAYBACK_SECTOR_NUM_ONE_READ)
    {
        req.buf = (unsigned char *)buf;
        req.sector_num = PLAYBACK_SECTOR_NUM_ONE_READ;
        req.start = start_sector + seek;
        if(read_file_segment(& req)<0){
            dbg("##############playback file end?#############\n");
            goto out;
        }
        if(playback_send_raw_data( socket,  pb, buf, PLAYBACK_SECTOR_NUM_ONE_READ * 512)<0)
            goto out ;
    }
    */
    dbg("################ok the file send completely################\n");
out:
    close(fd);
    free(buf);
    return;
}

#define PLAYBACK_SECTOR_NUM_ONE_READ 4
void* playback_thread(void * arg)
{
    playback_t* pb = (playback_t*)arg;
    record_file_t* file = NULL;
    int status;
    char* buf = NULL;
    unsigned int attr_table_size;
    unsigned int time_table_size;
    char *table_buf =NULL;
    index_table_item_t *table_item_p;
    int ret, size;
    nand_record_file_internal_header internal_header;
    nand_record_file_internal_header *internal_header_p;
    nand_record_file_header *file_header;
    int running = 1;
    unsigned int seek;
    unsigned int old_real_size;
    char *s,*e;
    unsigned int reserve_seek;
    int socket;
    int end_wait_time;
//    struct timeval old_send_time , currtime;
//    unsigned long long timeuse;
//    unsigned int snd_size;
    socket = pb->sess->sc->video_socket;
    pb->sess->ucount = 1;
    add_sess(pb->sess);
    take_sess_up(pb->sess);

    if (check_nand_file(pb->file)<0)
        goto __out;
    if (pb->seek <0)
    {
        dbg("###############downloal file###############\n");
        send_raw_record_file(pb);
        goto __out;
    }

    buf = malloc(PLAYBACK_SECTOR_NUM_ONE_READ*512);
    if (!buf)
    {
        printf("%s: malloc error\n", __func__);
        playback_set_dead(pb);
        del_sess(pb->sess);
        take_sess_down(pb->sess);
        pb->sess =NULL;
        return 0;
    }

    file = record_file_open(pb->file);
    if (file == NULL)
    {
        printf("%s: open file error\n", __func__);
        playback_set_dead(pb);
        free(buf);
        del_sess(pb->sess);
        take_sess_down(pb->sess);
        pb->sess =NULL;
        return 0;
    }
    playback_set_status(pb, PLAYBACK_STATUS_RUNNING);
    if (file->index_table_pos == 0xffffffff)
    {
no_table:
        memset(buf,0,8);
        ret = playback_send_data(socket , pb,buf,8);
        if (ret<0)
            goto __out;
    }
    else
    {
        dbg("try to read index table  pos = %u\n",file->index_table_pos);
        old_real_size = file->real_size;
        file->real_size = 200*1024*1024;
        if (record_file_seekto(file,file->index_table_pos)<0)
            goto __out;
        size = record_file_read(file ,(unsigned char *) buf,PLAYBACK_SECTOR_NUM_ONE_READ);
        if (size<=0)
        {
            printf("cannot read index table\n");
            goto __out;
        }
        s = buf +file->index_table_pos %512;
        size -=(s-buf);
        memcpy(&attr_table_size , s , 4);
        memcpy(&time_table_size,s+4,4);

        if (attr_table_size == 0xfffffffe)
        {
            dbg("##############table flag  deleted#############\n");
            goto __out;
        }
        if ((attr_table_size == 0 && time_table_size ==0)||attr_table_size ==0xffffffff)
        {
            dbg("############table flag no table##############\n");
            file->real_size = old_real_size;
            goto no_table;
        }

        dbg("try to malloc index table buff size = %u\n",attr_table_size +time_table_size +8);
        table_buf = (char *)malloc(attr_table_size + time_table_size + 8);
        if (!table_buf)
        {
            printf("error malloc buff for index table\n");
            goto __out;
        }
        if (attr_table_size + time_table_size +8<=size)
        {
            memcpy(table_buf , s , attr_table_size + time_table_size +8);
        }
        else
        {
            memcpy(table_buf,s,size);
            while (size <attr_table_size + time_table_size +8)
            {
                ret = record_file_read(file,(unsigned char *)buf , PLAYBACK_SECTOR_NUM_ONE_READ);
                if (ret<=0)
                {
                    printf("read index table error\n");
                    goto __out;
                }
                if (ret >= attr_table_size + time_table_size +8 - size)
                {
                    memcpy(table_buf + size , buf ,attr_table_size + time_table_size +8 - size);
                    break;
                }
                memcpy(table_buf + size , buf , ret);
                size += ret;
            }
        }
        size = playback_send_data(socket , pb,table_buf,attr_table_size+time_table_size+8);
        if (size<0)
            goto __out;
        file->real_size = old_real_size;
        dbg("send table ok\n");
    }

    if (record_file_seekto(file, pb->seek)<0)
        goto __out;
    /*get and send the last time stamp*/
    //gettimeofday(&old_send_time , NULL);
    dbg("In begining we seek to %d\n",pb->seek);
    size = record_file_read(
               file,(unsigned char *) buf, PLAYBACK_SECTOR_NUM_ONE_READ);
    if (size <= 0)
    {
        dbg("error read record data\n");
        goto __out;
    }
    else
    {
        file_header=(nand_record_file_header *)buf;
        if (file_header->head[0]!=0||file_header->head[1]!=0||file_header->head[2]!=0||
                file_header->head[3]!=1||file_header->head[4]!=0xc)
        {
            dbg("error file header cannot found\n");
        }
        else
            dbg("ok we found file header\n");
        memcpy(file_header->LastTimeStamp,file->LastTimeStamp,sizeof(file_header->LastTimeStamp));
        ret = playback_send_data(socket , pb,buf,size);
        if (ret < 0)
        {
            goto __out;
        }
    }
    //snd_size = size;
    //pb->seek = 0;
    //playback_set_status(pb, PLAYBACK_STATUS_SEEK);
    while (running)
    {
        //printf("playback thread: %d\n", pb->thread_id);
        status = playback_get_status(pb);
        switch (status)
        {
        case PLAYBACK_STATUS_RUNNING:
        {
            size = record_file_read(
                       file,(unsigned char *) buf, PLAYBACK_SECTOR_NUM_ONE_READ);
            if (size <= 0)
            {
                //ready to restart.
                /*
                playback_set_status(pb, PLAYBACK_STATUS_PAUSED);
                 record_file_seekto(file, 0);
                 */
                dbg("error read record data\n");
                running = 0;
            }
            else
            {
                //send it
                ret = playback_send_data(socket , pb,buf,size);
                if (ret < 0)
                {
                    running = 0;
                    dbg("playback_send_data error\n");
                }
                /*
                snd_size +=size;
                gettimeofday(&currtime , NULL);
                timeuse = (currtime.tv_sec - old_send_time.tv_sec)*1000000 + currtime.tv_usec -old_send_time.tv_usec;
                if(timeuse >=1000000){
                    dbg("send %u per sec\n",snd_size);
                    snd_size = 0;
                    memcpy(&old_send_time , &currtime , sizeof(struct timeval));
                }
                */
            }

            break;
        }
        case PLAYBACK_STATUS_EXIT:
            running = 0;
            break;
        case PLAYBACK_STATUS_SEEK:
__SEEK__:
            dbg("playback seek , pb->seek=%d\n",pb->seek);
            playback_set_status(pb, PLAYBACK_STATUS_RUNNING);
            reserve_seek = file->cur_sector *512;
            if (!table_buf)
                break;
            seek = 0;
            table_item_p=(index_table_item_t *)(table_buf + 8);
            while ((char *)table_item_p < table_buf +8 +attr_table_size)
            {
                if (table_item_p->location ==pb->seek)
                {
                    seek = table_item_p->location;
                    break;
                }
                if (table_item_p->location > pb->seek)
                {
                    if ((char *)table_item_p == table_buf + 8)
                        seek = 0;
                    else
                    {
                        table_item_p --;
                        seek = table_item_p->location;
                    }
                    break;
                }
                table_item_p ++;
                if ((char *)table_item_p == table_buf +8 +attr_table_size)
                {
                    table_item_p --;
                    seek = table_item_p ->location;
                    break;
                }
            }
            dbg("seek=%u , pb->seek = %u\n",seek , pb->seek);
            if (seek==0)
            {
                record_file_seekto(file,0);
                size = record_file_read(
                           file,(unsigned char *) buf, 1);
                if (size <=0)
                {
                    running = 0;
                    break;
                }
                else
                {
                    memset(&internal_header , 0 ,sizeof(internal_header));
                    file_header = (nand_record_file_header *)buf;
                    memcpy(internal_header.head , file_header->head , sizeof(internal_header.head));
                    /*
                    internal_header.flag[0]  |=(1<<FLAG0_TS_CHANGED_BIT | 1<<FLAG0_FR_CHANGED_BIT |
                        1<<FLAG0_FW_CHANGED_BIT | 1<<FLAG0_FH_CHANGED_BIT);
                    internal_header.flag[1] = 1;
                    */
                    internal_header.flag[0] = internal_header.flag[1] = internal_header.flag[2] = internal_header.flag[3] = 0xff;
                    memcpy(internal_header.StartTimeStamp , file_header->StartTimeStamp , sizeof(internal_header.StartTimeStamp));
                    memcpy(internal_header.FrameRateUs , file_header->FrameRateUs , sizeof(internal_header.FrameRateUs));
                    memcpy(internal_header.FrameWidth , file_header->FrameWidth , sizeof(internal_header.FrameWidth));
                    memcpy(internal_header.FrameHeight ,file_header->FrameHeight , sizeof(internal_header.FrameHeight));
                    status = playback_get_status(pb);
                    if (status !=PLAYBACK_STATUS_RUNNING)
                    {
                        record_file_seekto(file, reserve_seek);
                        goto __seek_out;
                    }
                    ret = playback_send_data(socket , pb,(char *)&internal_header,sizeof(internal_header));
                    if (ret < 0)
                    {
                        running = 0;
                        dbg("playback_send_data error\n");
                        break;
                    }
                }
            }
            else if (seek!=pb->seek)
            {
                if (record_file_seekto(file, seek)<0)
                    goto __out;
                size = record_file_read(
                           file,(unsigned char *) buf, 2);
                if (size <=0)
                {
                    running = 0;
                    break;
                }
                else
                {
                    s = buf + seek%512;
                    if (s[0]==0&&s[1]==0&&s[2]==0&&s[3]==1&&s[4]==0xc)
                    {
                        //dbg("ok we find internal header\n");
                        internal_header_p = (nand_record_file_internal_header *)s;
                        if (internal_header_p->flag[1]!=1)
                        {
                            dbg("##########the flag[1] is wrong  it equal %d#############\n" , internal_header_p->flag[1]);
                        }
                        else
                        {
                            dbg("############ok we find video internal header###########\n");
                            internal_header_p->flag[0] = internal_header_p->flag[1] = internal_header_p->flag[2] = internal_header_p->flag[3] = 0xff;
                        }
                    }
                    else
                    {
                        dbg("we not found internal header error\n");
                        record_file_seekto(file, reserve_seek);
                        goto __seek_out;
                    }
                    e = s;
                    ret =0;
                    while (e[0]!=0xff||e[1]!=0xd8||e[2]!=0xff||e[3]!=0xe0)
                    {
                        ret ++;
                        if (ret>sizeof(nand_record_file_internal_header)+1)
                        {
                            dbg("we not found the next jpeg header\n");
                            record_file_seekto(file, reserve_seek);
                            goto __seek_out;
                        }
                        e++;
                    }
                    status = playback_get_status(pb);
                    if (status !=PLAYBACK_STATUS_RUNNING)
                    {
                        record_file_seekto(file, reserve_seek);
                        goto __seek_out;
                    }
                    ret = playback_send_data(socket , pb,s,e -s);
                    if (ret < 0)
                    {
                        dbg("playback_send_data error\n");
                        running = 0;
                        break;
                    }
                }

            }
            if (record_file_seekto(file, pb->seek)<0)
                goto __out;
            size = record_file_read(
                       file,(unsigned char *) buf, PLAYBACK_SECTOR_NUM_ONE_READ);
            if (size<=0)
            {
                running = 0;
                break;
            }
            if (pb->seek==0&&seek==0)
                s=buf+sizeof(nand_record_file_header);
            else
            {
                s = buf + pb->seek%512;
                if (pb->seek ==seek)
                {
                    internal_header_p = (nand_record_file_internal_header *)s;
                    if (internal_header_p->head[0] == 0&&internal_header_p->head[1] == 0&&internal_header_p->head[2] == 0&&
                            internal_header_p->head[3] == 1&&internal_header_p->head[4] == 0xc)
                    {
                        if (internal_header_p->flag[1]!=1)
                        {
                            dbg("##########the flag[1] is wrong  it equal %d#############\n" , internal_header_p->flag[1]);
                        }
                        else
                        {
                            dbg("############ok we find video internal header###########\n");
                            internal_header_p->flag[0] = internal_header_p->flag[1] = internal_header_p->flag[2] = internal_header_p->flag[3] = 0xff;
                        }
                    }
                    else
                        dbg("#############error we not find internal header##############\n");
                }
            }
            status = playback_get_status(pb);
            if (status !=PLAYBACK_STATUS_RUNNING)
            {
                record_file_seekto(file, reserve_seek);
                goto __seek_out;
            }
            ret = playback_send_data(socket , pb,s,size -(s-buf));
            if (ret < 0)
            {
                dbg("playback_send_data error\n");
                running = 0;
            }
__seek_out:
            break;
        case PLAYBACK_STATUS_PAUSED:
        default:
            usleep(100000);
            break;

        }
    }
__out:
    for (end_wait_time = 15; end_wait_time>0 ; end_wait_time --)
    {
        status = playback_get_status(pb);
        switch (status)
        {
        case PLAYBACK_STATUS_EXIT:
            goto __exit;
            break;
        case PLAYBACK_STATUS_SEEK:
            running = 1;
            goto __SEEK__;
            break;
        default:
            break;
        }
        sleep(1);
    }
__exit:
    dbg("exit playback thread\n");
    if (buf)
        free(buf);
    if (table_buf)
        free(table_buf);
    del_sess(pb->sess);
    take_sess_down(pb->sess);
    pb->sess = NULL;
    if (file)
        record_file_close(file);
    playback_set_dead(pb);
    return NULL;
}

