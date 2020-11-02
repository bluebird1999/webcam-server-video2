/*
 * isp.h
 *
 *  Created on: Sep 8, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO_ISP_H_
#define SERVER_VIDEO_ISP_H_

/*
 * header
 */
#include "config.h"

/*
 * define
 */


/*
 * structure
 */

/*
 * function
 */
int video2_isp_get_attr(unsigned int id);
int video2_isp_set_attr(unsigned int id, int value);
int video2_isp_init(video2_isp_config_t *ctrl);

#endif /* SERVER_VIDEO_ISP_H_ */
