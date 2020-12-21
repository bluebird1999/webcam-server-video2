/*
 * spd.c
 *
 *  Created on: Oct 2, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtsbmp.h>
#include <malloc.h>
//program header
#include "../../tools/tools_interface.h"
#include "../../server/miio/miio_interface.h"
#include "../../server/recorder/recorder_interface.h"
#include "../../server/realtek/realtek_interface.h"
#include "../../server/micloud/micloud_interface.h"
//server header
#include "spd.h"
#include "config.h"
#include "video2_interface.h"

/*
 * static
 */

//variable
static void *md_handle;
static void *pd_handle;
static rts_img *src_img;
static rts_md_res *md_res;
static rts_md_cfg md_cfg;
static rts_pd_cfg pd_cfg;
static rts_pd_res pd_res = {0};
static unsigned long long int last_report = 0;

//function

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static int spd_trigger_message(video2_md_config_t *config)
{
	int ret = 0;
	unsigned long long int now;
	recorder_init_t init;
	if( config->cloud_report ) {
		now = time_get_now_stamp();
		if( config->alarm_interval < 1)
			config->alarm_interval = 1;
		if( ( now - last_report) >= config->alarm_interval * 60 ) {
			last_report = now;
			message_t msg;
			/********motion notification********/
			msg_init(&msg);
			msg.message = MICLOUD_EVENT_TYPE_PEOPLEMOTION;
			msg.sender = msg.receiver = SERVER_VIDEO2;
			msg.extra = &now;
			msg.extra_size = sizeof(now);
			ret = manager_common_send_message(SERVER_MICLOUD, &msg);
			/********recorder********/
			msg_init(&msg);
			msg.message = MSG_RECORDER_ADD;
			msg.sender = msg.receiver = SERVER_VIDEO2;
			init.video_channel = 1;
			init.mode = RECORDER_MODE_BY_TIME;
			init.type = RECORDER_TYPE_MOTION_DETECTION;
			init.audio = 1;
			memset(init.start, 0, sizeof(init.start));
			time_get_now_str(init.start);
			now += config->recording_length;
			memset(init.stop, 0, sizeof(init.stop));
			time_stamp_to_date(now, init.stop);
			init.repeat = 0;
			init.repeat_interval = 0;
			init.quality = 0;
			msg.arg = &init;
			msg.arg_size = sizeof(recorder_init_t);
			msg.extra = &now;
			msg.extra_size = sizeof(now);
			ret = manager_common_send_message(SERVER_RECORDER,    &msg);
			/********snap shot********/
			msg_init(&msg);
			msg.sender = msg.receiver = SERVER_VIDEO2;
			msg.arg_in.cat = 0;
			msg.arg_in.dog = 1;
			msg.arg_in.duck = 0;
			msg.arg_in.tiger = RTS_AV_CB_TYPE_ASYNC;
			msg.arg_in.chick = RECORDER_TYPE_MOTION_DETECTION;
			msg.message = MSG_VIDE02_SNAPSHOT;
			manager_common_send_message(SERVER_VIDEO2, &msg);
			/**********************************************/
		}
	}
	return ret;
}

static void spd_set_md_default_parameter(rts_md_cfg *cfg, int width, int height)
{
	cfg->w = width;
	cfg->h = height;
	cfg->md_opt = MD_BACKGROUND_MODELING;
	cfg->dmem_opt = 0;

	cfg->sensitivity = 5;
	cfg->scene_thd = 0.8 * width * height;

	//background modeling
	cfg->bgm_cfg.train_frames = 3;
	cfg->bgm_cfg.learn_rate = 248;
	cfg->bgm_cfg.fade_rate = 240;
	cfg->bgm_cfg.nr_bins = 16;
	cfg->bgm_cfg.bin_bits = 2;
	cfg->bgm_cfg.back_thd = 2;

	//frame difference
	cfg->fd_cfg.y_thd = 30;
	cfg->fd_cfg.uv_thd = 15;

	cfg->pp_en = 1;
	cfg->ioc_en = 0;
	cfg->nr_obj = 5;
}

static void spd_set_pd_default_parameter(rts_pd_cfg *cfg, int width, int height)
{
	cfg->cc_cfg.nr_cc_thd = 0.002 * width * height;
	cfg->cc_cfg.min_ar = 0.05;
	cfg->cc_cfg.max_ar = 3;
	cfg->cc_cfg.cc_ratio = 0.5;

	cfg->net_cfg.version = 3.1;
	cfg->net_cfg.cfg_filename = NULL;
	cfg->net_cfg.weights_filename = NULL;

	cfg->conf_thd = 0.5;
	cfg->pd_frames = 1;
	cfg->nr_pd = 5;

	cfg->w = width;
	cfg->h = height;
}

int video2_spd_proc(video2_md_config_t *ctrl, av_packet_t *packet, rts_md_src *md_src, rts_pd_src *pd_src)
{
	int ret = 0;
    pthread_rwlock_rdlock(packet->lock);
    if( ( *(packet->init) == 0 ) ||
    	( packet->data == NULL ) ) {
    	ret = -1;
    	goto exit;
	}
    if( ((packet->info.flag) & 0xF) != 1) { // only process i frame
    	ret = 0;
    	goto exit;
    }
	ret = rts_set_rts_img_data(src_img, packet->data, packet->info.size, NULL);
	if (ret < 0) {
    	ret = -1;
    	goto exit;
	}
	ret = rts_run_md(md_handle, md_src, md_res);
	if (ret < 0) {
    	ret = -1;
    	goto exit;
	}
	ret = rts_run_pd(pd_handle, pd_src, &pd_res);
	if (ret < 0) {
    	ret = -1;
    	goto exit;
	}
	log_qcy(DEBUG_VERBOSE, "md_res fl<%d>\t nrobj<%d>", md_res->motion_flag, md_res->nr_obj);
	log_qcy(DEBUG_VERBOSE, "\t pd_fl <%d>\t nrpd<%d>\n", pd_res.pd_flag, pd_res.nr_pd);
	if (pd_res.pd_flag) {
		for (int i = 0; i < pd_res.nr_pd; i++) {
			log_qcy(DEBUG_VERBOSE, "conf[%d] %.2f %.2f\n",
				i,
				pd_res.pd_boxes->conf[0],
				pd_res.pd_boxes->conf[1]
				);
		}
		spd_trigger_message(ctrl);
	}
exit:
	av_packet_sub(packet);
	pthread_rwlock_unlock(packet->lock);
    return ret;
}

int video2_spd_init(int width, int height, rts_md_src *md_src, rts_pd_src *pd_src)
{
	int ret = 0;
	//settings
	spd_set_md_default_parameter(&md_cfg, width, height);
	spd_set_pd_default_parameter(&pd_cfg, 128, 72);
	//init
	md_handle = rts_init_md(&md_cfg);
	pd_handle = rts_init_pd(&pd_cfg);
	src_img = rts_create_rts_img(width, height, RTS_8U, 2,
				RTS_FORMAT_YUV420, NULL);
	if (!md_handle || !pd_handle || !src_img) {
		ret = -1;
		video2_spd_release();
		return ret;
	}
	md_res = rts_create_res(md_handle);
	if (!md_res) {
		ret = -1;
		video2_spd_release();
		return ret;
	}
	md_src->src_img = src_img;
	pd_src->src_img = src_img;
	pd_src->md_res = md_res;
	return ret;
}

int video2_spd_release(void)
{
	rts_release_obj(&src_img);
	rts_release_handle(&md_handle);
	rts_release_handle(&pd_handle);
	rts_release_obj(&md_res);
}
