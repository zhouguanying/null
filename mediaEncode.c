#include "mediaEncode.h"
#include <akmedialib/media_recorder_lib.h>
#include "debug.h"
#include "log_dbg.h"
#include "data_chunk.h"
#include <pthread.h>
#include "video_stream_lib.h"
#include "server.h"

#ifdef DEBUG
#define printd printf
#else
void printd(const char *format, ...)
{
}
#endif
FILE* videofile;
FILE* audiofile;
FILE* headfile;
FILE* g_MovieRecHandle;

T_MEDIALIB_STRUCT mRecLibHandle;
//T_VOID record_media(char* filaname);
T_MEDIALIB_REC_OPEN_INPUT rec_open_input;
T_MEDIALIB_REC_OPEN_OUTPUT rec_open_output;

#if 0
static data_chunk_t *_encode_buf = NULL;
static pthread_mutex_t _encode_buf_lock;
void encode_buffer_lock()
{
    pthread_mutex_lock(&_encode_buf_lock);
}

void encode_buffer_unlock()
{
    pthread_mutex_unlock(&_encode_buf_lock);
}
//static long _l_seek = 0;

void clear_encode_video_buffer(void)
{
    encode_buffer_lock();
    data_chunk_clear(_encode_buf);
    encode_buffer_unlock();
}

int get_encode_video_buffer(unsigned char *buffer, int size)
{
    int ret = 0;
    encode_buffer_lock();
    if (data_chunk_size(_encode_buf) >= size)
    {
        data_chunk_popfront(_encode_buf, buffer, size);
        ret = 1;
    }
    encode_buffer_unlock();

    return ret;
}

int write_encode_video_buffer(unsigned char *buffer, int size)
{
	int ret = 0;

    encode_buffer_lock();
	if (data_chunk_freespace(_encode_buf) >= size)
	{
		data_chunk_pushback(_encode_buf, buffer, size);
	}
	else{
		ret = -1;
	}
	encode_buffer_unlock();

    return ret;
}

int get_encode_video_buffer_valid_size(void)
{
	return data_chunk_size(_encode_buf);
}

#endif

#define is_power_of_2(x)	((x) != 0 && (((x) & ((x) - 1)) == 0))


/* for caculate the time */
long long get_system_time_ms(void)
{
	struct timeval now;
	long long ret;
	static struct timeval last = {0,0};
	static struct timeval base = {0,0};
	gettimeofday(&now, NULL);
	if( abs( now.tv_sec - last.tv_sec ) > 1 ){
		base.tv_sec += now.tv_sec - last.tv_sec;
		base.tv_usec += now.tv_usec - last.tv_usec;
	}
	last = now;
	ret = ((long long)now.tv_sec - (long long)base.tv_sec) * 1000 + ((long long)now.tv_usec - (long long)base.tv_usec) / 1000;
	return ret;
}

T_VOID ak_rec_cb_printf(T_pCSTR format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

T_VOID ak_sd_cb_printf(T_pCSTR format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

T_pVOID ak_rec_cb_malloc(T_U32 size)
{	
    debug("ak_rec_cb_malloc in\n");
	return malloc(size);
}

T_VOID ak_rec_cb_free(T_pVOID mem)
{	
    debug("ak_rec_cb_free in\n");
	return free(mem);
}

T_pVOID ak_rec_cb_memcpy(T_pVOID dest, T_pCVOID src, T_U32 size)
{
	debug("ak_rec_cb_memcpy in, size %d\n", size);
	return memcpy(dest,src,size);
}

T_S32 ak_rec_cb_fread(T_S32 hFileWriter, T_pVOID buf, T_S32 size)
{
	debug("ak_rec_cb_fread in size %d\n", size);
	return fread(buf, 1, size, headfile);
}

static int encode_temp_buf_size = 5;
static char* encode_temp_buffer = NULL;

void clear_encode_temp_buffer()
{
	encode_temp_buf_size = 5;
	if( encode_temp_buffer == NULL ){
		  encode_temp_buffer = akuio_alloc_pmem( 512*1024);
		  memset(encode_temp_buffer, 0, ( 512*1024));
		  encode_temp_buffer[0] = encode_temp_buffer[1] = encode_temp_buffer[2] = 0;
		  encode_temp_buffer[3] = 1; encode_temp_buffer[4] = 0xc;
	}
}

int get_temp_buffer_data(char** buffer, int* size)
{
	if( encode_temp_buffer == NULL )
		return -1;
	*buffer = encode_temp_buffer;
	*size = encode_temp_buf_size;
	return 0;
}

T_S32 ak_rec_cb_fwrite(T_S32 hFileWriter, T_pVOID buf, T_S32 size)
{
//  int ret = size;
  if( encode_temp_buffer == NULL ){
		encode_temp_buffer = akuio_alloc_pmem( 512*1024);
		memset( encode_temp_buffer, 0, ( 512*1024));
		encode_temp_buffer[0] = encode_temp_buffer[1] = encode_temp_buffer[2] = 0;
		encode_temp_buffer[3] = 1; encode_temp_buffer[4] = 0xc;
  }
  
  if(!strncmp("00dc",buf,4))
  {
    //ret=fwrite(buf+8,1, size,videofile);
#if 0
    encode_buffer_lock();
    unsigned char *p = (unsigned char *)buf;
    //data_chunk_pushback(_encode_buf, buf + 8, size - 8);
    p[3] = 0;
    p[4] = 0;
    p[5] = 0;
    p[6] = 1;
    p[7] = 0x0c;
    data_chunk_pushback(_encode_buf, buf + 8 - 5, size + 5 - 8);
    //printf("pushback %ld size buffer size %ld\n", size, _encode_buf->data_size);
	//printf(" encode type: %x, %x, %x, %x, %x, %x, %x\n",p[8],p[9],p[10],p[11],p[12],p[13],p[14] );
    //printf("video data size %d, encode buffer size %ld\n", size, data_chunk_size(_encode_buf));
    encode_buffer_unlock();
#else
//	encode_buffer_lock();
	memcpy( encode_temp_buffer+encode_temp_buf_size, buf, size );
	encode_temp_buf_size += size;
//	encode_buffer_unlock();
#endif
  }
  else if(!strncmp("00wb",buf,4))
  {
      //printf("audio data size %d\n", size);
    //ret=fwrite(buf+8,1, size,audiofile);
  }
  else
  {
    //printf("other data size %d\n", size);
    size = fwrite(buf,1, size,headfile);
  }
    
	return size;
}

T_S32 ak_rec_cb_fseek(T_S32 hFileWriter, T_S32 offset, T_S32 whence)
{
	debug("ak_rec_cb_fseek in\n");
	
	if (fseek ((FILE *)headfile, offset, whence) < 0) {
		return -1;
    }

  return ftell ((FILE *)headfile);
}

T_S32 ak_rec_cb_ftell(T_S32 hFileWriter)
{
	debug("ak_rec_cb_ftell in\n");

	return ftell(headfile);
}

T_BOOL ak_rec_cb_lnx_filesys_isbusy(void)
{
	return AK_FALSE;
}

T_S32 ak_rec_cb_lnx_fhandle_exist(T_pVOID hFileWriter)
{
	return 1;
}
 
T_BOOL ak_rec_cb_lnx_delay(T_U32 ticks)
{
	return (usleep (ticks*1000) == 0);
}

int openFile()
{
	headfile = fopen("/tmp/head.bin","wb");
	videofile = fopen("/tmp/video.bin","wb");
	audiofile = fopen("/tmp/audio.bin","wb");
	return 0;
}

int closeFile()
{
	if( headfile ){
		fclose(headfile);
		headfile = NULL;
	}
	if( videofile ){
		fclose(videofile);
		videofile = NULL;
	}
	if( audiofile ){
		fclose(audiofile);
		audiofile = NULL;
	}
	return 0;
}

// initial callback function pointer
void init_cb_func_init(T_MEDIALIB_INIT_CB* pInit_cb)
{
  	pInit_cb->m_FunPrintf          = (MEDIALIB_CALLBACK_FUN_PRINTF)printf;
  	pInit_cb->m_FunLoadResource    = NULL;
  	pInit_cb->m_FunReleaseResource = NULL;
}

static T_pVOID g_hVS;
static T_VIDEOLIB_ENC_OPEN_INPUT open_input;
static T_VIDEOLIB_ENC_IO_PAR video_enc_io_param;

int closeMedia()
{
	VideoStream_Enc_Close(g_hVS);
	MediaLib_Destroy();
	return 0;
}

int openMedia(T_U32 nvbps, int width, int height)
{
#if !ENCODE_USING_MEDIA_LIB
  	T_MEDIALIB_INIT_CB init_cb;
  	T_MEDIALIB_INIT_INPUT init_input;

	init_cb_func_init(&init_cb);
	init_input.m_ChipType = MEDIALIB_CHIP_AK9801;
	init_input.m_AudioI2S = I2S_UNUSE;

	if (MediaLib_Init(&init_input, &init_cb) == AK_FALSE){
		printf("unable to init MediaLib\n");
		return -1;
	}

	printf("init MediaLib ok \n");

	memset(&open_input, 0, sizeof(T_VIDEOLIB_ENC_OPEN_INPUT));

	open_input.m_VideoType = VIDEO_DRV_MPEG;
	open_input.m_ulWidth = width;
	open_input.m_ulHeight = height;
	open_input.m_ulMaxVideoSize = (((open_input.m_ulWidth*open_input.m_ulHeight>>1)+511)/512)*512;
	
	open_input.m_CBFunc.m_FunPrintf = (MEDIALIB_CALLBACK_FUN_PRINTF)printf;
	open_input.m_CBFunc.m_FunMalloc = (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
	open_input.m_CBFunc.m_FunFree = (MEDIALIB_CALLBACK_FUN_FREE)free;
	open_input.m_CBFunc.m_FunMMUInvalidateDCache = NULL;
	open_input.m_CBFunc.m_FunCleanInvalidateDcache = NULL;

	open_input.m_CBFunc.m_FunDMAMalloc             = (MEDIALIB_CALLBACK_FUN_DMA_MALLOC)akuio_alloc_pmem;
  	open_input.m_CBFunc.m_FunDMAFree               = (MEDIALIB_CALLBACK_FUN_DMA_FREE)akuio_free_pmem;
  	open_input.m_CBFunc.m_FunVaddrToPaddr          = (MEDIALIB_CALLBACK_FUN_VADDR_TO_PADDR)akuio_vaddr2paddr;
  	open_input.m_CBFunc.m_FunMapAddr               = (MEDIALIB_CALLBACK_FUN_MAP_ADDR)akuio_map_regs;
  	open_input.m_CBFunc.m_FunUnmapAddr             = (MEDIALIB_CALLBACK_FUN_UNMAP_ADDR)akuio_unmap_regs;
  	open_input.m_CBFunc.m_FunRegBitsWrite          = (MEDIALIB_CALLBACK_FUN_REG_BITS_WRITE)akuio_sysreg_write;
	open_input.m_CBFunc.m_FunVideoHWLock           = NULL;
	open_input.m_CBFunc.m_FunVideoHWUnlock         = NULL;
	
	g_hVS = VideoStream_Enc_Open(&open_input);
	if (AK_NULL == g_hVS)
	{
		return -1;
	}

	return 0;
#else
  	T_MEDIALIB_INIT_CB init_cb;
  	T_MEDIALIB_INIT_INPUT init_input;

	init_cb_func_init(&init_cb);
	init_input.m_ChipType = MEDIALIB_CHIP_AK9801;
	init_input.m_AudioI2S = I2S_UNUSE;

	if (MediaLib_Init(&init_input, &init_cb) == AK_FALSE){
		printf("unable to init MediaLib\n");
		return -1;
	}

	debug("init MediaLib ok \n");

	memset(&rec_open_input, 0, sizeof(T_MEDIALIB_REC_OPEN_INPUT));
	rec_open_input.m_MediaRecType	= MEDIALIB_REC_AVI_NORMAL;//or MEDIALIB_REC_3GP;
	rec_open_input.m_hMediaDest		= (T_S32)headfile;
	rec_open_input.m_bCaptureAudio	= 0;
	rec_open_input.m_bHighQuality	= 1;
	rec_open_input.m_SectorSize		= 2048;
	rec_open_input.m_bIdxInMem		= 1;
	rec_open_input.m_hIndexFile		= 0;

	rec_open_input.m_IndexMemSize	= (9 + 2) * 16 * 30*60;	// 0;
	rec_open_input.m_RecordSecond	= 0;
// set video open info
	rec_open_input.m_VideoRecInfo.m_nWidth				= width;
	rec_open_input.m_VideoRecInfo.m_nHeight				= height;
	rec_open_input.m_VideoRecInfo.m_nFPS				= 10;
	rec_open_input.m_VideoRecInfo.m_nKeyframeInterval	= 100;//(mVideoFrameRate<<1) -1;
	rec_open_input.m_VideoRecInfo.m_nvbps				= nvbps;//mVideoBitRate;
	rec_open_input.m_VideoRecInfo.m_eVideoType			= MEDIALIB_V_ENC_H263;  //MEDIALIB_V_ENC_MPEG;
	rec_open_input.m_ExFunEnc							= NULL;//set mjpeg encode function
	
// set audio open info
	rec_open_input.m_AudioRecInfo.m_Type				= _SD_MEDIA_TYPE_PCM;
	rec_open_input.m_AudioRecInfo.m_nChannel			= 1;
	rec_open_input.m_AudioRecInfo.m_BitsPerSample		= 16;
	rec_open_input.m_AudioRecInfo.m_nSampleRate			= 8000;
	rec_open_input.m_AudioRecInfo.m_ulDuration			= 1000;

// set callback 
	memset(&rec_open_input.m_CBFunc, 0, sizeof(T_MEDIALIB_CB));
	rec_open_input.m_CBFunc.m_FunPrintf					= (MEDIALIB_CALLBACK_FUN_PRINTF)printd;
  	rec_open_input.m_CBFunc.m_FunRead                  = (MEDIALIB_CALLBACK_FUN_READ)ak_rec_cb_fread;
	rec_open_input.m_CBFunc.m_FunWrite                 = (MEDIALIB_CALLBACK_FUN_WRITE)ak_rec_cb_fwrite;
	rec_open_input.m_CBFunc.m_FunSeek					= (MEDIALIB_CALLBACK_FUN_SEEK)ak_rec_cb_fseek;
	rec_open_input.m_CBFunc.m_FunTell					= (MEDIALIB_CALLBACK_FUN_TELL)ak_rec_cb_ftell;
	rec_open_input.m_CBFunc.m_FunMalloc					= (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
	rec_open_input.m_CBFunc.m_FunFree					= (MEDIALIB_CALLBACK_FUN_FREE)free;
	rec_open_input.m_CBFunc.m_FunLoadResource          = NULL;
	rec_open_input.m_CBFunc.m_FunReleaseResource       = NULL;
	rec_open_input.m_CBFunc.m_FunRtcDelay              = (MEDIALIB_CALLBACK_FUN_RTC_DELAY)ak_rec_cb_lnx_delay;
	rec_open_input.m_CBFunc.m_FunDMAMemcpy             = (MEDIALIB_CALLBACK_FUN_DMA_MEMCPY)memcpy;
	rec_open_input.m_CBFunc.m_FunMMUInvalidateDCache	= NULL;
	rec_open_input.m_CBFunc. m_FunCleanInvalidateDcache	= NULL;
	
	rec_open_input.m_CBFunc.m_FunFileSysIsBusy         = NULL;
	rec_open_input.m_CBFunc.m_FunFileHandleExist       = (MEDIALIB_CALLBACK_FUN_FILE_HANDLE_EXIST)ak_rec_cb_lnx_fhandle_exist;

	rec_open_input.m_CBFunc.m_FunDMAMalloc             = (MEDIALIB_CALLBACK_FUN_DMA_MALLOC)akuio_alloc_pmem;
  	rec_open_input.m_CBFunc.m_FunDMAFree               = (MEDIALIB_CALLBACK_FUN_DMA_FREE)akuio_free_pmem;
  	rec_open_input.m_CBFunc.m_FunVaddrToPaddr          = (MEDIALIB_CALLBACK_FUN_VADDR_TO_PADDR)akuio_vaddr2paddr;
  	rec_open_input.m_CBFunc.m_FunMapAddr               = (MEDIALIB_CALLBACK_FUN_MAP_ADDR)akuio_map_regs;
  	rec_open_input.m_CBFunc.m_FunUnmapAddr             = (MEDIALIB_CALLBACK_FUN_UNMAP_ADDR)akuio_unmap_regs;
  	rec_open_input.m_CBFunc.m_FunRegBitsWrite          = (MEDIALIB_CALLBACK_FUN_REG_BITS_WRITE)akuio_sysreg_write;
	rec_open_input.m_CBFunc.m_FunVideoHWLock           = NULL;
	rec_open_input.m_CBFunc.m_FunVideoHWUnlock         = NULL;
	
	mRecLibHandle = MediaLib_Rec_Open(&rec_open_input, &rec_open_output);
	if(mRecLibHandle == AK_NULL){	
		printf("MediaLib_Rec_Open Return NULL\n");

		closeFile();
		return -1;
	}

	debug("MediaLib_Rec_Open ok \n");
	
	T_MEDIALIB_REC_INFO RecInfo;
	if (MediaLib_Rec_GetInfo(mRecLibHandle, &RecInfo) == AK_FALSE)
	{
		printf ("MediaLib_Rec_GetInfo error\n");
		MediaLib_Rec_Close(mRecLibHandle);
		closeFile();
		return -1;
	}

	debug("RecInfo.total_frames=%d\n",RecInfo.total_frames);

	if (AK_FALSE == MediaLib_Rec_Start(mRecLibHandle))
	{
		printf ("MediaLib_Rec_Start error \n");
		MediaLib_Rec_Close(mRecLibHandle);
		closeFile();
		return -1;
	}

	debug("MediaLib_Rec_Start ok \n");

#endif

	return 0;
	//above only call one time when system start
}

static int need_i_frame;
void encode_need_i_frame()
{
	need_i_frame = 1;
}

#define QP_BEST 3
#define QP_WORST 15
#define QUALITY_BEST 100
#define QUALITY_WORST 0
int encode_main(char* yuv_buf, int size)
{
	static int count = 0;
	int encode_size;
	video_enc_io_param.p_curr_data = yuv_buf;
	video_enc_io_param.p_vlc_data = (char*)((encode_temp_buffer+32));
	video_enc_io_param.QP = (threadcfg.record_quality-QUALITY_WORST)*(QP_BEST-QP_WORST)/(QUALITY_BEST-QUALITY_WORST)+QP_WORST;
retry:
	if( count % 100 == 0 || need_i_frame ){
		video_enc_io_param.mode = 0;
		need_i_frame = 0;
		//printf("qp=%d\n",video_enc_io_param.QP);
	}
	else{
		video_enc_io_param.mode = 1;
	}
	video_enc_io_param.bInsertP = AK_FALSE;
	encode_size = VideoStream_Enc_Encode(g_hVS, &video_enc_io_param);
	if( need_i_frame ){		//if the flag is set during encoding, we have to retry to make sure I frame is next frame
		goto retry;
	}
	encode_temp_buf_size = 32;
	encode_temp_buf_size += encode_size;
	count++;
	return encode_temp_buf_size;
}

int MediaEncodeMain(T_U32 nvbps, int w, int h)
{
	openFile();

	if (-1 == openMedia(nvbps, w, h)) {
		printf("-----------------------------------------------------Failed to openMedia()\n");
		return -1;
	}

	//play film or music
//	record_media();

	//below only call one time when system close
//	MediaLib_Destroy();

	return 1;
}

int MediaDestroy()
{
	debug("MediaLib_Rec_Stop start\n");
	MediaLib_Rec_Stop(mRecLibHandle);

	debug("MediaLib_Rec_Close start\n");
	MediaLib_Rec_Close(mRecLibHandle);

	debug("MediaLib_Destroy start\n");
	MediaLib_Destroy();	

	closeFile();
//    data_chunk_free(_encode_buf);
 //   _encode_buf = NULL;
 //   pthread_mutex_destroy(&_encode_buf_lock);

	return 1;
}

int MediaRestart(unsigned int nvbps, int w, int h)
{
	int ret;
	ret = MediaDestroy(mRecLibHandle);
	if( ret == -1 ){
		printf("MediaDestroy failed\n");
		return -1;
	}
	ret = MediaEncodeMain(nvbps,w,h);
	if( ret == -1 ){
		printf("MediaEncodeMain failed\n");
		return -1;
	}
	return 0;
}

//don't change the record parameters, so restart faster
int MediaRestartFast()
{
	T_BOOL ret;
	ret = MediaLib_Rec_Stop(mRecLibHandle);
	if( ret == AK_FALSE ){
		printf("MediaLib_Rec_Stop failed\n");
		return -1;
	}
	ret = MediaLib_Rec_Restart(mRecLibHandle, 0, MEDIALIB_REC_EV_NORMAL);
	if( ret == AK_FALSE ){
		printf("MediaLib_Rec_Restart failed\n");
		return -1;
	}
	return 0;
}

int processVideoData(T_U8* dataPtr, int datasize, int32_t timeStamp)
{
	int libStatus = MediaLib_Rec_GetStatus(mRecLibHandle);
	if (libStatus != MEDIALIB_REC_DOING) {
		debug("video data discarded in status %d for rec lib %p",libStatus,mRecLibHandle);
		return -1;
	}

	if (MediaLib_Rec_EncodeVideo(mRecLibHandle, dataPtr, timeStamp)) {
		libStatus = MediaLib_Rec_WriteVideo(mRecLibHandle);
		if (libStatus < 0) {
			debug("MediaLib_Rec_ProcessVideo error...\r\n");
			return -1;
		}
	}
	//printf("### process video timestamp %u, datasize %d address %p\n", timeStamp, datasize, dataPtr);
	return 0;
}

int processAudioData(T_U8* dataPtr, int datasize)
{
	int libStatus = MediaLib_Rec_GetStatus(mRecLibHandle);
	if(libStatus != MEDIALIB_REC_DOING) {
		debug("Audio data discarded in status %d for rec lib %p",libStatus, mRecLibHandle);
		return -1;
	}

	if (AK_FALSE ==  MediaLib_Rec_WriteAudio(mRecLibHandle, dataPtr, datasize)) {
		debug("MediaLib_Rec_WriteAudio() fail...data len = %d\n", datasize);
		return -1;
	}
	
	return 0;
}
