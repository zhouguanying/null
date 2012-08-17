#include <sys/types.h>
#include <unistd.h>
#include "utilities.h"
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h> 

int sem_wait_safe(sem_t *sem)
{
	while(sem_wait(sem) < 0);
	return 0;
}

int get_us()
{
	struct timeval tp;

	// get system time
	gettimeofday(&tp, NULL);
	
	// convert to us
	return (tp.tv_sec * 1000 * 1000 + tp.tv_usec);
}

int get_time_second()
{
	struct timeval tp;

	// get system time
	gettimeofday(&tp, NULL);
	
	// convert to us
	return tp.tv_sec;
}

int hex_string_to_int(char* string, int num)
{
	char buffer[20];
	char* end;
	if( num >= 20 )
		return 0;
	strcpy(buffer,"0x");
	memcpy( &buffer[2], string, num );
	buffer[num+2] = 0;
//	dbg("sequence:%s\n",buffer);
	return strtol(buffer,&end,16);
}

unsigned int hex_string_to_uint(char *string , int num)
{
	char buffer[20];
	char* end;
	if( num >= 20 )
		return 0;
	strcpy(buffer,"0x");
	memcpy( &buffer[2], string, num );
	buffer[num+2] = 0;
//	dbg("sequence:%s\n",buffer);
	return strtoul(buffer,&end,16);
}

int dec_string_to_int(char* string, int num)
{
	char buffer[20];
	char* end = 0;
	if( num >= 20 )
		return 0;
	memcpy( buffer, string, num );
	buffer[num] = 0;
//	dbg("sequence:%s\n",buffer);
	return strtol(buffer,&end,10);
}

long long dec_string_to_int64(char* string, int num)
{
#if 0
	char buffer[20];
	char* end;
	if( num >= 20 )
		return 0;
	memcpy( buffer, string, num );
	buffer[num] = 0;
//	dbg("sequence:%s\n",buffer);
	return strtol(buffer,&end,10);
#endif
	return 0;
}

int socket_set_nonblcok(int socket)
{
	long arg;
	if( (arg = fcntl(socket, F_GETFL, NULL)) < 0) { 
		//fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
		return -1;
	} 
	arg |= O_NONBLOCK; 
	if( fcntl(socket, F_SETFL, arg) < 0) { 
		//fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
		return -1;
	}

	return 0;
}
int socket_reset_block(int socket)
{
	long arg;
	if( (arg = fcntl(socket, F_GETFL, NULL)) < 0) { 
		//fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
		return -1;
	} 
	arg &=(~O_NONBLOCK);
	if( fcntl(socket, F_SETFL, arg) < 0) { 
		//fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
		return -1;
	}
	return 0;
}


