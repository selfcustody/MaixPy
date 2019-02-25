#ifndef __AVI_H
#define __AVI_H 
  
#include "stdbool.h"
#include "stdint.h"

#define AVI_RIFF_ID			0X46464952  
#define AVI_AVI_ID			0X20495641
#define AVI_LIST_ID			0X5453494C  
#define AVI_HDRL_ID			0X6C726468
#define AVI_MOVI_ID			0X69766F6D
#define AVI_STRL_ID			0X6C727473

#define AVI_AVIH_ID			0X68697661
#define AVI_STRH_ID			0X68727473
#define AVI_STRF_ID			0X66727473
#define AVI_STRD_ID			0X64727473

#define AVI_VIDS_STREAM		0X73646976
#define AVI_AUDS_STREAM		0X73647561

#define AVI_FORMAT_MJPG		0X47504A4D


typedef enum {
	AVI_STATUS_OK = 0     ,
	AVI_STATUS_ERR_RIFF   ,
	AVI_STATUS_ERR_AVI    ,
	AVI_STATUS_ERR_LIST   ,
	AVI_STATUS_ERR_HDRL   ,
	AVI_STATUS_ERR_AVIH   ,
	AVI_STATUS_ERR_STRL   ,
	AVI_STATUS_ERR_STRH   ,
	AVI_STATUS_ERR_STRF   ,
	AVI_STATUS_ERR_MOVI   ,
	AVI_STATUS_ERR_FORMAT ,
	AVI_STATUS_ERR_STREAM ,
    AVI_STATUS_MAX
}avi_status_t;

typedef struct
{	
	uint32_t riff_id;				//fixed 'RIFF' 0X61766968
	uint32_t file_size;			    //not include fiff_id and file_size
	uint32_t avi_id;				//fixed 'AVI '==0X41564920 
}avi_header_t;


typedef struct
{	
	uint32_t list_id;               // fiexed 'LIST'==0X4c495354
	uint32_t block_size;            // not include list_id and block_size
	uint32_t list_type;             // hdrl(info)/movi(data)/idxl(index,optionally)
}list_header_t;

typedef struct
{	
	uint32_t block_id;
	uint32_t block_size;
	uint32_t sec_per_frame;
	uint32_t max_byte_sec;
	uint32_t padding_franularity;
	uint32_t flags;
	uint32_t total_frame;
	uint32_t init_frames;
	uint32_t streams;
	uint32_t ref_buf_size;
	uint32_t width;
	uint32_t height;
	uint32_t reserved[4];
}avih_header_t;


typedef struct
{	
	uint32_t block_id;
	uint32_t block_size;
	uint32_t stream_type;
	uint32_t handler;
	uint32_t flags;
	uint32_t priority;
	uint32_t language;
	uint32_t init_frames;
	uint32_t scale;
	uint32_t rate;
	uint32_t start;
	uint32_t length;
 	uint32_t ref_buf_size;
    uint32_t quality;
	uint32_t sample_size;
	struct
	{				
	   	short left;
		short top;
		short right;
		short bottom;
	}frame;				
}strh_header_t;


typedef struct
{
	uint32_t	 bmp_size;
 	int32_t     width;
	int32_t     height;
	uint16_t     planes;
	uint16_t     bit_count;
	uint32_t     compression;
	uint32_t     size_image;
	int32_t     x_pix_per_meter;
	int32_t     y_pix_per_meter;
	uint32_t     color_used;
	uint32_t     color_important;
}bmp_header_t;

typedef struct 
{
	uint8_t  blue;
	uint8_t  green;
	uint8_t  red;
	uint8_t  reserved;
}avi_rgb_quad_t;

typedef struct 
{
	uint32_t block_id;         //strf==0X73747266
	uint32_t block_size;
	bmp_header_t bmi_header;
	avi_rgb_quad_t bm_colors[1];
}strf_bmp_header_t;



typedef struct 
{
	uint32_t block_id;			//块标志,strf==0X73747266
	uint32_t block_size;			//块大小(不包含最初的8字节,也就是BlockID和本BlockSize不计算在内)
   	uint16_t format_tag;			//格式标志:0X0001=PCM,0X0055=MP3...
	uint16_t channels;	  		//声道数,一般为2,表示立体声
	uint32_t sample_rate; 		//音频采样率
	uint32_t baud_rate;   		//波特率 
	uint16_t block_align; 		//数据块对齐标志
	uint16_t size;				//该结构大小
}strf_wav_header_t;


typedef struct
{
	uint32_t sec_per_frame;
	uint32_t total_frame;
	uint32_t width;
	uint32_t height;
	uint32_t audio_sample_rate;
	uint16_t audio_channels;
	uint16_t audio_buf_size;
	uint16_t audio_type;          //0X0001=PCM, 0X0050=MP2, 0X0055=MP3, 0X2000=AC3
	uint16_t stream_id;           //'dc'==0X6463(video) 'wb'==0X7762(audio)
	uint32_t stream_size;         // must be even, or +1 to be even
	uint8_t* video_flag;          //"00dc"/"01dc"
	uint8_t* audio_flag;          //"00wb"/"01wb"

    void* file;
} avi_t;


int avi_init(uint8_t* buff, uint32_t size, avi_t* avi);
uint32_t avi_srarch_id(uint8_t* buf, uint32_t size, uint8_t* id);
void avi_debug_info(avi_t* avi);

#endif

