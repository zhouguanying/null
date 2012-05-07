#ifndef __UDP_TRANSFER_H__
#define __UDP_TRANSFER_H__
#include "inet_type.h"

int start_udp_transfer();
int monitor_try_connected_thread();
struct udp_transfer *get_udp_transfer(uint32_t addr,uint16_t cliport);
struct mapping *remove_from_ready_list(uint32_t addr,uint16_t cliport);
void add_to_ready_list(struct mapping*p);
int set_ready_struct_connected(uint32_t ip,uint16_t cliport);
int set_read_struct_pb_running(uint32_t ip,uint16_t cliport);
void delete_timeout_mapping(uint32_t ip,uint16_t cliport);
void touch_cli_port(uint32_t ip, uint16_t port);
int touch_playback_port(uint16_t localport,uint32_t ip,uint16_t port);
#endif
