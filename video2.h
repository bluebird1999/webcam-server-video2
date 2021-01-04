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
#define		THREAD_OSD2			0
#define		THREAD_VIDEO2		1

#define 	RUN_MODE_MISS2		0

#define		VIDEO2_INIT_CONDITION_NUM			3
#define		VIDEO2_INIT_CONDITION_CONFIG		0
#define		VIDEO2_INIT_CONDITION_MIIO_TIME		1
#define		VIDEO2_INIT_CONDITION_REALTEK		2

#define		VIDEO2_EXIT_CONDITION			( (1 << SERVER_MISS) )

/*
 * structure
 */
/*
 * function
 */

#endif /* SERVER_VIDEO2_VIDEO2_H_ */
