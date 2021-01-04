/*
 * video.h
 *
 *  Created on: Aug 28, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO_VIDEO_H_
#define SERVER_VIDEO_VIDEO_H_

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
#define		THREAD_OSD					0
#define 	THREAD_3ACTRL				1
#define		THREAD_MD					2
#define		THREAD_VIDEO				3

#define		RUN_MODE_SAVE				0
#define		RUN_MODE_MICLOUD			4
#define		RUN_MODE_MOTION				8
#define 	RUN_MODE_MISS				16

#define		VIDEO_INIT_CONDITION_NUM			3
#define		VIDEO_INIT_CONDITION_CONFIG			0
#define		VIDEO_INIT_CONDITION_MIIO_TIME		1
#define		VIDEO_INIT_CONDITION_REALTEK		2

#define		VIDEO_EXIT_CONDITION				( (1 << SERVER_RECORDER) || (1 << SERVER_MISS) )

#define		VIDEO_ISP_IR_AUTO	2
/*
 * structure
 */
typedef struct video_md_run_t {
	int 				started;
	scheduler_time_t  	scheduler;
	int					mode;
	char				changed;
} video_md_run_t;
/*
 * function
 */

#endif /* SERVER_VIDEO_VIDEO_H_ */
