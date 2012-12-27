#include "data_chunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

data_chunk_t *
data_chunk_new(long size)
{
    if (size > 0)
    {
        struct data_chunk *dp = calloc(1, sizeof(struct data_chunk));
        dp->ptr = malloc(size);
        dp->size = size;
        return dp;
    }
    else
        return NULL;
}

void data_chunk_free(data_chunk_t *chunkp)
{
    if (chunkp)
    {
        if (chunkp->ptr)
        {
            free(chunkp->ptr);
            chunkp->ptr = 0;
        }

        free(chunkp);
    }
}

long data_chunk_pushback(data_chunk_t *p, unsigned char *buf, long size)
{
    if (!p || size <= 0)
        return -1;

    assert(size <= p->size);
    if (size > p->size)
        return -1;

    if (size > (p->size - p->data_size) )
    {
        fprintf(stderr, "%s buffer data is overflow !!! %ld -> %ld\n", __func__, size, p->size - p->data_size);
        size = p->size - p->data_size;
    }

    if (size + p->write_pos > p->size)
    {
        long tmp = p->size - p->write_pos;
        memcpy(p->ptr + p->write_pos, buf, tmp);
        memcpy(p->ptr, buf + tmp, size - tmp);
    }
    else
        memcpy(p->ptr + p->write_pos, buf, size);

    p->write_pos += size;
    p->write_pos %= p->size;
    p->data_size += size;

    return size;
}

long data_chunk_popfront(data_chunk_t *p, unsigned char *buf, long size)
{
    if (!p || size <= 0)
        return -1;

    assert(size <= p->size);
    if (size > p->size)
        return -1;

    if (size > p->data_size)
    {
        fprintf(stderr, "%s buffer data is not enough !!! %ld -> %ld\n", __func__, size, p->data_size);
        size = p->data_size;
    }

    if (size + p->read_pos > p->size)
    {
        long tmp = p->size - p->read_pos;
        memcpy(buf, p->ptr + p->read_pos,  tmp);
        memcpy(buf + tmp, p->ptr, size - tmp);
    }
    else
        memcpy(buf, p->ptr + p->read_pos, size);

    p->read_pos     += size;
    p->read_pos     %= p->size;
    p->data_size    -= size;

    return size;
}

void data_chunk_clear(data_chunk_t *p)
{
    if (p && p->ptr)
    {
        p->data_size = p->read_pos = p->write_pos = 0;
    }
}

long inline data_chunk_size(data_chunk_t *p)
{
    if (p && p->ptr)
        return p->data_size;
    else
        return 0;
}

long inline data_chunk_freespace(data_chunk_t *p)
{
    if (p && p->ptr)
        return p->size - p->data_size;
    else
        return 0;
}

long data_chunk_copy(data_chunk_t *dst, data_chunk_t *src, long size)
{
    if (size > src->size)
    {
        fprintf(stderr, "### size is too lager %ld >> %ld, will be clipped !!!\n",
                size, src->size);
        size = src->size;
    }

    if (src->read_pos + size > src->size)
    {
        long s = src->size - src->read_pos;
        data_chunk_pushback(dst, src->ptr + src->read_pos, s);
        data_chunk_pushback(dst, src->ptr, size - s);
    }
    else 
        data_chunk_pushback(dst, src->ptr + src->read_pos, size);

    return size;
}


frame_queue_t * 
frame_queue_new(int size)
{
    assert(size > 0);
    frame_queue_t *queue = malloc(sizeof(frame_queue_t));
    queue->array = malloc(size * sizeof(frame_buffer_t));
    queue->size = size;
    queue->count = 0;
    queue->i_index = 0;
    queue->o_index = 0;
    return queue;
}

void frame_queue_free(frame_queue_t *queue)
{
    if (queue)
    {
        if (queue->array)
            free(queue->array);
        free(queue);
    }
}

int frame_queue_push(frame_queue_t *queue, unsigned char *frame, int size)
{
    if (size > FRAME_SIZE)
        return 0;
    assert(queue->count < queue->size);
    if (queue->count >= queue->size)
        return 0;

    memcpy(queue->array[queue->i_index].data, frame, size);
    queue->array[queue->i_index].size = size;
    queue->i_index += 1;
    queue->i_index %= queue->size;
    queue->count   += 1;

    return 1;
}

int frame_queue_get(frame_queue_t *queue, unsigned char *recv_buf, int *size)
{
    assert(queue->count <= queue->size);
    if (queue->count <= 0)
        return 0;

    *size = queue->array[queue->o_index].size;
    memcpy(recv_buf, queue->array[queue->o_index].data, queue->array[queue->o_index].size);
    queue->o_index  += 1;
    queue->o_index  %= queue->size;
    queue->count    -= 1;

    return 1;
}

int frame_queue_length(frame_queue_t *queue)
{
    assert(queue);
    return queue->count;
}

int frame_queue_freespace(frame_queue_t *queue)
{
    assert(queue);
    return queue->size - queue->count;
}

void frame_queue_clear(frame_queue_t *queue)
{
    queue->count   = 0;
    queue->i_index = 0;
    queue->o_index = 0;
}

