#ifndef UTILITIES_H
#define UTILITIES_H

#include <semaphore.h>
#include <sys/time.h>

//#define DEBUG
#ifdef DEBUG
#define PRINTF(fmt,arg...) \
	printf(fmt,##arg)
#else
#define PRINTF(fmt,arg...)
#endif


int sem_wait_safe(sem_t *sem);

int get_us();
int get_time_second();
int hex_string_to_int(char* string, int num);
unsigned int hex_string_to_uint(char *string , int num);
//example: 2009 4 516: no zero ahead the number
int dec_string_to_int(char* string, int num);
long long dec_string_to_int64(char* string, int num);

int socket_set_nonblcok(int socket);
int socket_reset_block(int socket);
#endif


