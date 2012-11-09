#ifndef _DATA_CHUNK_H_
#define _DATA_CHUNK_H_

typedef struct data_chunk
{
    unsigned char *ptr;
    long           size;
    long           read_pos;
    long           write_pos;
    long           data_size;
} data_chunk_t;

data_chunk_t * data_chunk_new(long size);
void data_chunk_free(data_chunk_t *chunkp);
long data_chunk_pushback(data_chunk_t *p,
                unsigned char *buf, long size);
long data_chunk_popfront(data_chunk_t *p,
                unsigned char *buf, long size);
long inline data_chunk_size(data_chunk_t *p);
long inline data_chunk_freespace(data_chunk_t *p);
void data_chunk_clear(data_chunk_t *p);

#endif
