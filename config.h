/*
 * config_video2.h
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

/*
 * define
 */
#define		CONFIG_VIDEO2_MODULE_NUM			5

#define		CONFIG_VIDEO2_PROFILE				0
#define		CONFIG_VIDEO2_ISP					1
#define		CONFIG_VIDEO2_H264					2
#define		CONFIG_VIDEO2_OSD					3
#define		CONFIG_VIDEO2_3ACTRL				4

#define		AE_AUTO_MODE_NONE					0
#define		AE_AUTO_MODE_TARGET_DELTA			1
#define		AE_AUTO_MODE_GAIN_MAX				2
#define		AE_AUTO_MODE_MIN_FPS				3
#define		AE_AUTO_MODE_WEIGHT					4
#define		AE_MANUAL_MODE_TOTAL_GAIN			5
#define		AE_MANUAL_MODE_GAIN					6
#define		AE_MANUAL_MODE_EXPOSURE_TIME 		7

#define		OSD_FONT_PATH						"/opt/qcy/font/"
#define		JPG_LIBRARY_PATH					"/mnt/nfs/media/"
#define		MP4_LIBRARY_PATH					"/mnt/nfs/media/"

#define 	CONFIG_VIDEO2_PROFILE_PATH			"/opt/qcy/config/video2_profile.config"
#define 	CONFIG_VIDEO2_ISP_PATH				"/opt/qcy/config/video2_isp.config"
#define 	CONFIG_VIDEO2_H264_PATH				"/opt/qcy/config/video2_h264.config"
#define 	CONFIG_VIDEO2_OSD_PATH				"/opt/qcy/config/video2_osd.config"
#define 	CONFIG_VIDEO2_3ACTRL_PATH			"/opt/qcy/config/video2_3actrl.config"

/*
 * structure
 */
typedef struct isp_awb_para_t {
	int		awb_mode;
	int		awb_temperature;
	int		awb_rgain;
	int 	awb_bgain;
	int		awb_r;
	int		awb_g;
	int		awb_b;
} isp_awb_para_t;

typedef struct isp_af_para_t {
	int		af_window_size;
	int		af_window_num;
} isp_af_para_t;

typedef struct isp_ae_para_t {
	int		ae_mode;
	int 	ae_target_delta;
	int		ae_gain_max;
	int		ae_min_fps;
	int		ae_weight;
	int		ae_total_gain;
	int		ae_analog;
	int		ae_digital;
	int		ae_isp_digital;
	int		ae_exposure_time;
} isp_ae_para_t;

/*
 *
 */
typedef struct video2_profile_config_t {
	int						quality;
	struct rts_av_profile	profile[3];
} video2_profile_config_t;

typedef struct video2_isp_config_t {
	struct rts_isp_attr		isp_attr;
	int						awb_ctrl;
	int						af;
	int						exposure_mode;
	int						pan;
	int						tilt;
	int						mirror;
	int						flip;
	int						wdr_mode;
	int						wdr_level;
	int						ir_mode;
	int						smart_ir_mode;
	int						smart_ir_manual_level;
	int						ldc;
	int						noise_reduction;
	int						in_out_door_mode;
	int						detail_enhancement;
}video2_isp_config_t;

typedef struct video2_h264_config_t {
	struct rts_h264_attr		h264_attr;
	struct rts_video_h264_ctrl	h264_ctrl;
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

typedef struct video2_3actrl_config_t {
	isp_awb_para_t	awb_para;
	isp_af_para_t	af_para;
	isp_ae_para_t	ae_para;
} video2_3actrl_config_t;

typedef struct video2_config_t {
	int							status;
	video2_profile_config_t		profile;
	video2_isp_config_t			isp;
	video2_h264_config_t			h264;
	video2_osd_config_t 			osd;
	video2_3actrl_config_t		a3ctrl;
} video2_config_t;

/*
 * function
 */
int video2_config_video_read(video2_config_t *vconf);
int video2_config_video_set(int module, void *t);
int video2_config_video_get_config_status(int module);

#endif /* SERVER_VIDEO2_CONFIG_H_ */
