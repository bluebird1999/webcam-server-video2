/*
 * exposure.c
 *
 *  Created on: Sep 8, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <stdio.h>
#include <stdlib.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif>
//program header
#include "../../tools/tools_interface.h"
//server header
#include "exposure.h"

/*
 * static
 */
//variable
static int last_frame;
struct rts_isp_ae_ctrl *ae;

//function
static int exposure_set_auto_none(void);
static int exposure_set_auto_target_delta(int td);
static int exposure_set_auto_gain_max(int gm);
static int exposure_set_auto_min_fps(int mf);
static int exposure_set_auto_weight(int w);
static int exposure_set_manual_total_gain(int tg);
static int exposure_set_manual_gain(int a, int d, int id);
static int exposure_set_manual_exposure_time(int et);

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

static int exposure_set_auto_none(void)
{
	log_qcy(DEBUG_SERIOUS, "setup exposure mode to auto");
	ae->mode = RTS_ISP_AE_AUTO;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_auto_target_delta(int td)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.target_delta = td;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_auto_gain_max(int gm)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.gain_max = gm;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_auto_min_fps(int mf)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.min_fps = mf;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_auto_weight(int w)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.win_weights[0] = w;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_manual_total_gain(int tg)
{
	ae->mode = RTS_ISP_AE_MANUAL;
	ae->_manual.total_gain = tg;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_manual_gain(int a, int d, int id)
{
	ae->mode = RTS_ISP_AE_MANUAL;
	ae->_manual.gain.analog = a;
	ae->_manual.gain.digital = d;
	ae->_manual.gain.isp_digital = id;
	rts_av_set_isp_ae(ae);
	return 0;
}

static int exposure_set_manual_exposure_time(int et)
{
	ae->mode = RTS_ISP_AE_MANUAL;
	ae->_manual.exposure_time = et;
	rts_av_set_isp_ae(ae);
	return 0;
}

/*
 * interface
 */
int video_exposure_proc(isp_ae_para_t *ctrl, int frame)
{
	int ret=0;
	if( (frame - last_frame) > AE_FRAME_INTERVAL ) {
        ret = rts_av_query_isp_ae(&ae);
        if (ret) {
        	log_qcy(DEBUG_SERIOUS, "query isp ae ctrl fail, ret = %d\n", ret);
        	RTS_SAFE_RELEASE(ae, rts_av_release_isp_ae);
        	return 0;
        }
		last_frame = frame;
	}
	return ret;
}

int video_exposure_init(isp_ae_para_t *ctrl)
{
	int ret=0;
    last_frame = 0;
    ret = rts_av_query_isp_ae(&ae);
    if (ret) {
    	log_qcy(DEBUG_SERIOUS, "query isp ae ctrl fail, ret = %d\n", ret);
    	RTS_SAFE_RELEASE(ae, rts_av_release_isp_ae);
        return -1;
    }
/*
	if( ctrl->ae_mode == AE_AUTO_MODE_NONE )
		exposure_set_auto_none();
	else if( ctrl->ae_mode == AE_AUTO_MODE_TARGET_DELTA )
		exposure_set_auto_target_delta(ctrl->ae_target_delta);
	else if( ctrl->ae_mode == AE_AUTO_MODE_GAIN_MAX )
		exposure_set_auto_gain_max(ctrl->ae_gain_max);
	else if( ctrl->ae_mode == AE_AUTO_MODE_MIN_FPS )
		exposure_set_auto_min_fps(ctrl->ae_min_fps);
	else if( ctrl->ae_mode == AE_AUTO_MODE_WEIGHT )
		exposure_set_auto_weight(ctrl->ae_weight);
	else if( ctrl->ae_mode == AE_MANUAL_MODE_TOTAL_GAIN )
		exposure_set_manual_total_gain(ctrl->ae_total_gain);
	else if( ctrl->ae_mode == AE_MANUAL_MODE_GAIN )
		exposure_set_manual_gain(ctrl->ae_analog, ctrl->ae_digital, ctrl->ae_isp_digital);
	else if( ctrl->ae_mode == AE_MANUAL_MODE_EXPOSURE_TIME )
		exposure_set_manual_exposure_time(&ctrl->ae_exposure_time);
*/
	return ret;
}

int video_exposure_release(void)
{
	int ret=0;
    last_frame = 0;
	RTS_SAFE_RELEASE(ae, rts_av_release_isp_ae);
	return ret;
}


