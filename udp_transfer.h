#ifndef __UDP_TRANSFER_H__
#define __UDP_TRANSFER_H__
#include "inet_type.h"

int start_udp_transfer();
int remove_from_ready_list(uint32_t addr,uint16_t cliport);
void add_to_ready_list(struct mapping*p);
int set_ready_struct_connected(uint32_t ip,uint16_t cliport);
void delete_timeout_mapping(uint32_t ip,uint16_t cliport);
int touch_playback_port(uint16_t localport,uint32_t ip,uint16_t port);
#endif
