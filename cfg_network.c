#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <sys/msg.h>
#include <assert.h>
#include <string.h>

#include "cfg_network.h"
#include "server.h"

char w_ssid[64];
char w_key[64];

char w_ip[64];
char w_mask[64];
char w_dns1[64];
char w_dns2[64];
char e_ip[64];
char e_mask[64];
char e_dns1[64];
char e_dns2[64];

static int is_chareter(char a)
{
	if((a>='A'&&a<='Z')||(a>='a'&&a<='z'))
		return 1;
	else
		return 0;
}
static int __strncmp(char *s1 , char *s2,int n)
{
	int i;
	for(i = 0; i<n;i++){
		if(is_chareter(*s1)&&is_chareter(*s2)){
			if(*s1-*s2!=0&&abs(*s1-*s2)!=32)
				return (*s1-*s2);
		}else{
			if(*s1-*s2!=0)
				return (*s1-*s2);
		}
		s1++;
		s2++;
	}
	return 0;
}

char * get_parse_scan_result( int *numssid , char *wish_ssid)
 {
 	char *rawbuf;
	char *buf;
	int len;
	char *bssid;
	char *frequency;
	char *signal_leval;
	char *flags;
	char *ssid;
	char proto[64];
	char key_mgmt[64];
	char pairwise[64];
	char group[64];
	char *p;
	char *d;
	int i = 0;
	FILE *last_scan_fp;
	int j;
	*numssid = 0;
	buf = (char *)malloc(4096);
	if(!buf){
		printf("cannot malloc buf int parse scan result\n");
		return NULL;
	}
	memset(buf , 0 , 4096);
	if(wish_ssid!=NULL){
		last_scan_fp = fopen(RESERVE_SCAN_FILE,"r");
		if(last_scan_fp){
			while(fgets(buf,4096,last_scan_fp)!=NULL){
				p = buf;
				while(*p!='=')p++;
				p++;
				if(__strncmp(p, wish_ssid, strlen(wish_ssid))==0){
					fclose(last_scan_fp);
					*numssid = 1;
					return buf;
				}
				memset(buf,0,strlen(buf));
			}
			fclose(last_scan_fp);
		}
	}
	rawbuf = scan_wifi(&len);
	if(!rawbuf){
		printf("scan fail\n");
		free(buf);
		return NULL;
	}
	p = rawbuf +48;
	i+=48;
	d = buf;
	while(i<len){
		bssid = p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p =0;
		p++;
		i++;
		frequency = p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		signal_leval = p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		flags=p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		ssid=p;
		while(*p!='\t'&&*p!='\n'){
			p++;
			i++;
		}
		*p=0;
		p++;
		i++;
		(*numssid)++;
		memset(proto , 0, sizeof(proto));
		memset(key_mgmt , 0 , sizeof(key_mgmt));
		memset(pairwise , 0 ,sizeof(pairwise));
		memset(group , 0 ,sizeof(group));
		if(!*flags){
		}else{
			if(*flags=='[')flags++;
			if(*flags==']'){
			}else{
				if(strncmp(flags,"WEP",strlen("WEP"))==0){
					flags+=strlen("WEP");
					sprintf(key_mgmt,"NONE");
				}
				if(strncmp(flags,"WPA-PSK",strlen("WPA-PSK"))==0){
					flags+=strlen("WPA-PSK");
					sprintf(key_mgmt,"WPA-PSK");
					sprintf(proto,"WPA");
					if(*flags=='-'){
						flags++;
						for(j=0;*flags!=']';j++){
							if(*flags=='+'){
								group[j]=' ';
								flags++;
							}else if(*flags=='-'){
								while(*flags!=']'&&*flags!='+')flags++;
								j--;
							}else{
								group[j]=*flags;
								flags++;
							}
						}
						memcpy(pairwise , group,sizeof(pairwise));
					}
				}
				if(strncmp(flags,"WPA2-PSK",strlen("WPA2-PSK"))==0){
					flags+=strlen("WPA2-PSK");
					sprintf(key_mgmt,"WPA-PSK");
					sprintf(proto,"WPA2");
					if(*flags=='-'){
						flags++;
						for(j=0;*flags!=']';j++){
							if(*flags=='+'){
								group[j]=' ';
								flags++;
							}else if(*flags=='-'){
								while(*flags!=']'&&*flags!='+')flags++;
								j--;
							}else{
								group[j]=*flags;
								flags++;
							}
						}
						memcpy(pairwise , group,sizeof(pairwise));
					}
				}
			}
		}
		sprintf(d,"ssid=%s\tsignal_level=%s\tproto=%s\tkey_mgmt=%s\tpairwise=%s\tgroup=%s\n",ssid,signal_leval,proto,key_mgmt,pairwise,group);
		//printf("%s",d);
		d+=strlen(d);
	}
	last_scan_fp = fopen(RESERVE_SCAN_FILE,"w");
	if(!last_scan_fp){
		printf("save the scan result error :cannot open file for write\n");
	}else{
		fwrite(buf,1,strlen(buf),last_scan_fp);
		fclose(last_scan_fp);
	}
	free(rawbuf);
	return buf;
 }

int get_gateway(char * device ,char *gateway){
	char buf[512];
	FILE * routefp;
	memset(buf,0,512);
	sprintf(buf,"route | grep default | grep %s > /tmp/route",device);
	system(buf);
	routefp = fopen("/tmp/route","r");
	if(!routefp){
		printf("get eth default gateway error\n");
		return -1;
	}
	memset(buf,0,512);
	if(fgets(buf,512,routefp)!=NULL){
		char *src = buf;
		char *dst;
		while(*src==' ' || *src=='\t')src++;
		while(*src!=' '&&*src!='\t')src++;
		while(*src==' ' || *src=='\t')src++;
		dst = gateway;
		while(*src!=' '&&*src!='\t'){
			*dst = *src;
			dst++;
			src++;
		}
	}else{
		fclose(routefp);
		system("rm /tmp/route");
		return -1;
	}
	fclose(routefp);
	printf("%s gateway==%s\n",device,gateway);
	system("rm /tmp/route");
	return 0;
}

int get_ip(char * device , char *ip , char *mask)
{
     struct ifconf ifconf;   
     char buf[1024];        
    ifconf.ifc_len = 1024;  
    ifconf.ifc_buf = buf;   

    struct ifreq *ifreq;    
    ifreq = ifconf.ifc_req;  
     int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(sockfd, SIOCGIFCONF, &ifconf); 
    while(ifconf.ifc_len >= sizeof(struct ifreq))
    {
        struct ifreq brdinfo;
	if(strncmp(device,ifreq->ifr_name,strlen(device))==0){
	        printf("device = %s\n", ifreq->ifr_name);  
	        strcpy(ip,inet_ntoa((((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr)));
	        printf("%s  ip = %s\n",device, ip);
	        strcpy(brdinfo.ifr_name, ifreq->ifr_name);
	        ioctl(sockfd, SIOCGIFNETMASK, &brdinfo);     
		strcpy(mask,inet_ntoa(((struct sockaddr_in*)&(brdinfo.ifr_addr))->sin_addr));
	        printf("%s net mask   = %s\n",device, mask);
		close(sockfd);
		return 0;
    	}
        ifreq ++;
        ifconf.ifc_len -= sizeof(struct ifreq);
    }
	close(sockfd);
	return -1;
}

int get_dns(char  *dns1 , char *dns2)
{
	FILE *dnsfp;
	char buf[512];
	char *dns[2];
	char *p;
	int i ;
	dns1[0]=0;
	dns2[0]=0;
	dns[0] = dns1;
	dns[1] = dns2;
	dnsfp = fopen("/tmp/resolv.conf","r");
	if(!dnsfp){
		printf("cannot open resolv.conf \n");
		return -1;
	}
	memset(buf,0,512);
	i = 0;
	while(fgets(buf,512,dnsfp)!=NULL&&i<2){
		p= buf;
		while(*p==' ' || *p=='\t')p++;
		if(strncmp(p,"nameserver",strlen("nameserver"))==0){
			p+=strlen("nameserver");
			while(*p==' ' || *p=='\t')p++;
			while(*p!=' '&&*p!='\t'&&*p!='\n'&&*p!=0){
				*dns[i]=*p;
				dns[i]++;
				p++;
			}
			i++;
		}
		memset(buf,0,512);
	}
	fclose(dnsfp);
	return 0;
}
int set_dns(char  *dns1 , char *dns2)
{
	FILE *dnsfp;
	char buf[512];
	dnsfp = fopen("/tmp/resolv.conf","w");
	if(!dnsfp){
		printf("cannot open resolve.conf for write\n");
		return -1;
	}
	memset(buf,0,512);
	sprintf(buf,"nameserver  %s\n",dns1);
	fwrite(buf,1,strlen(buf),dnsfp);
	memset(buf,0,512);
	sprintf(buf,"nameserver  %s\n",dns2);
	fwrite(buf,1,strlen(buf),dnsfp);
	fflush(dnsfp);
	fclose(dnsfp);
	return 0;
}
//"ssid=%s\tsignal_level=%s\tproto=%s\tkey_mgmt=%s\tpairwise=%s\tgroup=%s\n"

int config_wifi()
{
	int i;
	char buf[256];
	char *argv[4];
	char network_id[32];
	int numssid;
	char *p;
	char * parse_result = NULL;
	struct stat st;
	char *ssid = NULL,*signal_level= NULL,*proto= NULL,*key_mgmt= NULL,*pairwise= NULL , *group= NULL;
	for(i=0;i<4;i++){
		argv[i] = (char *)malloc(256);
		if(!argv[i]){
			printf("cannot malloc buff to configure wifi something wrong\n");
			exit(0);
		}
		memset(argv[i],0,256);
	}
	if(stat("/data/wpa.conf",&st)<0)
		system("cp /etc/wpa.conf  /data/wpa.conf");
	system("mkdir /tmp/wpa_supplicant");
	system("killall wpa_supplicant");
	sleep(1);
	system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
	sleep(5);

	memset(buf,0,256);
	//extract_value(conf_p, lines, CFG_WLAN_SSID, 1, buf);
	memcpy(buf ,w_ssid , sizeof(w_ssid));
	printf("inet_wlan_ssid = %s\n",buf);
	if(!buf[0]){
		printf("error ssid\n");
		scanresult = NULL;
		goto error;
	}
	
	parse_result = get_parse_scan_result(& numssid, buf);
	if(!parse_result){
		scanresult = NULL;
		goto error;
	}
	
	p = parse_result;
	while(*p){
		ssid = p;
		signal_level = ssid;
		while(*signal_level!='\t')signal_level++;
		*signal_level = 0;
		signal_level++;
		proto = signal_level;
		while(*proto!='\t')proto++;
		*proto = 0;
		proto++;
		key_mgmt = proto;
		while(*key_mgmt!='\t')key_mgmt++;
		*key_mgmt=0;
		key_mgmt++;
		pairwise = key_mgmt;
		while(*pairwise!='\t')pairwise++;
		*pairwise = 0;
		pairwise++;
		group = pairwise;
		while(*group!='\t')group++;
		*group = 0;
		group++;
		p = group;
		while(*p!='\n')p++;
		*p=0;
		p++;
		while(*ssid!='=')ssid++;
		ssid++;
		while(*signal_level!='=')signal_level++;
		signal_level++;
		while(*proto!='=')proto++;
		proto++;
		while(*key_mgmt!='=')key_mgmt++;
		key_mgmt++;
		while(*pairwise!='=')pairwise++;
		pairwise++;
		while(*group!='=')group++;
		group++;
		if(__strncmp(buf,ssid,strlen(buf))==0){
			break;
		}
	}
	if(!*ssid){
		scanresult  = NULL;
		goto error;
	}
	if(__strncmp(buf,ssid,strlen(ssid))!=0){
		printf("*********error cannot scan the specify ssid***********\n");
		scanresult = NULL;
		goto error;
	}

	scanresult = (char *)malloc(2048);
	if(!scanresult){
		printf("malloc buff for scanresult error\n");
		goto error;
	}
	
	sprintf(argv[0],"remove_network");
	sprintf(argv[1],"0");
	mywpa_cli(2,  argv);
	
	sprintf(argv[0],"ap_scan");
	sprintf(argv[1],"1");
	mywpa_cli(2,  argv);
	
	sprintf(argv[0],"add_network");
	mywpa_cli(1,argv);
	if(strncmp(scanresult,"FAIL",strlen("FAIL"))==0){
		goto error;
	}
	memset(network_id , 0 ,sizeof(network_id));
	memcpy(network_id , scanresult , result_len-1);
	
	sprintf(argv[0],"set_network");
	sprintf(argv[1],"%s",network_id);
	sprintf(argv[2],"ssid");
	sprintf(argv[3],"\"%s\"",ssid);
	printf("try ssid\n");
	mywpa_cli(4,  argv );
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}
	/*
	memset(buf,0,256);
	extract_value(conf_p, lines, "inet_wlan_mode", 1, buf);
	printf("inet_wlan_mode = %s\n",buf);
	if(buf[0]){
		sprintf(argv[0],"set_network");
		//sprintf(argv[1],"0");
		sprintf(argv[1],"%s",network_id);
		sprintf(argv[2],"mode");
		sprintf(argv[3],"%s",buf);
		printf("try mode\n");
		mywpa_cli(4,  argv );
		if(strncmp(scanresult,"OK",strlen("OK"))!=0){
			goto error;
		}
	}
	*/
	//memset(buf,0,256);
	//extract_value(conf_p, lines, "inet_wlan_key_mgmt", 1, buf);
	printf("inet_wlan_key_mgmt = %s\n",key_mgmt);
	sprintf(argv[0],"set_network");
	sprintf(argv[1],"%s",network_id);
	sprintf(argv[2],"key_mgmt");
	sprintf(argv[3],"%s",key_mgmt);
	printf("try key_mgmt\n");
	mywpa_cli(4,  argv );
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}

	
	if(strncmp(key_mgmt, "WPA-PSK",7)==0){
		//memset(buf,0,256);
		//extract_value(conf_p, lines, "inet_wlan_proto", 1, buf);
		printf("inet_wlan_proto = %s\n",proto);
		if(*proto){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"proto");
			sprintf(argv[3],"%s",proto);
			printf("try proto\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

		//memset(buf,0,256);
		//extract_value(conf_p, lines, "inet_wlan_group", 1, buf);
		printf("inet_wlan_group = %s\n",group);
		if(*group){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"group");
			sprintf(argv[3],"%s",group);
			printf("try group\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

	//	memset(buf,0,256);
		//extract_value(conf_p, lines, "inet_wlan_pairwise", 1, buf);
		printf("inet_wlan_pairwise = %s\n",pairwise);
		if(*pairwise){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"pairwise");
			sprintf(argv[3],"%s",pairwise);
			printf("try pairwise\n");
			mywpa_cli(4,  argv );
			/*
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
			*/
		}

		memset(buf,0,256);
		//extract_value(conf_p, lines, CFG_WLAN_KEY, 1, buf);
		memcpy(buf , w_key , sizeof(w_key));
		printf("inet_wlan_psk = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"psk");
			sprintf(argv[3],"\"%s\"",buf);
			printf("try psk\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}
	}else{
		memset(buf,0,256);
		//extract_value(conf_p, lines, CFG_WLAN_KEY, 1, buf);
		memcpy(buf , w_key , sizeof(w_key));
		printf("inet_wlan_wep_key0 = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"wep_key0");
			sprintf(argv[3],"%s",buf);
			printf("try wep_key0\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

		/*
		memset(buf,0,256);
		extract_value(conf_p, lines, "inet_wlan_wep_key1", 1, buf);
		printf("inet_wlan_wep_key1 = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"wep_key1");
			sprintf(argv[3],"%s",buf);
			printf("try wep_key1\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}

		memset(buf,0,256);
		extract_value(conf_p, lines, "inet_wlan_wep_key2", 1, buf);
		printf("inet_wlan_wep_key2 = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"wep_key2");
			sprintf(argv[3],"%s",buf);
			printf("try wep_key2\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}
		*/
		if(buf[0]){
			memset(buf,0,256);
			//extract_value(conf_p, lines, "inet_wlan_wep_tx_keyindx", 1, buf);
			//printf("inet_wlan_wep_tx_keyindx = %s\n",buf);
			if(!buf[0])
				sprintf(buf,"0");
			if(buf[0]){
				sprintf(argv[0],"set_network");
				//sprintf(argv[1],"0");
				sprintf(argv[1],"%s",network_id);
				sprintf(argv[2],"wep_tx_keyidx");
				sprintf(argv[3],"%s",buf);
				printf("try wep_tx_keyidx\n");
				mywpa_cli(4,  argv );
				if(strncmp(scanresult,"OK",strlen("OK"))!=0){
					goto error;
				}
			}
		}

		/*
		memset(buf,0,256);
		extract_value(conf_p, lines, "inet_wlan_auth_alg", 1, buf);
		printf("inet_wlan_auth_alg = %s\n",buf);
		if(buf[0]){
			sprintf(argv[0],"set_network");
			//sprintf(argv[1],"0");
			sprintf(argv[1],"%s",network_id);
			sprintf(argv[2],"auth_alg");
			sprintf(argv[3],"%s",buf);
			printf("try auth_alg\n");
			mywpa_cli(4,  argv );
			if(strncmp(scanresult,"OK",strlen("OK"))!=0){
				goto error;
			}
		}
		*/
	}
	
	
	sprintf(argv[0],"select_network");
	//sprintf(argv[1],"0");
	sprintf(argv[1],"%s",network_id);
	printf("try select_network\n");
	mywpa_cli(2,argv);
	if(strncmp(scanresult,"OK",strlen("OK"))!=0){
		goto error;
	}
	free(scanresult);
	free(parse_result);
	for(i=0;i<4;i++)
		  free(argv[i]);
	return 0;
error:
	if(scanresult)
		free(scanresult);
	if(parse_result)
		free(parse_result);
	for(i=0;i<4;i++)
			free(argv[i]);
	return -1;
}

int get_netlink_status(const char *if_name)

{

    int skfd,err;

    struct ifreq ifr;

    struct ethtool_value edata;

    edata.cmd = ETHTOOL_GLINK;

    edata.data = 0;

    memset(&ifr, 0, sizeof(ifr));

    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name) - 1);

    ifr.ifr_data = (char *) &edata;

    if ((skfd=socket(AF_INET,SOCK_DGRAM,0))==0)
  {
    printf("socket error:%s\n",strerror(skfd));
        return -1;
  }

    if((err=ioctl( skfd, SIOCETHTOOL, &ifr )) == -1)

    {
    printf("ioctl error:%s\n",strerror(err));
        close(skfd);
        return -1;

    }

    close(skfd);
    return edata.data;

}

 char * scan_wifi(int *len)
 { 	
 	struct stat st;
	int i;
	int j;
	char *argv[4];
	int tryscan;
 	scanresult = (char *)malloc(4096);
	if(!scanresult){
		printf("cannot malloc buf for scanresult\n");
		return NULL;
	}
	system("ps -efww |grep wpa_supplicant | grep -v grep > /tmp/twpa");
	stat("/tmp/twpa", &st);
	if(st.st_size <=0){
		printf("start wpa_supplicant\n");
		if(stat("/data/wpa.conf",&st)<0)
			system("cp /etc/wpa.conf  /data/wpa.conf");
		system("mkdir /tmp/wpa_supplicant");
		system("wpa_supplicant -Dwext -iwlan0 -c/data/wpa.conf -B");
		sleep(3);
	}
	system("rm /tmp/twpa");
	for(i=0;i<4;i++){
		argv[i]=malloc(256);
		if(!argv[i]){
			printf("can not malloc buf for scan wifi\n");
			for(j=0;j<i;j++)
				free(argv[j]);
			free(scanresult);
			return NULL;
		}
	}
	
	sprintf(argv[0],"ap_scan");
	sprintf(argv[1],"1");
	mywpa_cli(2,  argv);
	sleep(1);
	
	printf("###########begin scan###########\n");
	tryscan = 5;
	while(tryscan){
		sprintf(argv[0],"scan");
		mywpa_cli(1,argv);
		sleep(6);
		sprintf(argv[0],"scan_results");
		mywpa_cli(1,argv);
		if(result_len>48)
			break;
		tryscan --;
	}
	for(i= 0; i < 4; i++)
		free(argv[i]);
	printf("############end###############\n");
	*len = result_len;
	if(tryscan>0)
		return scanresult;
	free(scanresult);
	return NULL;
 }

 void cfg_network()
 {
 	char buf[512];
 	if(threadcfg.inet_udhcpc){
		system("killall udhcpc");
		if(strncmp(threadcfg.inet_mode,"eth_only",strlen("eth_only"))==0
		||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0)
		{
			memset(buf,0,512);
			sprintf(buf,"udhcpc -i %s &",inet_eth_device);
			system(buf);
		}

		if(strncmp(threadcfg.inet_mode,"wlan_only",strlen("wlan_only"))==0
		||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0)
		{
			memset(buf,0,512);
			sprintf(buf,"udhcpc -i %s &",inet_wlan_device);
			system(buf);
		}
 	}
 	/*
	char buf[512];
	system("killall udhcpc");
	if(strncmp(threadcfg.inet_mode,"eth_only",strlen("eth_only"))==0
		||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0){
		printf("------------configure eth----------------\n");
			if(threadcfg.inet_udhcpc){
			eth_dhcp:
				memset(buf,0,512);
				sprintf(buf,"udhcpc -i %s &",inet_eth_device);
				system(buf);
			}else{
				if(e_dns1[0]||e_dns2[0]){
					set_dns(e_dns1, e_dns2);
					memset(buf,0,512);
					sprintf(buf,"ifconfig %s down",inet_eth_device);
					system(buf);
					sleep(1);
					sprintf(buf,"ifconfig %s up",inet_eth_device);
					system(buf);
					sleep(1);
				}else{
					printf("it is not dhcp mode but the dns is not set , something wrong\n");
					goto eth_dhcp;
				}
				if(e_ip[0]&&e_mask[0]){
					sprintf(buf,"ifconfig %s %s netmask %s",inet_eth_device,e_ip, e_mask);
					printf("before set ip and mask buf==%s\n",buf);
					system(buf);
					sleep(1);
				}else{
					printf("it is not dhcp mode but the ip or mask not set , something wrong\n");
					goto eth_dhcp;
				}
				printf("inet_eth_gateway = %s\n",inet_eth_gateway);
				if(!inet_eth_gateway[0]){
					printf("it is not dhcp mode but the gateway not set\n");
				}else{
					sprintf(buf,"route add default   gw  %s  %s",inet_eth_gateway,inet_eth_device);
					printf("before set gateway buf==%s\n",buf);
					system(buf);
					sleep(1);
				}
			}
	}
	if(strncmp(threadcfg.inet_mode,"wlan_only",strlen("wlan_only"))==0
		||strncmp(threadcfg.inet_mode,"inteligent",strlen("inteligent"))==0){
		printf("------------configure wlan----------------\n");
		if(config_wifi()<0){	
			printf("configure wifi error check your data\n");
		}else{
			if(threadcfg.inet_udhcpc){
			wlan_udhcpc:
				memset(buf,0,512);
				sprintf(buf,"udhcpc -i %s &",inet_wlan_device);
				system(buf);
			}else{
				
				if(w_dns1[0]||w_dns2[0]){
					set_dns(w_dns1, w_dns2);
					memset(buf,0,512);
					sprintf(buf,"ifconfig %s down",inet_wlan_device);
					system(buf);
					sleep(1);
					sprintf(buf,"ifconfig %s up",inet_wlan_device);
					system(buf);
					sleep(1);
				}else{
					printf("it is not dhcpc mode but the dns not set\n");
					goto wlan_udhcpc;
				}
				
				if(w_ip[0]&&w_mask[0]){
					sprintf(buf,"ifconfig %s %s netmask %s",inet_wlan_device,w_ip, w_mask);
					printf("before set ip and mask buf==%s\n",buf);
					system(buf);
					sleep(1);
				}else{
					printf("it is not udhcpc mode but the ip or mask not set\n");
					goto wlan_udhcpc;
				}
				
				printf("inet_wlan_gateway = %s\n",inet_wlan_gateway);
				
				sprintf(buf,"route add default   gw  %s  %s",inet_wlan_gateway,inet_wlan_device);
				printf("before set gateway buf==%s\n",buf);
				system(buf);
				sleep(1);
				
			}
		}
	}
	*/
 }

void *network_thread(void *arg)
{
	sleep(60);
	for(;;){
		if(check_net("www.baidu.com\0" , inet_eth_device)==0)
			goto done;
		if(check_net("www.baidu.com\0" , inet_wlan_device)==0)
			goto done;
		if(check_net(inet_eth_gateway, inet_eth_device)==0)
			goto done;
		if(check_net(inet_wlan_gateway , inet_wlan_device)==0)
			goto done;
		cfg_network();
	done:
		sleep(60 *5);
	}
	return NULL;
}


