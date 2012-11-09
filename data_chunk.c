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

