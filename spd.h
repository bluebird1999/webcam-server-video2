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
int video2_spd_proc(video2_md_config_t *ctrl, av_packet_t *packet, rts_md_src *md_src, rts_pd_src *pd_src);
int video2_spd_init(int width, int height, rts_md_src *md_src, rts_pd_src *pd_src);
int video2_spd_release(void);

#endif /* SERVER_VIDEO_SPD_H_ */
