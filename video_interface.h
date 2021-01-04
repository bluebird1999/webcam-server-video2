/*
 * vedio_interface.h
 *
 *  Created on: Aug 27, 2020
 *      Author: ning
 */
#ifndef SERVER_VIDEO_VIDEO_INTERFACE_H_
#define SERVER_VIDEO_VIDEO_INTERFACE_H_

/*
 * header
 */
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		SERVER_VIDEO_VERSION_STRING		"alpha-6.0"

#define		MSG_VIDEO_BASE						(SERVER_VIDEO<<16)
#define		MSG_VIDEO_SIGINT					(MSG_VIDEO_BASE | 0x0000)
#define		MSG_VIDEO_SIGINT_ACK				(MSG_VIDEO_BASE | 0x1000)
//video control message
#define		MSG_VIDEO_START						(MSG_VIDEO_BASE | 0x0010)
#define		MSG_VIDEO_START_ACK					(MSG_VIDEO_BASE | 0x1010)
#define		MSG_VIDEO_STOP						(MSG_VIDEO_BASE | 0x0011)
#define		MSG_VIDEO_STOP_ACK					(MSG_VIDEO_BASE | 0x1011)
#define		MSG_VIDEO_PROPERTY_SET				(MSG_VIDEO_BASE | 0x0012)
#define		MSG_VIDEO_PROPERTY_SET_ACK			(MSG_VIDEO_BASE | 0x1012)
#define		MSG_VIDEO_PROPERTY_SET_EXT			(MSG_VIDEO_BASE | 0x0013)
#define		MSG_VIDEO_PROPERTY_SET_EXT_ACK		(MSG_VIDEO_BASE | 0x1013)
#define		MSG_VIDEO_PROPERTY_SET_DIRECT		(MSG_VIDEO_BASE | 0x0014)
#define		MSG_VIDEO_PROPERTY_SET_DIRECT_ACK	(MSG_VIDEO_BASE | 0x1014)
#define		MSG_VIDEO_PROPERTY_GET				(MSG_VIDEO_BASE | 0x0015)
#define		MSG_VIDEO_PROPERTY_GET_ACK			(MSG_VIDEO_BASE | 0x1015)
#define		MSG_VIDEO_SNAPSHOT					(MSG_VIDEO_BASE | 0x0020)
#define		MSG_VIDEO_SNAPSHOT_ACK				(MSG_VIDEO_BASE | 0x1020)
#define		MSG_VIDE0_SNAPSHOT_THUMB			(MSG_VIDEO_BASE | 0x0021)
#define		MSG_VIDEO_SNAPSHOT_THUMB_ACK		(MSG_VIDEO_BASE | 0x1021)
#define		MSG_VIDEO_SPD_VIDEO_DATA			(MSG_VIDEO_BASE | 0x0030)

//video control command from
//standard camera
#define		VIDEO_PROPERTY_SWITCH						(0x0000 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_IMAGE_ROLLOVER				(0x0001 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_NIGHT_SHOT               	(0x0002 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_TIME_WATERMARK           	(0x0003 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
//standard motion detection
#define 	VIDEO_PROPERTY_MOTION_SWITCH          		(0x0010 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_MOTION_ALARM_INTERVAL    	(0x0011 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_MOTION_SENSITIVITY 			(0x0012 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_MOTION_START		        	(0x0013 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_MOTION_END		          	(0x0014 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
//qcy custom
#define 	VIDEO_PROPERTY_CUSTOM_WARNING_PUSH         	(0x0100 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO_PROPERTY_CUSTOM_DISTORTION          	(0x0101 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
//video control command others
#define		VIDEO_PROPERTY_QUALITY						(0x1000 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)


/*
 * structure
 */
typedef struct osd_text_info_t {
    char *text;
	int cnt;
	uint32_t x;
	uint32_t y;
	uint8_t *pdata;
	uint32_t len;
} osd_text_info_t;

/*
 * function
 */
int server_video_start(void);
int server_video_message( message_t *msg);

#endif
