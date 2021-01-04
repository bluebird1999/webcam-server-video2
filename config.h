/*
 * config_video.h
 *
 *  Created on: Sep 1, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO2_CONFIG_H_
#define SERVER_VIDEO2_CONFIG_H_

/*
 * header
 */
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		CONFIG_VIDEO2_MODULE_NUM		4

#define		CONFIG_VIDEO2_PROFILE			0
#define		CONFIG_VIDEO2_ISP				1
#define		CONFIG_VIDEO2_H264				2
#define		CONFIG_VIDEO2_OSD				3

#define 	CONFIG_VIDEO2_PROFILE_PATH		"config/video2_profile.config"
#define 	CONFIG_VIDEO2_ISP_PATH			"config/video2_isp.config"
#define 	CONFIG_VIDEO2_H264_PATH			"config/video2_h264.config"
#define 	CONFIG_VIDEO2_OSD_PATH			"config/video2_osd.config"

#define		AUTO_PROFILE_START				3
#define		AUTO_PROFILE_NUM				6

/*
 * structure
 */

/*
 *
 */
typedef struct video2_profile_config_t {
	int						quality;
	struct rts_av_profile	profile[AUTO_PROFILE_START + AUTO_PROFILE_NUM];
} video2_profile_config_t;

typedef struct video2_h264_config_t {
	struct rts_h264_attr		h264_attr;
	struct rts_video_h264_ctrl	h264_ctrl;
	int							h264_gop[AUTO_PROFILE_START + AUTO_PROFILE_NUM];
	int							h264_bitrate[AUTO_PROFILE_START + AUTO_PROFILE_NUM];
	int							h264_level[AUTO_PROFILE_START + AUTO_PROFILE_NUM];
	int							h264_qp[AUTO_PROFILE_START + AUTO_PROFILE_NUM];
} video2_h264_config_t;

typedef struct video2_osd_config_t {
	int		enable;
	int		time_mode;
	int		time_rotate;
	char	time_font_face[MAX_SYSTEM_STRING_SIZE];
	int 	time_pixel_size;
	int		time_alpha;
	int		time_color;
	int		time_pos_mode;
	int		time_offset;
	int		time_flick;
	int		time_flick_on;
	int		time_flick_off;
	int		warning_mode;
	int		warning_rotate;
	char	warning_font_face[MAX_SYSTEM_STRING_SIZE];
	int 	warning_pixel_size;
	int		warning_alpha;
	int		warning_color;
	int		warning_pos_mode;
	int		warning_offset;
	int		warning_flick;
	int		warning_flick_on;
	int		warning_flick_off;
	int		label_mode;
	int		label_rotate;
	char	label_font_face[MAX_SYSTEM_STRING_SIZE];
	int 	label_pixel_size;
	int		label_alpha;
	int		label_color;
	int		label_pos_mode;
	int		label_offset;
	int		label_flick;
	int		label_flick_on;
	int		label_flick_off;
} video2_osd_config_t;

typedef struct video2_config_t {
	int							status;
	video2_profile_config_t		profile;
	struct rts_isp_attr			isp;
	video2_h264_config_t		h264;
	video2_osd_config_t 		osd;
} video2_config_t;

/*
 * function
 */
int video2_config_video_read(video2_config_t *vconf);
int video2_config_video_set(int module, void *t);

#endif /* SERVER_VIDEO2_CONFIG_H_ */
