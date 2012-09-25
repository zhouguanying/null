#ifndef __VIDEO_CFG_H__
#define __VIDEO_CFG_H__

#define PASSWORD_FILE					"/data/pswd.cfg"
#define PASSWORD_FAIL					"PSWD_FAIL"
#define PASSWORD_OK					"PSWD_OK"
#define PASSWORD_NOT_SET				"PSWD_NOT_SET"
#define PASSWORD_SET					"PSWD_SET"
#define PASSWORD_PART_ARG				"PSWD="

#define CURR_VIDEO_CFG_VERSION		"1.0.3"

#define APP_VERSION 					"version=6.0.5\n"
#define UDP_SERVER_ADDR  				"udp.iped.com.cn"

#define CFG_VERSION						"cfg_v"
#define CFG_CAM_ID						"id"
#define CFG_PSWD					       "pswd"
#define CFG_MONITOR_MODE 			       "mon_mode"
#define CFG_MONITOR_RATE		       	"rate"
#define CFG_COMPRESSION			        "compression"
#define CFG_MONITOR_RESOLUTION		 "reso"
#define CFG_GOP							 "gop"
#define CFG_ROTATION_ANGLE			 "angle"
#define CFG_MIRROR_ANGLE				 "mirror_angle"
#define CFG_BITRATE						 "bs"
#define CFG_BRIGHTNESS					 "brightness"
#define CFG_CONTRAST					 "contrast"
#define CFG_SATURATION					 "saturation"
#define CFG_GAIN						 "gain"
#define CFG_VOLUME						 "volume"
#define CFG_RECORD_MODE				 "r_mode"
#define CFG_RECORD_RESOLUTION			 "r_reso"
#define CFG_RECORD_NORMAL_SPEED		 "r_nor_speed"
#define CFG_RECORD_NORMAL_DURATION	 "r_nor_duration"
#define CFG_RECORD_SENSITIVITY			 "r_sensitivity"
#define CFG_RECORD_SLOW_SPEED		 "r_slow_speed"
#define CFG_RECORD_FAST_SPEED			 "r_fast_speed"
#define CFG_RECORD_FAST_DURATION		 "r_fast_duration"
#define CFG_EMAIL_ALARM				 "email_alarm"
#define CFG_MAILBOX						 "mailbox"
#define CFG_MAILSENDER					 "mailsender"
#define CFG_SENDERPSWD				 "senderpswd"
#define CFG_SENDERSMTP					 "sendersmtp"
#define CFG_SOUND_DUPLEX				 "snd_duplex"
#define CFG_UDHCPC						 "udhcpc"
#define CFG_NET_MODE					 "net_mode"
#define CFG_ETH_DEVICE					 "e_dev"
#define CFG_ETH_IP						 "e_ip"
#define CFG_ETH_MASK					 "e_mask"
#define CFG_ETH_GATEWAY				 "e_gw"
#define CFG_ETH_DNS1					 "e_dns1"
#define CFG_ETH_DNS2					 "e_dns2"
#define CFG_WLAN_DEVICE				 "w_dev"
#define CFG_WLAN_IP					 "w_ip"
#define CFG_WLAN_MASK					 "w_mask"
#define CFG_WLAN_GATEWAY				 "w_gw"
#define CFG_WLAN_DNS1					 "w_dns1"
#define CFG_WLAN_DNS2					 "w_dns2"
#define CFG_WLAN_SSID					 "w_ssid"
#define CFG_WLAN_KEY					 "w_key"


#define SYSTEM_TIME						 "sys_time"
#define TFCARD_MAXSIZE					 "tf_msize"
#define TFCARD_FREE_SIZE				 "tf_fsize"

#define CFG_VALUE_NORMAL				 "normal"
#define CFG_VALUE_INTELLIGENT			 "inteligent"

/*this file mark if this kernel is a encryption kernel or no encryption kernel 
*encryption kernel if this file exist , otherwise no encryption kernel
*/
#define ENCRYPTION_FILE_PATH 	"/encryption"

char *get_version_in_binary();

#endif
