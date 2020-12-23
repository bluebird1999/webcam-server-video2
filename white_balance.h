/*
 * white_balance.h
 *
 *  Created on: Sep 8, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO_WHITE_BALANCE_H_
#define SERVER_VIDEO_WHITE_BALANCE_H_

/*
 * header
 */
#include "../video/config.h"

/*
 * define
 */
#define	AWB_FRAME_INTERVAL		60

/*
 * structure
 */

/*
 * function
 */
int video_white_balance_int(isp_awb_para_t *ctrl);
int video_white_balance_release(void);
int video_white_balance_proc(isp_awb_para_t *ctrl, int frame);

#endif /* SERVER_VIDEO_WHITE_BALANCE_H_ */
