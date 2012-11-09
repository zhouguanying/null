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

#define FRAME_SIZE (64*1024)
#define FRAME_QUEUE_LEN 2

struct frame_buffer
{
    unsigned char data[FRAME_SIZE];
    int size;
};

typedef struct frame_buffer frame_buffer_t;

typedef struct 
{
    frame_buffer_t *array;
    int count;
    int size;
    int i_index;
    int o_index;
} frame_queue_t;

frame_queue_t * frame_queue_new(int size);
void            frame_queue_free(frame_queue_t *queue);
int             frame_queue_push(frame_queue_t *queue, 
                        unsigned char *frame, int size);
int             frame_queue_get(frame_queue_t *queue,
                        unsigned char *recv_buf, int *size);
int             frame_queue_length(frame_queue_t *queue);
int             frame_queue_freespace(frame_queue_t *queue);
void            frame_queue_clear(frame_queue_t *queue);
#endif
