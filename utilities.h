#ifndef UTILITIES_H
#define UTILITIES_H

#include <semaphore.h>
#include <sys/time.h>

int hex_string_to_int(char* string, int num);
unsigned int hex_string_to_uint(char *string , int num);

//example: 2009 4 516: no zero ahead the number
int dec_string_to_int(char* string, int num);
long long dec_string_to_int64(char* string, int num);

int socket_set_nonblcok(int socket);
#endif
