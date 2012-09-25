#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <alsa/asoundlib.h>
#include <sys/poll.h>
#include "amixer.h"
#include "server.h"

#define LEVEL_BASIC		(1<<0)
#define LEVEL_INACTIVE		(1<<1)
#define LEVEL_ID		(1<<2)

static int quiet = 0;
static int debugflag = 0;
static int no_check = 0;
static int ignore_error = 0;
static char card[64] = "default";

static int volume_percent = -1;

static void error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "amixer: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static const char *control_iface(snd_ctl_elem_id_t *id)
{
	return snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(id));
}

static const char *control_type(snd_ctl_elem_info_t *info)
{
	return snd_ctl_elem_type_name(snd_ctl_elem_info_get_type(info));
}

static const char *control_access(snd_ctl_elem_info_t *info)
{
	static char result[10];
	char *res = result;

	*res++ = snd_ctl_elem_info_is_readable(info) ? 'r' : '-';
	*res++ = snd_ctl_elem_info_is_writable(info) ? 'w' : '-';
	*res++ = snd_ctl_elem_info_is_inactive(info) ? 'i' : '-';
	*res++ = snd_ctl_elem_info_is_volatile(info) ? 'v' : '-';
	*res++ = snd_ctl_elem_info_is_locked(info) ? 'l' : '-';
	*res++ = snd_ctl_elem_info_is_tlv_readable(info) ? 'R' : '-';
	*res++ = snd_ctl_elem_info_is_tlv_writable(info) ? 'W' : '-';
	*res++ = snd_ctl_elem_info_is_tlv_commandable(info) ? 'C' : '-';
	*res++ = '\0';
	return result;
}

#define check_range(val, min, max) \
	(no_check ? (val) : ((val < min) ? (min) : (val > max) ? (max) : (val))) 

/* Function to convert from percentage to volume. val = percentage */

#define convert_prange1(val, min, max) \
	ceil((val) * ((max) - (min)) * 0.01 + (min))

static long get_integer(char **ptr, long min, long max)
{
	long val = min;
	char *p = *ptr, *s;

	if(volume_percent>=0){
		val =(int) (max*volume_percent/100);
		val = check_range(val, min, max);
		return val;
	}
	if (*p == ':')
		p++;
	if (*p == '\0' || (!isdigit(*p) && *p != '-'))
		goto out;

	s = p;
	val = strtol(s, &p, 10);
	if (*p == '.') {
		p++;
		strtol(p, &p, 10);
	}
	if (*p == '%') {
		val = (long)convert_prange1(strtod(s, NULL), min, max);
		p++;
	}
	val = check_range(val, min, max);
	if (*p == ',')
		p++;
 out:
	*ptr = p;
	return val;
}

static long get_integer64(char **ptr, long long min, long long max)
{
	long long val = min;
	char *p = *ptr, *s;

	if (*p == ':')
		p++;
	if (*p == '\0' || (!isdigit(*p) && *p != '-'))
		goto out;

	s = p;
	val = strtol(s, &p, 10);
	if (*p == '.') {
		p++;
		strtol(p, &p, 10);
	}
	if (*p == '%') {
		val = (long long)convert_prange1(strtod(s, NULL), min, max);
		p++;
	}
	val = check_range(val, min, max);
	if (*p == ',')
		p++;
 out:
	*ptr = p;
	return val;
}

static void show_control_id(snd_ctl_elem_id_t *id)
{
	unsigned int index, device, subdevice;
	printf("numid=%u,iface=%s,name='%s'",
	       snd_ctl_elem_id_get_numid(id),
	       control_iface(id),
	       snd_ctl_elem_id_get_name(id));
	index = snd_ctl_elem_id_get_index(id);
	device = snd_ctl_elem_id_get_device(id);
	subdevice = snd_ctl_elem_id_get_subdevice(id);
	if (index)
		printf(",index=%i", index);
	if (device)
		printf(",device=%i", device);
	if (subdevice)
		printf(",subdevice=%i", subdevice);
}

static void print_spaces(unsigned int spaces)
{
	while (spaces-- > 0)
		putc(' ', stdout);
}

static void print_dB(long dB)
{
	printf("%li.%02lidB", dB / 100, (dB < 0 ? -dB : dB) % 100);
}

static void decode_tlv(unsigned int spaces, unsigned int *tlv, unsigned int tlv_size)
{
	unsigned int type = tlv[0];
	unsigned int size;
	unsigned int idx = 0;

	if (tlv_size < 2 * sizeof(unsigned int)) {
		printf("TLV size error!\n");
		return;
	}
	print_spaces(spaces);
	printf("| ");
	type = tlv[idx++];
	size = tlv[idx++];
	tlv_size -= 2 * sizeof(unsigned int);
	if (size > tlv_size) {
		printf("TLV size error (%i, %i, %i)!\n", type, size, tlv_size);
		return;
	}
	switch (type) {
	case SND_CTL_TLVT_CONTAINER:
		size += sizeof(unsigned int) -1;
		size /= sizeof(unsigned int);
		while (idx < size) {
			if (tlv[idx+1] > (size - idx) * sizeof(unsigned int)) {
				printf("TLV size error in compound!\n");
				return;
			}
			decode_tlv(spaces + 2, tlv + idx, tlv[idx+1]);
			idx += 2 + (tlv[1] + sizeof(unsigned int) - 1) / sizeof(unsigned int);
		}
		break;
	case SND_CTL_TLVT_DB_SCALE:
		printf("dBscale-");
		if (size != 2 * sizeof(unsigned int)) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
		} else {
			printf("min=");
			print_dB((int)tlv[2]);
			printf(",step=");
			print_dB(tlv[3] & 0xffff);
			printf(",mute=%i", (tlv[3] >> 16) & 1);
		}
		break;
#ifdef SND_CTL_TLVT_DB_LINEAR
	case SND_CTL_TLVT_DB_LINEAR:
		printf("dBlinear-");
		if (size != 2 * sizeof(unsigned int)) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
		} else {
			printf("min=");
			print_dB(tlv[2]);
			printf(",max=");
			print_dB(tlv[3]);
		}
		break;
#endif
#ifdef SND_CTL_TLVT_DB_RANGE
	case SND_CTL_TLVT_DB_RANGE:
		printf("dBrange-\n");
		if ((size % (6 * sizeof(unsigned int))) != 0) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
			break;
		}
		while (size > 0) {
			print_spaces(spaces + 2);
			printf("rangemin=%i,", tlv[idx++]);
			printf(",rangemax=%i\n", tlv[idx++]);
			decode_tlv(spaces + 4, tlv + idx, 4 * sizeof(unsigned int));
			idx += 4;
			size -= 6 * sizeof(unsigned int);
		}
		break;
#endif
#ifdef SND_CTL_TLVT_DB_MINMAX
	case SND_CTL_TLVT_DB_MINMAX:
	case SND_CTL_TLVT_DB_MINMAX_MUTE:
		if (type == SND_CTL_TLVT_DB_MINMAX_MUTE)
			printf("dBminmaxmute-");
		else
			printf("dBminmax-");
		if (size != 2 * sizeof(unsigned int)) {
			while (size > 0) {
				printf("0x%08x,", tlv[idx++]);
				size -= sizeof(unsigned int);
			}
		} else {
			printf("min=");
			print_dB(tlv[2]);
			printf(",max=");
			print_dB(tlv[3]);
		}
		break;
#endif
	default:
		printf("unk-%i-", type);
		while (size > 0) {
			printf("0x%08x,", tlv[idx++]);
			size -= sizeof(unsigned int);
		}
		break;
	}
	putc('\n', stdout);
}

static int show_control(const char *space, snd_hctl_elem_t *elem,
			int level)
{
	int err;
	unsigned int item, idx, count, *tlv;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *control;
	snd_aes_iec958_t iec958;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_value_alloca(&control);
	if ((err = snd_hctl_elem_info(elem, info)) < 0) {
		error("Control %s snd_hctl_elem_info error: %s\n", card, snd_strerror(err));
		return err;
	}
	if (level & LEVEL_ID) {
		snd_hctl_elem_get_id(elem, id);
		show_control_id(id);
		printf("\n");
	}
	count = snd_ctl_elem_info_get_count(info);
	type = snd_ctl_elem_info_get_type(info);
	printf("%s; type=%s,access=%s,values=%i", space, control_type(info), control_access(info), count);
	switch (type) {
	case SND_CTL_ELEM_TYPE_INTEGER:
		printf(",min=%li,max=%li,step=%li\n", 
		       snd_ctl_elem_info_get_min(info),
		       snd_ctl_elem_info_get_max(info),
		       snd_ctl_elem_info_get_step(info));
		break;
	case SND_CTL_ELEM_TYPE_INTEGER64:
		printf(",min=%Li,max=%Li,step=%Li\n", 
		       snd_ctl_elem_info_get_min64(info),
		       snd_ctl_elem_info_get_max64(info),
		       snd_ctl_elem_info_get_step64(info));
		break;
	case SND_CTL_ELEM_TYPE_ENUMERATED:
	{
		unsigned int items = snd_ctl_elem_info_get_items(info);
		printf(",items=%u\n", items);
		for (item = 0; item < items; item++) {
			snd_ctl_elem_info_set_item(info, item);
			if ((err = snd_hctl_elem_info(elem, info)) < 0) {
				error("Control %s element info error: %s\n", card, snd_strerror(err));
				return err;
			}
			printf("%s; Item #%u '%s'\n", space, item, snd_ctl_elem_info_get_item_name(info));
		}
		break;
	}
	default:
		printf("\n");
		break;
	}
	if (level & LEVEL_BASIC) {
		if (!snd_ctl_elem_info_is_readable(info))
			goto __skip_read;
		if ((err = snd_hctl_elem_read(elem, control)) < 0) {
			error("Control %s element read error: %s\n", card, snd_strerror(err));
			return err;
		}
		printf("%s: values=", space);
		for (idx = 0; idx < count; idx++) {
			if (idx > 0)
				printf(",");
			switch (type) {
			case SND_CTL_ELEM_TYPE_BOOLEAN:
				printf("%s", snd_ctl_elem_value_get_boolean(control, idx) ? "on" : "off");
				break;
			case SND_CTL_ELEM_TYPE_INTEGER:
				printf("%li", snd_ctl_elem_value_get_integer(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_INTEGER64:
				printf("%Li", snd_ctl_elem_value_get_integer64(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED:
				printf("%u", snd_ctl_elem_value_get_enumerated(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_BYTES:
				printf("0x%02x", snd_ctl_elem_value_get_byte(control, idx));
				break;
			case SND_CTL_ELEM_TYPE_IEC958:
				snd_ctl_elem_value_get_iec958(control, &iec958);
				printf("[AES0=0x%02x AES1=0x%02x AES2=0x%02x AES3=0x%02x]",
				       iec958.status[0], iec958.status[1],
				       iec958.status[2], iec958.status[3]);
				break;
			default:
				printf("?");
				break;
			}
		}
		printf("\n");
	      __skip_read:
		if (!snd_ctl_elem_info_is_tlv_readable(info))
			goto __skip_tlv;
		tlv = malloc(4096);
		if ((err = snd_hctl_elem_tlv_read(elem, tlv, 4096)) < 0) {
			error("Control %s element TLV read error: %s\n", card, snd_strerror(err));
			free(tlv);
			return err;
		}
		decode_tlv(strlen(space), tlv, 4096);
		free(tlv);
	}
      __skip_tlv:
	return 0;
}

static int parse_control_id(const char *str, snd_ctl_elem_id_t *id)
{
	int c, size, numid;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			str += 6;
			numid = atoi(str);
			if (numid <= 0) {
				fprintf(stderr, "amixer: Invalid numid %d\n", numid);
				return -EINVAL;
			}
			snd_ctl_elem_id_set_numid(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "iface=", 6)) {
			str += 6;
			if (!strncasecmp(str, "card", 4)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
				str += 4;
			} else if (!strncasecmp(str, "mixer", 5)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
				str += 5;
			} else if (!strncasecmp(str, "pcm", 3)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
				str += 3;
			} else if (!strncasecmp(str, "rawmidi", 7)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_RAWMIDI);
				str += 7;
			} else if (!strncasecmp(str, "timer", 5)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_TIMER);
				str += 5;
			} else if (!strncasecmp(str, "sequencer", 9)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_SEQUENCER);
				str += 9;
			} else {
				return -EINVAL;
			}
		} else if (!strncasecmp(str, "name=", 5)) {
			char buf[64];
			str += 5;
			ptr = buf;
			size = 0;
			if (*str == '\'' || *str == '\"') {
				c = *str++;
				while (*str && *str != c) {
					if (size < (int)sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				if (*str == c)
					str++;
			} else {
				while (*str && *str != ',') {
					if (size < (int)sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
			}
			*ptr = '\0';
			snd_ctl_elem_id_set_name(id, buf);
		} else if (!strncasecmp(str, "index=", 6)) {
			str += 6;
			snd_ctl_elem_id_set_index(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "device=", 7)) {
			str += 7;
			snd_ctl_elem_id_set_device(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "subdevice=", 10)) {
			str += 10;
			snd_ctl_elem_id_set_subdevice(id, atoi(str));
			while (isdigit(*str))
				str++;
		}
		if (*str == ',') {
			str++;
		} else {
			if (*str)
				return -EINVAL;
		}
	}			
	return 0;
}

static int get_ctl_enum_item_index(snd_ctl_t *handle, snd_ctl_elem_info_t *info,
				   char **ptrp)
{
	char *ptr = *ptrp;
	int items, i, len;
	const char *name;
	
	items = snd_ctl_elem_info_get_items(info);
	if (items <= 0)
		return -1;

	for (i = 0; i < items; i++) {
		snd_ctl_elem_info_set_item(info, i);
		if (snd_ctl_elem_info(handle, info) < 0)
			return -1;
		name = snd_ctl_elem_info_get_item_name(info);
		len = strlen(name);
		if (! strncmp(name, ptr, len)) {
			if (! ptr[len] || ptr[len] == ',' || ptr[len] == '\n') {
				ptr += len;
				*ptrp = ptr;
				return i;
			}
		}
	}
	return -1;
}

static int cset(int argc, char *argv[], int roflag, int keep_handle)
{
	int err;
	static snd_ctl_t *handle = NULL;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	char *ptr;
	unsigned int idx, count;
	long tmp;
	snd_ctl_elem_type_t type;
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&control);

	if (argc < 1) {
		fprintf(stderr, "Specify a full control identifier: [[iface=<iface>,][name='name',][index=<index>,][device=<device>,][subdevice=<subdevice>]]|[numid=<numid>]\n");
		return -EINVAL;
	}
	if (parse_control_id(argv[0], id)) {
		fprintf(stderr, "Wrong control identifier: %s\n", argv[0]);
		return -EINVAL;
	}
	if (debugflag) {
		printf("VERIFY ID: ");
		show_control_id(id);
		printf("\n");
	}
	if (handle == NULL &&
	    (err = snd_ctl_open(&handle, card, 0)) < 0) {
		error("Control %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	snd_ctl_elem_info_set_id(info, id);
	if ((err = snd_ctl_elem_info(handle, info)) < 0) {
		if (ignore_error)
			return 0;
		error("Cannot find the given element from control %s\n", card);
		if (! keep_handle) {
			snd_ctl_close(handle);
			handle = NULL;
		}
		return err;
	}
	snd_ctl_elem_info_get_id(info, id);	/* FIXME: Remove it when hctl find works ok !!! */
	type = snd_ctl_elem_info_get_type(info);
	count = snd_ctl_elem_info_get_count(info);
	snd_ctl_elem_value_set_id(control, id);
	
	if (!roflag) {
		ptr = argv[1];
		for (idx = 0; idx < count && idx < 128 && ptr && *ptr; idx++) {
			switch (type) {
			case SND_CTL_ELEM_TYPE_BOOLEAN:
				tmp = 0;
				if (!strncasecmp(ptr, "on", 2) || !strncasecmp(ptr, "up", 2)) {
					tmp = 1;
					ptr += 2;
				} else if (!strncasecmp(ptr, "yes", 3)) {
					tmp = 1;
					ptr += 3;
				} else if (!strncasecmp(ptr, "toggle", 6)) {
					tmp = snd_ctl_elem_value_get_boolean(control, idx);
					tmp = tmp > 0 ? 0 : 1;
					ptr += 6;
				} else if (isdigit(*ptr)) {
					tmp = atoi(ptr) > 0 ? 1 : 0;
					while (isdigit(*ptr))
						ptr++;
				} else {
					while (*ptr && *ptr != ',')
						ptr++;
				}
				snd_ctl_elem_value_set_boolean(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_INTEGER:
				tmp = get_integer(&ptr,
						  snd_ctl_elem_info_get_min(info),
						  snd_ctl_elem_info_get_max(info));
				snd_ctl_elem_value_set_integer(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_INTEGER64:
				tmp = get_integer64(&ptr,
						  snd_ctl_elem_info_get_min64(info),
						  snd_ctl_elem_info_get_max64(info));
				snd_ctl_elem_value_set_integer64(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED:
				tmp = get_ctl_enum_item_index(handle, info, &ptr);
				if (tmp < 0)
					tmp = get_integer(&ptr, 0, snd_ctl_elem_info_get_items(info) - 1);
				snd_ctl_elem_value_set_enumerated(control, idx, tmp);
				break;
			case SND_CTL_ELEM_TYPE_BYTES:
				tmp = get_integer(&ptr, 0, 255);
				snd_ctl_elem_value_set_byte(control, idx, tmp);
				break;
			default:
				break;
			}
			if (!strchr(argv[1], ','))
				ptr = argv[1];
			else if (*ptr == ',')
				ptr++;
		}
		if ((err = snd_ctl_elem_write(handle, control)) < 0) {
			if (!ignore_error)
				error("Control %s element write error: %s\n", card, snd_strerror(err));
			if (!keep_handle) {
				snd_ctl_close(handle);
				handle = NULL;
			}
			return ignore_error ? 0 : err;
		}
	}
	if (! keep_handle) {
		snd_ctl_close(handle);
		handle = NULL;
	}
	if (!quiet) {
		snd_hctl_t *hctl;
		snd_hctl_elem_t *elem;
		if ((err = snd_hctl_open(&hctl, card, 0)) < 0) {
			error("Control %s open error: %s\n", card, snd_strerror(err));
			return err;
		}
		if ((err = snd_hctl_load(hctl)) < 0) {
			error("Control %s load error: %s\n", card, snd_strerror(err));
			return err;
		}
		elem = snd_hctl_find_elem(hctl, id);
		if (elem)
			show_control("  ", elem, LEVEL_BASIC | LEVEL_ID);
		else
			printf("Could not find the specified element\n");
		snd_hctl_close(hctl);
	}
	return 0;
}

int alsa_set_mic_volume(int value)
{
	int ret;
	char *argv[3];
	if(value<0||value>100)
		return -1;
	volume_percent = value;
	argv[0]=malloc(256);
	argv[1]=malloc(256);
	argv[2]=malloc(256);
	if(!argv[0]||!argv[1]||!argv[2]){
		printf("error malloc buff for test set sound card\n");
		exit(0);
	}
	sprintf(argv[0],"numid=9,iface=MIXER,name=\'Mic PGA Capture Volume\'");
	sprintf(argv[1],"0");
	ret = cset(2, argv, 0, 0) ;
	if(ret<0){
		printf("**********************set mic volume error****************\n");
		free(argv[0]);
		free(argv[1]);
		free(argv[2]);
		volume_percent = -1;
		return ret;
	}
	free(argv[0]);free(argv[1]);free(argv[2]);
	volume_percent = -1;
	printf("----------------------set mic volume ok------------------\n");
	return 0;
}
int alsa_set_hp_volume(int value)
{
	int ret;
	//int i;
	char *argv[3];
	if(value<0||value>100)
		return -1;
	volume_percent = value;
	argv[0]=malloc(256);
	argv[1]=malloc(256);
	argv[2]=malloc(256);
	if(!argv[0]||!argv[1]||!argv[2]){
		printf("error malloc buff for test set sound card\n");
		exit(0);
	}
	sprintf(argv[0],"numid=3,iface=MIXER,name=\'HP Playback Volume\'");
	sprintf(argv[1],"0");
	ret = cset(2, argv, 0, 0) ;
	if(ret<0){
		printf("*******************set hp volume error****************\n");
		free(argv[0]);
		free(argv[1]);
		free(argv[2]);
		volume_percent = -1;
		return ret;
	}
	printf("----------------------set hp volume ok------------------\n");
	free(argv[0]);
	free(argv[1]);
	free(argv[2]);
	volume_percent = -1;
	return 0;
}
int alsa_set_volume(int value)
{
	int ret;
	char *argv[3];
	if(value<0||value>100)
		return -1;
	volume_percent = value;
	argv[0]=malloc(256);
	argv[1]=malloc(256);
	argv[2]=malloc(256);
	if(!argv[0]||!argv[1]||!argv[2]){
		printf("error malloc buff for test set sound card\n");
		exit(0);
	}
	sprintf(argv[0],"numid=9,iface=MIXER,name=\'Mic PGA Capture Volume\'");
	sprintf(argv[1],"0");
	argv[0][strlen(argv[0])]='\0';
	argv[1][strlen(argv[1])]='\0';
	ret = cset(2, argv, 0, 0) ;
	if(ret<0){
		printf("**********************cset1 error****************\n");
		free(argv[0]);
		free(argv[1]);
		free(argv[2]);
		volume_percent = -1;
		return ret;
	}
	usleep(1000);
	memset(argv[0],0,256);
	memset(argv[1],0,256);
	sprintf(argv[0],"numid=3,iface=MIXER,name=\'HP Playback Volume\'");
	sprintf(argv[1],"0");
	argv[0][strlen(argv[0])]='\0';
	argv[1][strlen(argv[1])]='\0';
	ret = cset(2, argv, 0, 0) ;
	if(ret<0){
		printf("**********************cset2 error****************\n");
		free(argv[0]);
		free(argv[1]);
		free(argv[2]);
		volume_percent = -1;
		return ret;
	}
	printf("-----------cset sucess---------------------------\n");
	
	free(argv[0]);
	free(argv[1]);
	free(argv[2]);
	volume_percent = -1;
	return 0;
}

int test_set_sound_card()
{
	alsa_set_mic_volume(70);
	alsa_set_hp_volume(91);
	return 0;
}

