//#define DEBUG
#include <signal.h>
#include "playback.h"
#include "nand_file.h"
#include "record_file.h"
#include "utilities.h"
#include "cli.h"

#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt , __func__, ## args); \
    } while (0)

static LIST_HEAD(playback_list);
pthread_mutex_t list_lock;
//pthread_mutex_t pb_rtp_port_lock;
//char  pb_rtp_port[MAX_CONNECTIONS];
//#define PB_BASE_PORT 	5006

void* playback_thread(void * arg);

int playback_init()
{
	/*
	int i;
	for(i=0;i<MAX_CONNECTIONS;i++){
		pb_rtp_port[i]=0;
	}
	pthread_mutex_init(&pb_rtp_port_lock,NULL);
	*/
	pthread_mutex_init(&list_lock, NULL);
	return 0;
}
/*
uint16_t get_playback_port()
{
	int i;
	pthread_mutex_lock(&pb_rtp_port_lock);
	for(i=0;i<MAX_CONNECTIONS;i++)
		if(pb_rtp_port[i]==0)
			break;
		pb_rtp_port[i]=1;
	pthread_mutex_unlock(&pb_rtp_port_lock);
	printf("in get_playback_port i==%d,port==%d\n",i,PB_BASE_PORT+(i<<1));
	return (PB_BASE_PORT+(i<<1));
}
int put_playback_port(uint16_t port)
{
	int i;
	if(port<PB_BASE_PORT)
		return -1;
	i=(port-PB_BASE_PORT)>>1;
	printf("in put playback port port==%d ,  i==%d\n",port,i);
	pthread_mutex_lock(&pb_rtp_port_lock);
	pb_rtp_port[i]=0;
	pthread_mutex_unlock(&pb_rtp_port_lock);
	return 0;
}
*/
/*
int add_to_keep_alive_list(struct pb_port_connet_management *new_kalive)
{
	if(new_kalive==NULL)
		return -1;
	pthread_mutex_lock(&pb_rtp_port_lock);
	new_kalive->next=keep_alive_list;
	keep_alive_list=new_kalive;
	pthread_mutex_unlock(&pb_rtp_port_lock);
	return 0;
	
}
struct pb_port_connet_management * remove_from_keep_alive_list(uint32_t addr)
{
	struct pb_port_connet_management**tmp;
	struct pb_port_connet_management*p;
	pthread_mutex_lock(&pb_rtp_port_lock);
	tmp=&keep_alive_list;
	while(((*tmp)!=NULL)&&((*tmp)->monitor_ip!=addr))
		tmp=&((*tmp)->next);
	if((*tmp)==NULL)
		p=NULL;
	else{
		p=(*tmp);
		*tmp=(*tmp)->next;
	}
	pthread_mutex_unlock(&pb_rtp_port_lock);
	return p;
}
int touch_playback_port(uint16_t localport,uint32_t ip,uint16_t port)
{
	int sockfd;
	int on;
	struct sockaddr_in from;
	socklen_t fromlen;
	if ((sockfd = create_udp_socket()) < 0) {
       	 perror("Error creating socket\n");
        	return -1;
   	 }
	on = 1;
	 if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
              printf("Error enabling socket address reuse\n");
        }
  	  if ((bind_udp_socket(sockfd, localport)) == NULL) {
     	   	 printf("****************************Error binding udp socket****************************\n");
      	  	 close(sockfd);
       	 return -1;
   	 }
	  fromlen=sizeof(struct sockaddr_in);
	  from.sin_addr.s_addr=ip;
	  from.sin_port=port;
	  sendto(sockfd, "touch", strlen("touch"), 0,(struct sockaddr *) &from, fromlen);
	  close(sockfd);
	  return 0;
}
int set_keep_alive_data(uint32_t who,uint32_t monitor_ip,uint16_t monitor_pb_port)
{
	struct pb_port_connet_management*p;
	int ret=-1;
	pthread_mutex_lock(&pb_rtp_port_lock);
	p=keep_alive_list;
	while(p!=NULL&&p->who!=who)
		p=p->next;
	if(p!=NULL){
		p->monitor_ip=monitor_ip;
		p->monitor_pb_port=monitor_pb_port;
		ret=touch_playback_port(p->local_pb_port, monitor_ip, monitor_pb_port);
		if(ret<0){
			printf("first touch playback port false\n");
		}
	}
	pthread_mutex_unlock(&pb_rtp_port_lock);
	if(p!=NULL&&ret<0)
		free(remove_from_keep_alive_list(p->monitor_ip));
	return ret;
}
int keep_playback_port_alive_thread()
{
	struct pb_port_connet_management *p;
	while(1){
		pthread_mutex_lock(&pb_rtp_port_lock);
		p=keep_alive_list;
		while(p!=NULL){
			if(p->monitor_ip>0&&p->monitor_pb_port>0)
				touch_playback_port(p->local_pb_port, p->monitor_ip, p->monitor_pb_port);
			p=p->next;
		}
		pthread_mutex_unlock(&pb_rtp_port_lock);
		sleep(60);
	}
	return 0;
}
*/
 playback_t* playback_find(struct sockaddr_in address)
{
	struct list_head* p;
	playback_t* pb;

	PRINTF("%s: enter\n", __func__);

	list_for_each(p, &playback_list){
		pb = list_entry(p, playback_t, list);
		if(pb->address.sin_addr.s_addr ==
				address.sin_addr.s_addr&&pb->address.sin_port==address.sin_port){
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
	//int i;
	//i=(int)(pb->rtpport-PB_BASE_PORT)>>1;
	list_del(&pb->list);
	if(pb->socket>=0)
		close(pb->socket);
	/*
	pthread_mutex_lock(&pb_rtp_port_lock);
	pb_rtp_port[i]=0;
	pthread_mutex_unlock(&pb_rtp_port_lock);
	*/
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

	list_for_each_safe(p, n, &playback_list){
		pb = list_entry(p, playback_t, list);
		if(playback_is_dead(pb)){
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
	if(pb){
		//alread have a playback instance? kill it.
		if(playback_get_status(pb) != PLAYBACK_STATUS_OFFLINE){
			if(!playback_is_dead(pb)){
				playback_set_status(pb, PLAYBACK_STATUS_EXIT);
				if(pb->thread_id)
					pthread_join(pb->thread_id, NULL);
			}
		}

		playback_destroy(pb);
	}

	pb = (playback_t *)malloc(sizeof(playback_t));
	if(pb){
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
		/*
		pthread_mutex_lock(&pb_rtp_port_lock);
		for(i=0;i<MAX_CONNECTIONS;i++){
			if(pb_rtp_port[i]==0)
				break;
		}
		pb_rtp_port[i]=1;
		pthread_mutex_unlock(&pb_rtp_port_lock);
		pb->rtpport=(uint16_t)(PB_BASE_PORT+(i<<1));
		pb->destaddr=(uint32_t)ntohl(address.sin_addr.s_addr);
		pb->destrtpport=(uint16_t)VIDEO_SESS_PORT;
		pb->destrtcpport=(uint16_t)(VIDEO_SESS_PORT+1);
		*/
		
	}else{
		ret = -1;
		pthread_mutex_unlock(&list_lock);
	}
	return ret;
}
/*
int rtp_playback_connect(struct sockaddr_in address )
{
	int ret =0;
	playback_t * pb;
	playback_remove_dead();

	pthread_mutex_lock(&list_lock);
	
	pb=playback_find( address);
	if(pb && (playback_get_status(pb) == PLAYBACK_STATUS_OFFLINE)){
		pb->status = PLAYBACK_STATUS_RUNNING;
		pb->socket=-1;
		pthread_create(
			&pb->thread_id,
			NULL,
			start_playback_session,
			(void*)pb);
		pthread_mutex_unlock(&list_lock);
		pthread_join(pb->thread_id,NULL);
		ret = 0;
	}else{
		pthread_mutex_unlock(&list_lock);
		ret = -1;
	}

	return ret;
}
*/
int playback_connect(struct sockaddr_in address, int socket)
{
	int ret = 0;
	playback_t* pb;

	playback_remove_dead();

	pthread_mutex_lock(&list_lock);
	
	pb = playback_find(address);

	if(pb && (playback_get_status(pb) == PLAYBACK_STATUS_OFFLINE)){
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
	}else{
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
	
	if(status == PLAYBACK_STATUS_SEEK &&!value)
		return -1;
	
	pthread_mutex_lock(&list_lock);
	
	pb = playback_find(address);
	if(pb){
		if(status == PLAYBACK_STATUS_SEEK){
			pb->seek = *(int *)value;
		}
		playback_set_status(pb, status);
	}else{
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

	if(pb){
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
	void* status;
	playback_t* pb;

	pthread_mutex_lock(&list_lock);
	
	pb = playback_find(address);
	if(pb){
		playback_set_status(pb, PLAYBACK_STATUS_EXIT);
		if(pb->thread_id)
			pthread_join(pb->thread_id, &status);
		playback_destroy(pb);
		printf("playback exit!\n");
	}else{
		ret = -1;
		printf("playback not found\n");
	}

	pthread_mutex_unlock(&list_lock);
	return ret;
	
}

static int playback_send_data(playback_t* pb, char* buf, int len)
{
	int retry, ret;
	
	retry = 1000;
	while(len > 0){
		//printf("%s: running.\n", __func__);
		ret = send(
				pb->socket,
				(void *) buf,
				len,
				0);
		if(ret >= 0){
			len -= ret;
			buf += ret;
		}else{
			if(--retry <= 0){
				printf("%s: retry time out.\n", __func__);
				if(playback_get_status(pb) ==PLAYBACK_STATUS_PAUSED)
					ret = 1;
				else
					ret = -1;
				break;
			}else{
				if(playback_get_status(pb) == PLAYBACK_STATUS_EXIT){
					printf("%s: exit.\n", __func__);
					ret = -1;
					break;
				}else{
					usleep(5000);
				}
			}
		}
		if(playback_get_status(pb) == PLAYBACK_STATUS_EXIT){
			printf("%s: retry time out.\n", __func__);
			ret = -1;
			break;
		}
	}

	return ret;
}

#define PLAYBACK_SECTOR_NUM_ONE_READ 4
void* playback_thread(void * arg)
{
	playback_t* pb = (playback_t*)arg;
	record_file_t* file;
	int status;
	char* buf;
	unsigned int attr_table_size;
	unsigned int time_table_size;
	char *table_buf =NULL;
	index_table_item_t *table_item_p;
	int ret, size;
	nand_record_file_internal_header internal_header;
	nand_record_file_header *file_header;
	int running = 1;
	unsigned int seek;
	unsigned int old_real_size;
	char *s,*e;
	unsigned int reserve_seek;
//	struct timeval old_send_time , currtime;
//	unsigned long long timeuse;
//	unsigned int snd_size;

	buf = malloc(PLAYBACK_SECTOR_NUM_ONE_READ*512);
	if(!buf){
		printf("%s: malloc error\n", __func__);
		playback_set_dead(pb);
		return 0;
	}
	
	file = record_file_open(pb->file);
	if(file == NULL){
		printf("%s: open file error\n", __func__);
		playback_set_dead(pb);
		free(buf);
		return 0;
	}
	
	if(file->index_table_pos == 0xffffffff)
	{
		memset(buf,0,8);
		ret = playback_send_data(pb,buf,8);
		if(ret<0)
			goto __out;
	}
	else
	{
		dbg("try to read index table  pos = %u\n",file->index_table_pos);
		old_real_size = file->real_size;
		file->real_size = 200*1024*1024;
		if(record_file_seekto(file,file->index_table_pos)<0)
			goto __out;
		size = record_file_read(file ,(unsigned char *) buf,PLAYBACK_SECTOR_NUM_ONE_READ);
		if(size<=0){
			printf("cannot read index table\n");
			goto __out;
		}
		s = buf +file->index_table_pos %512;
		size -=(s-buf);
		memcpy(&attr_table_size , s , 4);
		memcpy(&time_table_size,s+4,4);
		dbg("try to malloc index table buff size = %u\n",attr_table_size +time_table_size +8);
		table_buf = (char *)malloc(attr_table_size + time_table_size + 8);
		if(!table_buf){
			printf("error malloc buff for index table\n");
			goto __out;
		}
		if(attr_table_size + time_table_size +8<=size){
			memcpy(table_buf , s , attr_table_size + time_table_size +8);
		}else{
			memcpy(table_buf,s,size);
			while(size <attr_table_size + time_table_size +8){
				ret = record_file_read(file,(unsigned char *)buf , PLAYBACK_SECTOR_NUM_ONE_READ);
				if(ret<=0){
					printf("read index table error\n");
					goto __out;
				}
				if(ret >= attr_table_size + time_table_size +8 - size){
					memcpy(table_buf + size , buf ,attr_table_size + time_table_size +8 - size);
					break;
				}
				memcpy(table_buf + size , buf , ret);
				size += ret;
			}
		}
		/*
		printf("#####################check read table#########################\n");
		for(ret = 0;ret<attr_table_size + time_table_size +8 ; ret ++){
			printf("%02x ",table_buf[ret]);
		}
		printf("\n");
		printf("######################end#########################\n");
		printf("###################check read attr struct#################\n");
		table_item_p=(index_table_item_t *) (table_buf + 8);
		while(table_item_p < table_buf +8 +attr_table_size)
		{
			if(record_file_seekto( file, table_item_p->location)<0){
				dbg("error seek to attr struct \n");
				exit(0);
			}
			size = record_file_read(
					file,(unsigned char *) buf, 2);
			if(size <=0){
				dbg("error read attr struct\n");
				exit(0);
			}else{
				s = buf + table_item_p->location%512;
				if(s[0]!=0||s[1]!=0||s[2]!=0||s[3]!=1||s[4]!=0xc){
					dbg("we not found internal header error\n");
					exit(0);
				}
				e = s;
				while(e[0]!=0xff||e[1]!=0xd8||e[2]!=0xff||e[3]!=0xe0)
					e++;
				e[0]=0;
				printf("%d ,%s   ",e-s,s+9);
			}
			table_item_p++;
		}
		printf("\n");
		printf("##################check time_15sec_table###############\n");
		while(table_item_p < table_buf +8 +attr_table_size +time_table_size){
			if(record_file_seekto( file, table_item_p->location)<0){
				dbg("error seek to time struct \n");
				exit(0);
			}
			size = record_file_read(
					file,(unsigned char *) buf, 2);
			if(size <=0){
				dbg("error read time struct\n");
				exit(0);
			}else{
				e = buf + table_item_p->location%512;
				if(e[0]!=0xff||e[1]!=0xd8||e[2]!=0xff||e[3]!=0xe0){
					if(e[0]!=0||e[1]!=0||e[2]!=0||e[3]!=1||e[4]!=0xc)
					{
						dbg("error  time_15sec_table\n");
						printf("location=%u , value =%02x%02x%02x%02x\n",table_item_p->location,e[0],e[1],e[2],e[3]);
						e = (char *)table_item_p;
						printf("table_item_p = %p , base table addr = %p\n",e,table_buf);
						printf("location b = %02x %02x %02x %02x\n",e[0],e[1],e[2],e[3]);
						exit(0);
					}
				}
			}
			table_item_p++;
		}
		printf("#######################end########################\n");
		*/
		size = playback_send_data(pb,table_buf,attr_table_size+time_table_size+8);
		if(size<0)
			goto __out;
		file->real_size = old_real_size;
		dbg("send table ok\n");
	}
	
	if(record_file_seekto(file, pb->seek)<0)
		goto __out;
	/*get and send the last time stamp*/
	//gettimeofday(&old_send_time , NULL);
	size = record_file_read(
							file,(unsigned char *) buf, PLAYBACK_SECTOR_NUM_ONE_READ);
	if(size <= 0){
		 dbg("error read record data\n");
		goto __out;
	}else{
		file_header=(nand_record_file_header *)buf;
		memcpy(file_header->LastTimeStamp,file->LastTimeStamp,sizeof(file_header->LastTimeStamp));
		ret = playback_send_data(pb,buf,size);
		if(ret <= 0){
			goto __out;
		}
	}
	//snd_size = size;
	while(running){
		//printf("playback thread: %d\n", pb->thread_id);
		status = playback_get_status(pb);
		switch(status)
		{
			case PLAYBACK_STATUS_RUNNING:
			{
				size = record_file_read(
							file,(unsigned char *) buf, PLAYBACK_SECTOR_NUM_ONE_READ);
				if(size <= 0){
					//ready to restart.
					/*
					playback_set_status(pb, PLAYBACK_STATUS_PAUSED);
					 record_file_seekto(file, 0);
					 */
					 dbg("error read record data\n");
					 running = 0;
				}else{
					//send it
					ret = playback_send_data(pb,buf,size);
					if(ret < 0){
						running = 0;
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
				dbg("playback seek , pb->seek=%d\n",pb->seek);
				playback_set_status(pb, PLAYBACK_STATUS_RUNNING);
				reserve_seek = file->cur_sector *512;
				if(!table_buf)
					break;
				seek = 0;
				table_item_p=(index_table_item_t *) (table_buf + 8);
				while((char *)table_item_p < table_buf +8 +attr_table_size)
				{
					if(table_item_p->location ==pb->seek){
						seek = table_item_p->location;
						break;
					}
					if(table_item_p->location > pb->seek){
						if((char *)table_item_p == table_buf + 8)
							seek = 0;
						else{
							table_item_p --;
							seek = table_item_p->location;
						}
						break;
					}
					table_item_p ++;
					if(table_item_p == table_buf +8 +attr_table_size){
						table_item_p --;
						seek = table_item_p ->location;
						break;
					}
				}
				dbg("seek=%u , pb->seek = %u\n",seek , pb->seek);
				if(seek==0)
				{
					record_file_seekto(file,0);
					size = record_file_read(
							file,(unsigned char *) buf, 1);
					if(size <=0){
						running = 0;
						break;
					}else{
						memset(&internal_header , 0 ,sizeof(internal_header));
						file_header = (nand_record_file_header *)buf;
						memcpy(internal_header.head , file_header->head , sizeof(internal_header.head));
						internal_header.flag[0]  |=(1<<FLAG0_TS_CHANGED_BIT | 1<<FLAG0_FR_CHANGED_BIT | 
							1<<FLAG0_FW_CHANGED_BIT | 1<<FLAG0_FH_CHANGED_BIT);
						internal_header.flag[1] = 1;
						memcpy(internal_header.StartTimeStamp , file_header->StartTimeStamp , sizeof(internal_header.StartTimeStamp));
						memcpy(internal_header.FrameRateUs , file_header->FrameRateUs , sizeof(internal_header.FrameRateUs));
						memcpy(internal_header.FrameWidth , file_header->FrameWidth , sizeof(internal_header.FrameWidth));
						memcpy(internal_header.FrameHeight ,file_header->FrameHeight , sizeof(internal_header.FrameHeight));
						ret = playback_send_data(pb,&internal_header,sizeof(internal_header));
						if(ret <= 0){
							running = 0;
							break;
						}
					}
				}
				else if(seek!=pb->seek)
				{
					if(record_file_seekto( file, seek)<0)
						goto __out;
					size = record_file_read(
							file,(unsigned char *) buf, 2);
					if(size <=0){
						running = 0;
						break;
					}else{
						s = buf + seek%512;
						if(s[0]==0&&s[1]==0&&s[2]==0&&s[3]==1&&s[4]==0xc)
							dbg("ok we find internal header\n");
						else{
							dbg("we not found internal header error\n");
							record_file_seekto( file, reserve_seek);
							goto __seek_out;
						}
						e = s;
						ret =0;
						while(e[0]!=0xff||e[1]!=0xd8||e[2]!=0xff||e[3]!=0xe0){
							ret ++;
							if(ret>sizeof(nand_record_file_internal_header)+1){
								dbg("we not found the next jpeg header\n");
								record_file_seekto( file, reserve_seek);
								goto __seek_out;
							}
							e++;
						}
						ret = playback_send_data(pb,s,e -s);
						if(ret <= 0){
							running = 0;
							break;
						}
					}
					
				}
				if(record_file_seekto(file, pb->seek)<0)
					goto __out;
				size = record_file_read(
							file,(unsigned char *) buf, PLAYBACK_SECTOR_NUM_ONE_READ);
				if(size<=0){
					running = 0;
					break;
				}
				if(pb->seek==0&&seek==0)
					s=buf+sizeof(nand_record_file_header);
				else
					s = buf + pb->seek%512;
				ret = playback_send_data(pb,s,size -( s-buf));
				if(ret <= 0){
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
	dbg("exit playback thread\n");
	free(buf);
	if(table_buf)
		free(table_buf);
	record_file_close(file);
	playback_set_dead(pb);
	return NULL;
}

