/*
 * vedio2_interface.h
 *
 *  Created on: Aug 27, 2020
 *      Author: ning
 */
#ifndef SERVER_VIDEO2_INTERFACE_H_
#define SERVER_VIDEO2_INTERFACE_H_

/*
 * header
 */
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		SERVER_VIDEO2_VERSION_STRING		"alpha-5.2"

#define		MSG_VIDEO2_BASE						(SERVER_VIDEO2<<16)
#define		MSG_VIDEO2_SIGINT					(MSG_VIDEO2_BASE | 0x0000)
#define		MSG_VIDEO2_SIGINT_ACK				(MSG_VIDEO2_BASE | 0x1000)
//video control message
#define		MSG_VIDEO2_START					(MSG_VIDEO2_BASE | 0x0010)
#define		MSG_VIDEO2_START_ACK				(MSG_VIDEO2_BASE | 0x1010)
#define		MSG_VIDEO2_STOP						(MSG_VIDEO2_BASE | 0x0011)
#define		MSG_VIDEO2_STOP_ACK					(MSG_VIDEO2_BASE | 0x1011)
#define		MSG_VIDEO2_PROPERTY_SET				(MSG_VIDEO2_BASE | 0x0012)
#define		MSG_VIDEO2_PROPERTY_SET_ACK			(MSG_VIDEO2_BASE | 0x1012)
#define		MSG_VIDEO2_PROPERTY_SET_EXT			(MSG_VIDEO2_BASE | 0x0013)
#define		MSG_VIDEO2_PROPERTY_SET_EXT_ACK		(MSG_VIDEO2_BASE | 0x1013)
#define		MSG_VIDEO2_PROPERTY_SET_DIRECT		(MSG_VIDEO2_BASE | 0x0014)
#define		MSG_VIDEO2_PROPERTY_SET_DIRECT_ACK	(MSG_VIDEO2_BASE | 0x1014)
#define		MSG_VIDEO2_PROPERTY_GET				(MSG_VIDEO2_BASE | 0x0015)
#define		MSG_VIDEO2_PROPERTY_GET_ACK			(MSG_VIDEO2_BASE | 0x1015)

//video control command from
//standard camera
#define 	VIDEO2_PROPERTY_TIME_WATERMARK           	(0x0003 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
//video control command others
#define		VIDEO2_PROPERTY_QUALITY						(0x1000 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)


/*
 * structure
 */
typedef struct osd2_text_info_t {
    char *text;
	int cnt;
	uint32_t x;
	uint32_t y;
	uint8_t *pdata;
	uint32_t len;
} osd2_text_info_t;
/*
 * function
 */
int server_video2_start(void);
int server_video2_message(message_t *msg);

#endif
