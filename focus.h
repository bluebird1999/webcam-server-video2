/*
 * focus.h
 *
 *  Created on: Sep 8, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO_FOCUS_H_
#define SERVER_VIDEO_FOCUS_H_

/*
 * header
 */
#include "../video/config.h"

/*
 * define
 */
#define	AF_FRAME_INTERVAL		60
/*
 * structure
 */

/*
 * function
 */
int video_focus_int(isp_af_para_t *ctrl);
int video_focus_release(void);
int video_focus_proc(isp_af_para_t *ctrl, int frame);

#endif /* SERVER_VIDEO_FOCUS_H_ */
