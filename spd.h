/*
 * spd.h
 *
 *  Created on: Oct 2, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO_SPD_H_
#define SERVER_VIDEO_SPD_H_

/*
 * header
 */
#include <rts_file_io.h>
#include <rts_common_function.h>
#include <rts_md.h>
#include <rts_pd.h>
#include <rts_network.h>

#include "../video/config.h"

/*
 * define
 */
#define 	SPD_CONFIG_FILE_NAME			"spd.config"
#define 	SPD_WEIGHT_FILE_NAME			"spd_weight.data"

/*
 * structure
 */

/*
 * function
 */
int video_spd_proc(video_spd_config_t *ctrl, rts_md_src *md_src, rts_pd_src *pd_src);
int video_spd_init(video_spd_config_t *ctrl, rts_md_src *md_src, rts_pd_src *pd_src);
int video_spd_release(void);

#endif /* SERVER_VIDEO_SPD_H_ */
