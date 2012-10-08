#ifndef __CFG_NETWORK_H__
#define __CFG_NETWORK_H__

#define RESERVE_SCAN_FILE         "/data/wifi_last_scan.cfg"

extern char *scanresult;
extern int result_len;
extern char inet_eth_device[64];
extern char inet_wlan_device[64];
extern char inet_eth_gateway[64];
extern char inet_wlan_gateway[64];
extern char w_ssid[64];
extern char w_key[64];
extern char w_ip[64];
extern char w_mask[64];
extern char w_dns1[64];
extern char w_dns2[64];
extern char e_ip[64];
extern char e_mask[64];
extern char e_dns1[64];
extern char e_dns2[64];
int check_net_thread();
int check_net(char *ping_addr, char * __device);
char * get_parse_scan_result(int *numssid , char *wish_ssid);
int mywpa_cli(int argc, char *argv[]);
int built_net(int check_wlan0, int check_eth0 , int ping_wlan0 , int ping_eth0);
int get_gateway(char * device , char *gateway);
int get_ip(char * device , char *ip , char *mask);
int get_dns(char  *dns1 , char *dns2);
int set_dns(char  *dns1 , char *dns2);
int config_wifi();
int get_netlink_status(const char *if_name);
char * scan_wifi(int *len);
void * network_thread(void * arg);

#endif
