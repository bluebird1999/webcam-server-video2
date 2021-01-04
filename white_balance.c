/*
 * white_balance.c
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
#endif
//program header
#include "../../tools/log.h"
//server header
#include "white_balance.h"

/*
 * static
 */
//variable
static int last_frame;
struct rts_isp_awb_ctrl *awb;

//function
static int white_balance_set_auto(int r_gain, int b_gain);
static int white_balance_set_temperature(int temperature);
static int white_balance_set_component(int r, int g, int b);

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

static int white_balance_set_auto(int r_gain, int b_gain)
{
	log_qcy(DEBUG_SERIOUS, "input r_gain,b_gain(%d, %d):",
			 awb->_auto.adjustment.r_gain,
			 awb->_auto.adjustment.b_gain);
	awb->mode = RTS_ISP_AWB_AUTO;
	awb->_auto.adjustment.r_gain = r_gain;
	awb->_auto.adjustment.b_gain = b_gain;
	rts_av_set_isp_awb(awb);
	return 0;
}

static int white_balance_set_temperature(int temperature)
{
	log_qcy(DEBUG_SERIOUS, "input temperature(%d):", awb->_manual.temperature);
	awb->mode = RTS_ISP_AWB_TEMPERATURE;
	awb->_manual.temperature = temperature;
	rts_av_set_isp_awb(awb);
	return 0;
}

static int white_balance_set_component(int r, int g, int b)
{
	log_qcy(DEBUG_SERIOUS, "input component red,green,blue(%d,%d,%d):",
			 awb->_component.red,
			 awb->_component.green,
			 awb->_component.blue);
	awb->mode = RTS_ISP_AWB_COMPONENT;
	awb->_component.red = r;
	awb->_component.green = g;
	awb->_component.blue = b;
	rts_av_set_isp_awb(awb);
	return 0;
}

/*
 * interface
 */
int video_white_balance_proc(isp_awb_para_t *ctrl, int frame)
{
	int ret=0;
	struct rts_isp_awb_gain ct_gain;
	if( (frame - last_frame) > AWB_FRAME_INTERVAL ) {
        ret = rts_av_query_isp_awb(&awb);
        if (ret) {
        	log_qcy(DEBUG_SERIOUS, "query isp af ctrl fail, ret = %d\n", ret);
        	RTS_SAFE_RELEASE(awb, rts_av_release_isp_awb);
        	return 0;
        }

        ret = rts_av_get_isp_awb_ct_gain(RTS_ISP_CT_2800K, &ct_gain);
		log_qcy(DEBUG_SERIOUS, "awb: rgain = %d bgain = %d",awb->_auto.adjustment.r_gain, awb->_auto.adjustment.b_gain);
		log_qcy(DEBUG_SERIOUS, "awb: ct rgain = %d ct bgain = %d",ct_gain.r_gain, ct_gain.b_gain);
		last_frame = frame;
	}
	return ret;
}

int video_white_balance_init(isp_awb_para_t *ctrl)
{
	int ret=0;
    last_frame = 0;
	ret = rts_av_query_isp_awb(&awb);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "query isp awb ctrl fail, ret = %d\n", ret);
		RTS_SAFE_RELEASE(awb, rts_av_release_isp_awb);
		return -1;
	}
/*
	if( ctrl->awb_mode == RTS_ISP_AWB_TEMPERATURE )
		white_balance_set_temperature(ctrl->awb_temperature);
	else if( ctrl->awb_mode == RTS_ISP_AWB_AUTO )
		white_balance_set_auto(ctrl->awb_rgain, ctrl->awb_bgain);
	else if( ctrl->awb_mode == RTS_ISP_AWB_COMPONENT )
		white_balance_set_component(ctrl->awb_r, ctrl->awb_g, ctrl->awb_b);
*/
	return ret;
}

int video_white_balance_release(void)
{
	int ret=0;
	RTS_SAFE_RELEASE(awb, rts_av_release_isp_awb);
    last_frame = 0;
	return ret;
}

