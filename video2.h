/*
 * video2.h
 *
 *  Created on: Aug 28, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO2_VIDEO2_H_
#define SERVER_VIDEO2_VIDEO2_H_

/*
 * header
 */
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include "config.h"

/*
 * define
 */
#define		THREAD_OSD				0
#define 	THREAD_3ACTRL			1

#define		RUN_MODE_SAVE			0
#define 	RUN_MODE_SEND_MISS		1
#define		RUN_MODE_SEND_MICLOUD	2

#define		VIDEO2_INIT_CONDITION_NUM			3
#define		VIDEO2_INIT_CONDITION_CONFIG		0
#define		VIDEO2_INIT_CONDITION_MIIO_TIME		1
#define		VIDEO2_INIT_CONDITION_REALTEK		2

/*
 * structure
 */
typedef struct video2_stream_t {
	//channel
	int isp;
	int h264;
	int osd;
	//data
	int	frame;
} video2_stream_t;
/*
 * function
 */

#endif /* SERVER_VIDEO2_VIDEO2_H_ */
