#include <stdio.h>
#include <stdlib.h>
#include"video_cfg.h"

char *get_version_in_binary()
{
	char *v;
	int v1 ,v2 ,v3;
	v = (char *)malloc(32);
	memset(v,0,32);
	memcpy(v,APP_VERSION , strlen(APP_VERSION));
	sscanf(v , "version=%d.%d.%d",&v1 ,&v2,&v3);
	v[7] = (char)v1;
	v[8] = (char)v2;
	v[9] = (char)v3;
	return v;
}
