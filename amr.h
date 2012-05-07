#ifndef __AMR_H__
#define __AMR_H__
#ifdef __cplusplus
extern "C"
{
#endif
int init_amrcoder(short  dtx_t);
void  exit_amrcoder();
 int init_amrdecoder();
 void exit_amrdecoder();
int amrcoder(char *src,ssize_t src_size,char *dst,ssize_t *dst_size,int armmode,int channels);
int amrdecoder(char *src,ssize_t src_size,char *dst,ssize_t*dst_size,int channels);
#ifdef __cplusplus
}
#endif
#endif
