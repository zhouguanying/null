#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <linux/fs.h>

#include "vpu_server.h"
#include "cli.h"

#define FNT_DBG
#ifdef FNT_DBG
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt, __func__, ## args); \
    } while (0)
#else
#define dbg(fmt, args...)	do {} while (0)
#endif



int dev_mem_fd;

#define MULTI_PROCESS

/**
 * config_params - configuration parameters
 * @field_name: param name in file
 * @field_len: length of field descriptor
 * @field_val: value associated with field
 * @field_str: string value associated with field
 * @change_pending: Indicates param has changed
 */
struct config_params  {
    char field_name[256];
    int field_len;
    int field_val;
    char *field_str;
    int change_pending;
};

#if 0
int IOSystemInit(void *callback)
{
	return 0;
}
int IOSystemShutdown(void)
{
	return 0;
}

int IOGetPhyMem(vpu_mem_desc * buff)
{
	return 0;
}
int IOFreePhyMem(vpu_mem_desc * buff)
{
	return 0;
}
int IOGetVirtMem(vpu_mem_desc * buff)
	
{
	return 0;
}
int IOFreeVirtMem(vpu_mem_desc * buff)
	
{
	return 0;
}
#endif

/**
 * video_config_params - video configuration parameters
 * @framerate: video frame rate
 * @bitrate: bitrate used by compressor
 * @resolution: capture device resolution 
 * @gop: group of pictures format
 * @rotation_angle: output image rotation angle 
 * @output_ration:
 * @mirror_angle: mirror angle
 * @name: camera name
 */
struct video_config_params {
    struct config_params framerate;
    struct config_params bitrate;
    struct config_params compression;
    struct config_params resolution;
    struct config_params gop;
    struct config_params rotation_angle;
    struct config_params output_ratio;
    struct config_params mirror_angle;
    struct config_params name;
};

#if 0
//client.........
int vs_server_init_instance(vs_instance* instance, int width, int height, int bitrate, int framerate)
//vs_instance* vs_client_get_instance(int width, int height, int bitrate, int framerate)
{
	RetCode ret;
	int i;
	EncHeaderParam header;
	SearchRamParam search;
	EncInitialInfo initinfo = {0};
	FrameBuffer *fb;

	instance->owner = 0;
	memset(&instance->bit_stream_buf, 0, sizeof(vpu_mem_desc));
	instance->bit_stream_buf.size = VS_STREAM_BUF_SIZE;
	IOGetPhyMem(&instance->bit_stream_buf);
	IOGetVirtMem(&instance->bit_stream_buf);

	memset(&(instance->open_param), 0, sizeof(instance->open_param));

	instance->open_param.bitstreamBuffer = instance->bit_stream_buf.phy_addr;
	instance->open_param.bitstreamBufferSize = VS_STREAM_BUF_SIZE;
	instance->open_param.bitstreamFormat = STD_AVC;

	instance->open_param.bitRate = bitrate;
//	instance->open_param.frameRateInfo = framerate;
	instance->open_param.frameRateInfo = 25;

	/* framebufWidth and framebufHeight must be a multiple of 16 */
    	 instance->open_param.picWidth = ((width + 15) & ~15);  	
	instance->open_param.picHeight = ((height + 15) & ~15);

     ret = vpu_EncOpen(&(instance->handle), &(instance->open_param));
	if (ret != RETCODE_SUCCESS) {
             printf("%s: error opening VPU:error code:%d\n", __func__, ret);
             goto error;
     }

	search.searchRamAddr = 0xFFFF4C00;
#if 0
	if (cpu_is_mx27()) {
		
	}else{
		iram_t iram;
		int ram_size;
		memset(&iram, 0, sizeof(iram_t));
		ram_size = ((width + 15) & ~15) * 36 + 2048;
		IOGetIramBase(&iram);
		if ((iram.end - iram.start) < ram_size) {
			printf("vpu iram is less than needed.\n");
			return -1;
		} else {
			/* Allocate max iram for vpu encoder search ram*/
			ram_size = iram.end - iram.start;
			search.searchRamAddr = iram.start;
			search.SearchRamSize = ram_size;
		}
	}
#endif
	ret = vpu_EncGiveCommand(
						instance->handle,
						ENC_SET_SEARCHRAM_PARAM, 
                        		&search);
	if (ret != RETCODE_SUCCESS) {
		printf("%s: error searching VPU params\n", __func__);
		goto error;
	}

	ret = vpu_EncGetInitialInfo(instance->handle, &initinfo);
	if (ret != RETCODE_SUCCESS) {
		printf("Encoder GetInitialInfo failed\n");
		return -1;
	}
	printf("%s: minFrameBufferCount = %d\n", __func__, initinfo.minFrameBufferCount);

	for(i = 0; i < VS_FRAME_BUFFER_NUM; i++){
		instance->frame[i].index = i;
		instance->frame[i].state = VS_FRAME_BUFFER_STATE_IDLE;
		instance->frame[i].instance = instance->id;
		instance->frame[i].desc.size = (width * height * 3 / 2);
		instance->frame[i].force_iframe = 0;
		IOGetPhyMem(&(instance->frame[i].desc));
		if (instance->frame[i].desc.phy_addr == 0) {
			int j;
			for (j = 0; j < i; j++) {
				IOFreeVirtMem(&(instance->frame[i].desc));
				IOFreePhyMem(&(instance->frame[i].desc));
			}
			printf("%s: error framebuffer!\n", __func__);
			goto error;
		}
		IOGetVirtMem(&(instance->frame[i].desc));
	}

	fb = (FrameBuffer*)malloc(sizeof(FrameBuffer)*VS_FRAME_BUFFER_NUM);
	for (i = 0; i < VS_FRAME_BUFFER_NUM; ++i) {
		fb[i].bufY =  instance->frame[i].phy.bufY =
			instance->frame[i].desc.phy_addr;
		fb[i].bufCb = instance->frame[i].phy.bufCb =
			instance->frame[i].phy.bufY + width* height;
		fb[i].bufCr = instance->frame[i].phy.bufCr =
			instance->frame[i].phy.bufCb + width * height / 4;
	}
	
	ret = vpu_EncRegisterFrameBuffer(
						instance->handle,
						fb,
						2,
						width);
	if (ret != RETCODE_SUCCESS) {
		printf("%s: error vpu_EncRegisterFrameBuffer\n", __func__);
		goto error;
	}

	/*encode header*/
	header.headerType = SPS_RBSP;
	vpu_EncGiveCommand(
					instance->handle, 
					ENC_PUT_AVC_HEADER, 
					&header);
	memcpy(
		instance->header.buf,
		(char *)(header.buf - instance->bit_stream_buf.phy_addr + instance->bit_stream_buf.virt_uaddr),
		header.size);
	instance->header.size = header.size;

	header.headerType = PPS_RBSP;
	vpu_EncGiveCommand(
					instance->handle, 
					ENC_PUT_AVC_HEADER, 
					&header);
	memcpy(
		&instance->header.buf[instance->header.size],
		(char *)(header.buf - instance->bit_stream_buf.phy_addr + instance->bit_stream_buf.virt_uaddr),
		header.size);
	instance->header.size += header.size;

	free(fb);
	return 0;
	
error:
	IOFreeVirtMem(&instance->bit_stream_buf);
	IOFreePhyMem(&instance->bit_stream_buf);
	return 0;
}

vs_context* vs_server_create_context(vs_record_parameter* record, vs_record_parameter* monitor)
{
	int ret;
	vpu_versioninfo ver;
	vs_context* context;

	context = malloc(sizeof(vs_context));
	if(context == 0){
		printf("%s: malloc error!\n", __func__);
		return 0;
	}

	context->sem = sem_open(VS_SEMAPHARE_NAME, O_CREAT | O_RDWR);
	if(context->sem == SEM_FAILED){
		printf("%s: sem_open fail\n", __func__);
		ret = -1;
		goto error0;
	}

	context->shm_handle = 
				shmget(
					VS_SHARE_MEMORY_KEY,
					sizeof(vs_instance)*2,
					IPC_CREAT |S_IRUSR|
					S_IWUSR|S_IROTH|S_IWOTH);

	if(context->shm_handle < 0){
		printf("%s: create shared memory error!\n", __func__);
		goto error0;
	}
	context->shm_addr = context->instance = (vs_instance*)shmat(context->shm_handle, NULL, 0);

	

	//ret = vpu_Init(NULL);
	ret = IOSystemInit(NULL);
	if(ret < 0){
		printf("%s: IOSystemInit fail\n", __func__);
		goto error1;
	}

	ret = vpu_GetVersionInfo(&ver);
	if (ret) {
		printf("Cannot get version info\n");
		IOSystemShutdown();
		goto error1;
	}

	printf("VPU firmware version: %d.%d.%d\n", ver.fw_major, ver.fw_minor,
						ver.fw_release);
	printf("VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor,
						ver.lib_release);

	//todo.
	context->instance[0].id = 0;
	context->instance[1].id = 1;
	vs_server_init_instance(
			&context->instance[0],
			record->width,
			record->height,
			record->bitrate,
			record->framerate);
//	if( record->width > monitor->width && record->height > monitor->height )
		vs_server_init_instance(
				&context->instance[1],
				monitor->width,
				monitor->height,
				monitor->bitrate,
				monitor->framerate);

	return context;

error1:
#ifdef MULTI_PROCESS
	sem_close(context->sem);
#endif
error0:
	return 0;
}

static int policy = 0;
int vs_server_get_filled_frame(vs_context* context, vs_frame** frame)
{
	int i, j;
	vs_instance* instance;

	//sem_wait(context->sem);

	for(i = 0; i < VS_INSTANCE_NUM; i++){
		instance = &context->instance[i];
		#if 0
		for(j = 2; j < VS_FRAME_BUFFER_NUM; j++){
			//printf("%s: frame %d state = %d\n", __func__, i, instance->frame[j].state);
			if(instance->frame[j].state == VS_FRAME_BUFFER_STATE_FILLED){
				instance->frame[j].state = VS_FRAME_BUFFER_STATE_WORKING;
				*frame = &(instance->frame[j]);
				return 0;
			}
		}
		#endif
		#if 1
		if(policy == 0){
			for(j = 2; j < VS_FRAME_BUFFER_NUM; j++){
				//printf("%s: frame %d state = %d\n", __func__, i, instance->frame[j].state);
				if(instance->frame[j].state == VS_FRAME_BUFFER_STATE_FILLED){
					instance->frame[j].state = VS_FRAME_BUFFER_STATE_WORKING;
					*frame = &(instance->frame[j]);
					return 0;
				}
			}
			policy = 1;
		}else{
			for(j = VS_FRAME_BUFFER_NUM - 1; j >= 2; j--){
				//printf("%s: frame %d state = %d\n", __func__, i, instance->frame[j].state);
				if(instance->frame[j].state == VS_FRAME_BUFFER_STATE_FILLED){
					instance->frame[j].state = VS_FRAME_BUFFER_STATE_WORKING;
					*frame = &(instance->frame[j]);
					return 0;
				}
			}
			policy = 0;
		}
		#endif
	}

	//sem_post(context->sem);

	return -1;
}

int vs_server_encode_frame(vs_context* context, vs_frame* frame)
{
	vs_instance* instance = &context->instance[frame->instance];
	EncParam enc_param;
	
	enc_param.sourceFrame = &(frame->phy);
	enc_param.quantParam = 30;
	enc_param.forceIPicture = frame->force_iframe;
	enc_param.skipPicture = 0;

	//printf("%s: vpu encode begin\n", __func__);
      /* Start encoding data */
      if (vpu_EncStartOneFrame(instance->handle, &enc_param) 
                      != RETCODE_SUCCESS) {
              printf("%s: error encode on raw data - exitting\n", __func__);
              return -1;
      }

	/*wait here*/
	while (vpu_IsBusy()) {
		vpu_WaitForInt(2000);
		//printf("%s: vpu busy\n", __func__);
	}

	//printf("%s: vpu encode finished\n", __func__);
#if STREAM_ENC_PIC_RESET == 1
	if (vpu_EncGetOutputInfo(instance->handle, &frame->output_info) 
	    != RETCODE_SUCCESS) {
		printf("%s: error encoded data params - exitting\n",
		       __func__);
		return -1;
	}

	#if 0
	{
		unsigned char* buf;
		int size;
		buf = frame->output_info.bitstreamBuffer -
					instance->bit_stream_buf.phy_addr +
					instance->bit_stream_buf.virt_uaddr;
		size = frame->output_info.bitstreamSize;
		memcpy(frame->bitstream, buf, size);
	}
	#endif
#else
	{
		RetCode ret;
		
		sem_wait(context->sem);
		ret = vpu_EncGetBitstreamBuffer(instance->handle, &instance->ring.rd, &instance->ring.wr,
						&instance->ring.size);
		if (ret != RETCODE_SUCCESS) {
			printf("EncGetBitstreamBuffer failed\n");
			sem_post(context->sem);
			return -1;
		}
		sem_post(context->sem);
	}
#endif

	//sem_wait(context->sem);
	frame->state = VS_FRAME_BUFFER_STATE_ENCODED;
	//sem_post(context->sem);
	
	return 0;
}

int vs_server_destroy_context(vs_context* context)
{
	int ret;
	vpu_EncClose(context->instance[0].handle);
	vpu_EncClose(context->instance[1].handle);
	ret = shmdt((void*)context->shm_addr);
	ret = shmctl(context->shm_handle, IPC_RMID, 0);
	sem_close(context->sem);
	free(context);
	return 0;
}

int vs_client_destroy_context(vs_context* context)
{
	int ret;

	context->instance->owner--;
	ret = shmdt((void*)context->shm_addr);
	sem_close(context->sem);
	free(context);
	close(dev_mem_fd);
	return 0;
}


vs_context* vs_client_create_context(int instance_id)
{
	int i, j;
	vs_instance* instance;
	vs_context* context;

	context = malloc(sizeof(vs_context));
	if(context == 0){
		printf("%s: malloc error!\n", __func__);
		return 0;
	}
	
	context->sem = sem_open(VS_SEMAPHARE_NAME, O_RDWR|O_CREAT);
	if(context->sem == SEM_FAILED){
		printf("%s: sem_open fail\n", __func__);
		goto error0;
	}

	context->shm_handle = 
				shmget(
					VS_SHARE_MEMORY_KEY,
					sizeof(vs_instance)*2,
					IPC_EXCL|S_IRUSR|S_IWUSR|
					S_IROTH|S_IWOTH);
	if(context->shm_handle < 0){
		printf("%s: create shared memory error!\n", __func__);
		goto error0;
	}
	context->shm_addr = shmat(context->shm_handle, NULL, 0);
	instance = context->instance = &((vs_instance*)context->shm_addr)[instance_id];
//	printf("%s: instance=0x%x\n", __func__, context->instance);

	dev_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(dev_mem_fd < 0){
		printf("%s: open dev_mem error!\n", __func__);
		goto error0;
	}

	context->bitstream_buffer_virt = 
			(unsigned long)mmap(
						NULL,
						instance->bit_stream_buf.size,
						PROT_READ | PROT_WRITE,
						MAP_SHARED,
						dev_mem_fd,
						instance->bit_stream_buf.phy_addr);
//	printf("%s: instance=0x%x, bit stream virt=0x%x\n", __func__, instance, instance->bitstream_buffer_virt);

	if(instance->owner == 0){
		for(i = 0; i < VS_FRAME_BUFFER_NUM; i++){
			instance->frame[i].virt_client =
						(unsigned long)mmap(
									NULL,
									instance->frame[i].desc.size,
									PROT_READ | PROT_WRITE,
									MAP_SHARED,
									dev_mem_fd,
									instance->frame[i].desc.phy_addr);
			printf("%s: frame virt=0x%x, instance=0x%x\n", 
				__func__, instance->frame[i].virt_client, instance->frame[i].instance);
		}
	}

	instance->owner++;
	
	return context;
	
error0:
	return 0;
}

int vs_client_get_header(vs_context* context, unsigned char** buf, int *size)
{
	vs_instance* instance = context->instance;
	*buf = instance->header.buf;
	*size = instance->header.size;

	return 0;
}

vs_frame* vs_client_get_idle_frame(vs_context* context)
{
	int i;
	vs_frame* frame = NULL;
	vs_instance* instance = context->instance;
	
	sem_wait(context->sem);

	for(i = 2; i < VS_FRAME_BUFFER_NUM; i++){
	//for(i = VS_FRAME_BUFFER_NUM - 1; i >= 0; i--){
		//printf("%s: state = %d\n", __func__, instance->frame[i].state);
		if(instance->frame[i].state == VS_FRAME_BUFFER_STATE_IDLE){
			instance->frame[i].state = VS_FRAME_BUFFER_STATE_WORKING;
			frame = &(instance->frame[i]);
			break;
		}
	}

	sem_post(context->sem);

	return frame;
}

int vs_client_queue_frame(vs_context* context, vs_frame* frame)
{
#ifdef MULTI_PROCESS
	sem_wait(context->sem);
#endif
	frame->state = VS_FRAME_BUFFER_STATE_FILLED;

#ifdef MULTI_PROCESS
	sem_post(context->sem);
#endif
	return 0;
}

int vs_client_get_output_info(vs_context* context, vs_frame* frame, unsigned char** buf, int *size)
{
	int ret;
	vs_instance* instance = context->instance;

#ifdef MULTI_PROCESS
	sem_wait(context->sem);
#endif
	
	if(frame->state != VS_FRAME_BUFFER_STATE_ENCODED){
		ret = -1;
	}else{
		*buf = frame->output_info.bitstreamBuffer -
					instance->bit_stream_buf.phy_addr +
					context->bitstream_buffer_virt;
		//*buf = frame->bitstream;
		*size = frame->output_info.bitstreamSize;
		ret = 0;

		/*printf("%s: instance=0x%x, virt=0x%x, phy=0x%x, start=0x%x\n", 
				__func__,
				instance,
				instance->bitstream_buffer_virt,
				instance->bit_stream_buf.phy_addr,
				frame->output_info.bitstreamBuffer);*/
	}

#ifdef MULTI_PROCESS
		sem_post(context->sem);
#endif

	return ret;
}

int vs_client_consum_encoded_data(vs_context* context, vs_frame* frame, int size)
{
#ifdef MULTI_PROCESS
	sem_wait(context->sem);
#endif

	frame->state = VS_FRAME_BUFFER_STATE_IDLE;

#ifdef MULTI_PROCESS
		sem_post(context->sem);
#endif
	return 0;
}
#endif

/*
*************************** get the config file**********************************
*/

static struct video_config_params video_template = {
        {"framerate=",      10, 0, 0},
        {"bitrate=",        8,  0, 0},
        {"compression=",    12, 0, 0},
        {"resolution=",     11, 0, 0},
        {"gop=",            4,  0, 0},
        {"rotation_angle=", 15, 0, 0},
        {"output_ratio=",   13, 0, 0},
        {"mirror_angle=",   13, 0, 0},
        {"name=",           5, 0, 0}
};

/* Local helper methods */

static char * get_line(char *s, int size, FILE *stream, int *line,
                       char **_pos)
{
	char *pos, *end = NULL;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		/* Skip white space from the beginning of line. */
		while (*pos == ' ' || *pos == '\t' || *pos == '\r')
			pos++;

		/* Skip comment lines and empty lines */
		if (*pos == ';' || *pos == '\n' || *pos == '\0')
			continue;

		/* Remove trailing white space. */
		while (end > pos &&
		       (*end == '\n' || *end == ' ' || *end == '\t' ||
			*end == '\r'))
			*end-- = '\0';

		if (*pos == '\0')
			continue;

		if (_pos)
			*_pos = pos;
		return pos;
	}

	if (_pos)
		*_pos = NULL;
	return NULL;
}

static int populate_video(struct video_config_params *c, char *filename)
{
        FILE *fp;
        char buf[256];
        char *pos;
	int line = 0;

        if ((fp = fopen(filename, "r")) == NULL) {
                printf("%s input error\n", __func__);
                return -1;
        }

        /* Parse file and populate configuration params */
        while (get_line(buf, sizeof(buf), fp, &line, &pos)) {
                if (strncmp(pos, c->framerate.field_name, 
                                        c->framerate.field_len) == 0) {
                        c->framerate.field_val = atoi(pos + 
                                        c->framerate.field_len); 
 //                       dbg("%s=%d", c->framerate.field_name, 
                           //             c->framerate.field_val);
                } else if (strncmp(pos, c->bitrate.field_name, 
                                        c->bitrate.field_len)
                        == 0) {
                        c->bitrate.field_val = atoi(pos + 
                                        c->bitrate.field_len); 
                        //dbg("%s=%d", c->bitrate.field_name, 
                              //          c->bitrate.field_val);
                } else if (strncmp(pos, c->compression.field_name,
                           c->compression.field_len) == 0) {
                        free(c->compression.field_str);
                        c->compression.field_str =
                        strndup(pos + c->compression.field_len, 5); 
                        //dbg("%s=%s", c->compression.field_name, 
                               //         c->compression.field_str);
                } else if (strncmp(pos, c->resolution.field_name,
                           c->resolution.field_len) == 0) {
                        free(c->resolution.field_str);
                        c->resolution.field_str = strndup(pos + 
                                        c->resolution.field_len, 7); 
                       // dbg("%s=%s", c->resolution.field_name, 
                               //         c->resolution.field_str);
                } else if (strncmp(pos, c->gop.field_name, 
                                        c->gop.field_len) == 0) {
                        c->gop.field_val = atoi(pos + c->gop.field_len); 
                       // dbg("%s=%d", c->gop.field_name, c->gop.field_val);
                } else if (strncmp(pos, c->rotation_angle.field_name,
                           c->rotation_angle.field_len) == 0) {
                        c->rotation_angle.field_val =
                                atoi(pos + c->rotation_angle.field_len); 
                       // dbg("%s=%d", c->rotation_angle.field_name,
                    //    c->rotation_angle.field_val);
                } else if (strncmp(pos, c->output_ratio.field_name,
                           c->output_ratio.field_len) == 0) {
                        c->output_ratio.field_val = atoi(pos + 
                                        c->output_ratio.field_len); 
                       // dbg("%s=%d", c->output_ratio.field_name, 
                        //                c->output_ratio.field_val);
                } else if (strncmp(pos, c->mirror_angle.field_name,
                           c->mirror_angle.field_len) == 0) {
                        c->mirror_angle.field_val = atoi(pos + 
                                        c->mirror_angle.field_len); 
                        //dbg("%s=%d", c->mirror_angle.field_name, 
                             //           c->mirror_angle.field_val);
                } else if (strncmp(pos, c->name.field_name,
                           c->name.field_len) == 0) {
                        free(c->name.field_str);    
                        c->name.field_str = strndup(pos + 
                                        c->name.field_len, 20); 
                        //dbg("%s=%s", c->name.field_name, 
                                 //       c->name.field_str);
                } else ;
        }
        
        fclose(fp);
        
        return 0;
}

/**
 * parse_video_conf - parse video configuration file
 * @filename: configuration file
 * Returns video config context on success or < 0 on error
 */
static struct video_config_params * parse_video_conf(char *filename)
{
        struct video_config_params *c;

        if (filename == NULL || (c = malloc(sizeof(*c))) == NULL) {
                printf("%s input e rror\n", __func__);     
                return NULL;
        }
        memset(c, 0, sizeof(*c));

        /* Populate config structure */ 
        memcpy(c, (char *) &video_template, sizeof(*c));
        if (populate_video(c, filename) < 0) {
                free(c);
                return NULL;
        } else
                return c;
}

static int get_video_resolution(struct video_config_params *p,int* width, int* height)
{
	char* str = NULL;
        /* Screen resolution */
        if (strncmp(p->resolution.field_str, "d1_ntsc", 7) == 0) {
                *width = 720;
                *height = 486;
                str = "D1 NTSC (720 x 486)";
        } else if (strncmp(p->resolution.field_str, "d1_pal", 6) == 0) {
                *width = 720;
                *height = 576;
                str = "D1 PAL (720 x 576)";
        } else if (strncmp(p->resolution.field_str, "qvga", 4) == 0) {
                *width = 320;
                *height = 240;
                str = "QVGA (320 x 240)";
        } else if (strncmp(p->resolution.field_str, "cif", 3) == 0) {
                *width = 352;
                *height = 288;
                str = "CIF (352 x 288)";
        } else if (strncmp(p->resolution.field_str, "qcif", 4) == 0) {
                *width = 176;
                *height = 144;
                str = "QCIF (176 x 144)";
        } else if (strncmp(p->resolution.field_str, "sqcif", 5) == 0) {
                *width = 128;
                *height = 96;        
                str = "SQCIF (128 x 96)";
        } else if (strncmp(p->resolution.field_str, "4cif", 4) == 0) {
                *width = 704;
                *height = 576;        
                str = "4CIF (704 x 576)";        
        } else if (strncmp(p->resolution.field_str, 
                                "svga", 4) == 0) {
                *width = 800;
                *height = 600;
                str = "SVGA (800 x 600)";
        } else {
                /* Defaults to VGA */
                *width = 640;
                *height = 480;
                str = "VGA (640 x 480)";
        }
//	dbg("resolution:%s\n", str);
}

static int get_video_bitrate(struct video_config_params *p,int* bitrate)
{
	*bitrate = p->bitrate.field_val;
}

static int get_video_framerate(struct video_config_params *p,int* framerate)
{
	*framerate = p->framerate.field_val;
}

static vs_record_parameter* record_para = NULL;
static vs_record_parameter* monitor_para = NULL;
vs_record_parameter* vs_get_record_para(void)
{
	struct video_config_params* video_config;
	if( record_para == NULL ){
		video_config = parse_video_conf(RECORD_PAR_FILE);
		if( NULL == video_config ){
			return NULL;
		} 
		record_para = malloc( sizeof(vs_record_parameter) );
		if( NULL == record_para )
			return NULL;
		get_video_resolution(video_config, &record_para->width, &record_para->height);
		get_video_bitrate(video_config, &record_para->bitrate);
		get_video_framerate(video_config, &record_para->framerate);
		return record_para;
	}
	return record_para;
}

vs_record_parameter* vs_get_monitor_para(void)
{
	struct video_config_params* video_config;
	if( monitor_para == NULL ){
		video_config = parse_video_conf(MONITOR_PAR_FILE);
		if( NULL == video_config ){
			return NULL;
		} 
		monitor_para = malloc( sizeof(vs_record_parameter) );
		if( NULL == monitor_para )
			return NULL;
		get_video_resolution(video_config, &monitor_para->width, &monitor_para->height);
		get_video_bitrate(video_config, &monitor_para->bitrate);
		get_video_framerate(video_config, &monitor_para->framerate);
		return monitor_para;
	}
	return monitor_para;
}

int vs_get_record_config()
{
	int fd;
	char* buffer;
	fd = open("/dev/nand-data", O_RDONLY);
	buffer = malloc( 10000 );
	if( NULL == buffer ){
		printf("malloc buffer error\n");
		close(fd);
		return -1;
	}
	memset(buffer, 0, 10000);
	if( read(fd, buffer, 10000) != 10000 ){
		printf("read error\n");
		close(fd);
		return -1;
	}
	close(fd);
//	dbg("get record mode: %d\n",buffer[4096]);
	return (int)buffer[4096];
}

int vs_set_record_config(int mode)
{
	int fd;
	char* buffer;

	printf("%s\n", __func__);
	fd = open("/dev/nand-data", O_RDWR|O_SYNC);
	if( fd < 0 ){
		perror("open nand-data");
		return -1;
	}
	buffer = malloc( 512*1024 );
	memset(buffer, 0, 512*1024);
	if( read(fd, buffer, 512*1024) != 512*1024 ){
		printf("read error\n");
		return -1;
	}
	buffer[4096] = (char)mode;
//	dbg("write data file\n");
	close(fd);
	fd = open("/dev/nand-data", O_RDWR|O_SYNC);
	if( fd < 0 ){
		perror("open nand-data");
		return -1;
	}
//	dbg("write file\n");
	write( fd, buffer, 512*1024 );
//	ioctl(fd, BLKFLSBUF, NULL );
	close(fd);
	system("sync");	
	system("/nand-flush /dev/nand-data");
	system("sync");
	return 0;
}


