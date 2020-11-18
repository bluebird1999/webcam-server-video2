/*
 * video2.c
 *
 *  Created on: Aug 27, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <malloc.h>
#include <miss.h>
//program header
#include "../../manager/manager_interface.h"
#include "../../server/realtek/realtek_interface.h"
#include "../../tools/tools_interface.h"
#include "../../server/miss/miss_interface.h"
#include "../../server/miio/miio_interface.h"
#include "../../server/recorder/recorder_interface.h"
#include "../../server/device/device_interface.h"
//server header
#include "video2.h"
#include "video2_interface.h"
#include "video2.h"
#include "white_balance.h"
#include "focus.h"
#include "exposure.h"
#include "isp.h"
#include "config.h"

/*
 * static
 */
//variable
static 	message_buffer_t	message;
static 	server_info_t 		info;
static	video2_stream_t		stream;
static	video2_config_t		config;


//function
//common
static void *server_func(void);
static int server_message_proc(void);
static int server_release(void);
static int server_restart(void);
static void task_default(void);
static void task_error(void);
static void task_start(void);
static void task_stop(void);
static void task_control(void);
static void task_control_ext(void);
static int send_message(int receiver, message_t *msg);
static int server_get_status(int type, int st);
static int server_set_status(int type, int st, int value);
static void server_thread_termination(int sign);
//specific
static int write_video2_buffer(struct rts_av_buffer *data, int id, int target, int type);
static int *video2_3acontrol_func(void *arg);
static int *video2_osd_func(void *arg);
static int stream_init(void);
static int stream_destroy(void);
static int stream_start(void);
static int stream_stop(void);
static int video2_init(void);
static int video2_main(void);
/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
 */
static int send_message(int receiver, message_t *msg)
{
	int st = 0;
	switch(receiver) {
		case SERVER_DEVICE:
			st = server_device_message(msg);
			break;
		case SERVER_KERNEL:
	//		st = server_kernel_message(msg);
			break;
		case SERVER_REALTEK:
			st = server_realtek_message(msg);
			break;
		case SERVER_MIIO:
			st = server_miio_message(msg);
			break;
		case SERVER_MISS:
			st = server_miss_message(msg);
			break;
		case SERVER_MICLOUD:
	//		st = server_micloud_message(msg);
			break;
		case SERVER_AUDIO:
			st = server_audio_message(msg);
			break;
		case SERVER_RECORDER:
			st = server_recorder_message(msg);
			break;
		case SERVER_PLAYER:
			st = server_player_message(msg);
			break;
		case SERVER_SPEAKER:
			st = server_speaker_message(msg);
			break;
		case SERVER_VIDEO2:
			st = server_video2_message(msg);
			break;
		case SERVER_SCANNER:
//			st = server_scanner_message(msg);
			break;
		case SERVER_MANAGER:
			st = manager_message(msg);
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "unknown message target! %d", receiver);
			break;
	}
	return st;
}

static int video2_get_property(message_t *msg)
{
	int ret = 0, st;
	int temp;
	message_t send_msg;
    /********message body********/
	msg_init(&send_msg);
	memcpy(&(send_msg.arg_pass), &(msg->arg_pass),sizeof(message_arg_t));
	send_msg.message = msg->message | 0x1000;
	send_msg.sender = send_msg.receiver = SERVER_VIDEO2;
	send_msg.arg_in.cat = msg->arg_in.cat;
	send_msg.result = 0;
	/****************************/
	st = info.status;
	if( st < STATUS_WAIT ) {
		send_msg.result = -1;
		send_message( msg->receiver, &send_message);
	}
	else {
		if( send_msg.arg_in.cat == VIDEO2_PROPERTY_SWITCH) {
			temp = ( st == STATUS_RUN ) ? 1:0;
			send_msg.arg = (void*)(&temp);
			send_msg.arg_size = sizeof(temp);
		}
		else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_IMAGE_ROLLOVER) {
			if( config.isp.flip == 0 && config.isp.mirror == 0) temp = 0;
			else if( config.isp.flip == 1 && config.isp.mirror == 1) temp = 180;
			else{
				send_msg.result = -1;
			}
			send_msg.arg = (void*)(&temp);
			send_msg.arg_size = sizeof(temp);
		}
		else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_NIGHT_SHOT) {
			if( config.isp.smart_ir_mode == RTS_ISP_SMART_IR_MODE_AUTO) temp = 0;
			else if( config.isp.smart_ir_mode == RTS_ISP_SMART_IR_MODE_DISABLE) temp = 1;
			else if( config.isp.smart_ir_mode == RTS_ISP_SMART_IR_MODE_LOW_LIGHT_PRIORITY) temp = 2;
			send_msg.arg = (void*)(&temp);
			send_msg.arg_size = sizeof(temp);
		}
		else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_TIME_WATERMARK) {
			send_msg.arg = (void*)(&config.osd.enable);
			send_msg.arg_size = sizeof(config.osd.enable);
		}
		else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_CUSTOM_DISTORTION) {
			send_msg.arg = (void*)(&config.isp.ldc);
			send_msg.arg_size = sizeof(config.isp.ldc);
		}
		ret = send_message( msg->receiver, &send_msg);
	}
	return ret;
}

static int video2_set_property(message_t *msg)
{
	int ret=0, mode = -1;
	message_t send_msg;
    /********message body********/
	msg_init(&send_msg);
	memcpy(&(send_msg.arg_pass), &(msg->arg_pass),sizeof(message_arg_t));
	send_msg.message = msg->message | 0x1000;
	send_msg.sender = send_msg.receiver = SERVER_VIDEO2;
	send_msg.arg_in.cat = msg->arg_in.cat;
	/****************************/
	if( msg->arg_in.cat == VIDEO2_PROPERTY_IMAGE_ROLLOVER ) {
		int temp = *((int*)(msg->arg));
		if( temp == 0 && (config.isp.flip!=0 || config.isp.mirror!=0) ) {
			ret = video2_isp_set_attr(RTS_VIDEO_CTRL_ID_FLIP, 0);
			ret |= video2_isp_set_attr(RTS_VIDEO_CTRL_ID_MIRROR, 0);
			if(!ret) {
				config.isp.flip = 0;
				config.isp.mirror = 0;
				log_qcy(DEBUG_SERIOUS, "changed the isp flip = %d", config.isp.flip);
				log_qcy(DEBUG_SERIOUS, "changed the isp mirror = %d", config.isp.mirror);
				video2_config_video_set(CONFIG_VIDEO2_ISP, &config.isp);
			}
		}
		else if( temp == 180 && (config.isp.flip!=1 || config.isp.mirror!=1) ) {
			ret = video2_isp_set_attr(RTS_VIDEO_CTRL_ID_FLIP, 1);
			ret |= video2_isp_set_attr(RTS_VIDEO_CTRL_ID_MIRROR, 1);
			if(!ret) {
				config.isp.flip = 1;
				config.isp.mirror = 1;
				log_qcy(DEBUG_SERIOUS, "changed the isp flip = %d", config.isp.flip);
				log_qcy(DEBUG_SERIOUS, "changed the isp mirror = %d", config.isp.mirror);
				video2_config_video_set(CONFIG_VIDEO2_ISP, &config.isp);
			}
		}
	}
	else if( msg->arg_in.cat == VIDEO2_PROPERTY_NIGHT_SHOT ) {
		int temp = *((int*)(msg->arg));
		if( temp == 0) {	//automode
			ret = video2_isp_set_attr(RTS_VIDEO_CTRL_ID_SMART_IR_MODE, RTS_ISP_SMART_IR_MODE_AUTO);
			if(!ret) {
				config.isp.smart_ir_mode = RTS_ISP_SMART_IR_MODE_AUTO;
			}
			mode = DAY_NIGHT_AUTO;
		}
		else if( temp == 1) {//close
			ret = video2_isp_set_attr(RTS_VIDEO_CTRL_ID_SMART_IR_MODE, RTS_ISP_SMART_IR_MODE_DISABLE);
			if(!ret) {
				config.isp.smart_ir_mode = RTS_ISP_SMART_IR_MODE_DISABLE;
			}
			mode = DAY_NIGHT_OFF;
		}
		else if( temp == 2) {//open
			ret = video2_isp_set_attr(RTS_VIDEO_CTRL_ID_SMART_IR_MODE, RTS_ISP_SMART_IR_MODE_LOW_LIGHT_PRIORITY);
			if(!ret) {
				config.isp.smart_ir_mode = RTS_ISP_SMART_IR_MODE_LOW_LIGHT_PRIORITY;
			}
			mode = DAY_NIGHT_ON;
		}
		if(!ret) {
			log_qcy(DEBUG_SERIOUS, "changed the smart night mode = %d", config.isp.smart_ir_mode);
			video2_config_video_set(CONFIG_VIDEO2_ISP, &config.isp);
		    /********message body********/
			send_msg.arg_in.cat = DEVICE_CTRL_DAY_NIGHT_MODE;
			send_msg.arg = (void *)&mode;
			send_msg.arg_size = sizeof(mode);
			/***************************/
		}
	}
	else if( msg->arg_in.cat == VIDEO2_PROPERTY_CUSTOM_DISTORTION ) {
		int temp = *((int*)(msg->arg));
		if( temp != config.isp.ldc ) {
			ret = video2_isp_set_attr(RTS_VIDEO_CTRL_ID_LDC, temp );
			if(!ret) {
				config.isp.ldc = temp;
				log_qcy(DEBUG_SERIOUS, "changed the lens distortion correction = %d", config.isp.ldc);
				video2_config_video_set(CONFIG_VIDEO2_ISP, &config.isp);
			}
		}
	}
	/***************************/
	send_msg.result = ret;
	ret = send_message(msg->receiver, &send_msg);
	/***************************/
	return ret;
}

static int *video2_3acontrol_func(void *arg)
{
	video2_3actrl_config_t ctrl;
	server_status_t st;
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    misc_set_thread_name("server_video2_3a_control");
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl, (video2_3actrl_config_t*)arg, sizeof(video2_3actrl_config_t));
    video2_white_balance_init( &ctrl.awb_para);
    video2_exposure_init(&ctrl.ae_para);
    video2_focus_init(&ctrl.af_para);
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_3ACTRL, 1 );
    while( 1 ) {
    	st = info.status;
    	if( info.exit ) break;
    	if( st != STATUS_START && st != STATUS_RUN )
    		break;
    	else if( st == STATUS_START )
    		continue;
    	video2_white_balance_proc( &ctrl.awb_para,stream.frame);
    	video2_exposure_proc(&ctrl.ae_para,stream.frame);
    	video2_focus_proc(&ctrl.af_para,stream.frame);
    }
    //release
    log_qcy(DEBUG_SERIOUS, "-----------thread exit: server_video2_3a_control-----------");
    video2_white_balance_release();
    video2_exposure_release();
    video2_focus_release();
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_3ACTRL, 0 );
    pthread_exit(0);
}

static int *video2_osd_func(void *arg)
{
	int ret=0, st;
	video2_osd_config_t ctrl;
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    misc_set_thread_name("server_video2_osd");
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl,(video2_osd_config_t*)arg, sizeof(video2_osd_config_t));
    ret = video2_osd_init(&ctrl, stream.osd, config.profile.profile[config.profile.quality].video.width,
    		config.profile.profile[config.profile.quality].video.height);
    if( ret != 0) {
    	log_qcy(DEBUG_SERIOUS, "osd init error!");
    	goto exit;
    }
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_OSD, 1 );
    while( 1 ) {
    	if( info.exit ) break;
    	st = info.status;
    	if( st != STATUS_START && st != STATUS_RUN )
    		break;
    	else if( st == STATUS_START )
    		continue;
    	video2_osd_proc(&ctrl,stream.frame);
    }
    //release
exit:
    log_qcy(DEBUG_SERIOUS, "-----------thread exit: server_video2_osd-----------");
    video2_osd_release();
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_OSD, 0 );
    pthread_exit(0);
}

static int stream_init(void)
{
	stream.isp = -1;
	stream.h264 = -1;
	stream.osd = -1;
	stream.frame = 0;
}

static int stream_destroy(void)
{
	int ret = 0;
	if (stream.isp >= 0) {
		rts_av_destroy_chn(stream.isp);
		stream.isp = -1;
	}
	if (stream.h264 >= 0) {
		rts_av_destroy_chn(stream.h264);
		stream.h264 = -1;
	}
	if (stream.osd >= 0) {
		rts_av_destroy_chn(stream.osd);
		stream.osd = -1;
	}
	return ret;
}

static int stream_start(void)
{
	int ret=0;
	pthread_t isp_3a_id, osd_id, md_id;
	config.profile.profile[config.profile.quality].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	ret = rts_av_set_profile(stream.isp, &config.profile.profile[config.profile.quality]);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "set isp profile fail, ret = %d", ret);
		return -1;
	}
	if( stream.isp != -1 ) {
		ret = rts_av_enable_chn(stream.isp);
		if (ret) {
			log_qcy(DEBUG_SERIOUS, "enable isp fail, ret = %d", ret);
			return -1;
		}
	}
	else {
		return -1;
	}
	if( stream.h264 != -1 ) {
		ret = rts_av_enable_chn(stream.h264);
		if (ret) {
			log_qcy(DEBUG_SERIOUS, "enable h264 fail, ret = %d", ret);
			return -1;
		}
	}
	else {
		return -1;
	}
	if( config.osd.enable ) {
		if( stream.osd != -1 ) {
			ret = rts_av_enable_chn(stream.osd);
			if (ret) {
				log_qcy(DEBUG_SERIOUS, "enable osd fail, ret = %d", ret);
				return -1;
			}
		}
		else {
			return -1;
		}
	}
	stream.frame = 0;
    ret = rts_av_start_recv(stream.h264);
    if (ret) {
    	log_qcy(DEBUG_SERIOUS, "start recv h264 fail, ret = %d", ret);
    	return -1;
    }
/*    //start the 3a control thread
	ret = pthread_create(&isp_3a_id, NULL, video2_3acontrol_func, (void*)&config.a3ctrl);
	if(ret != 0) {
		log_qcy(DEBUG_SERIOUS, "3a control thread create error! ret = %d",ret);
		return -1;
	 }
	else {
		log_qcy(DEBUG_SERIOUS, "3a control thread create successful!");
	}
*/
	if( config.osd.enable && stream.osd != -1 ) {
		//start the osd thread
		ret = pthread_create(&osd_id, NULL, video2_osd_func, (void*)&config.osd);
		if(ret != 0) {
			log_qcy(DEBUG_SERIOUS, "osd thread create error! ret = %d",ret);
		 }
		else {
			log_qcy(DEBUG_SERIOUS, "osd thread create successful!");
		}
	}
    return 0;
}

static int stream_stop(void)
{
	int ret=0;
	if(stream.h264!=-1)
		ret = rts_av_stop_recv(stream.h264);
	if(stream.osd!=-1)
		ret = rts_av_disable_chn(stream.osd);
	if(stream.h264!=-1)
		ret = rts_av_disable_chn(stream.h264);
	if(stream.isp!=-1)
		ret = rts_av_disable_chn(stream.isp);
	return ret;
}

static int video2_init(void)
{
	int ret;
	stream_init();
	stream.isp = rts_av_create_isp_chn(&config.isp.isp_attr);
	if (stream.isp < 0) {
		log_qcy(DEBUG_SERIOUS, "fail to create isp chn, ret = %d", stream.isp);
		return -1;
	}
	log_qcy(DEBUG_SERIOUS, "isp chnno:%d", stream.isp);
	stream.h264 = rts_av_create_h264_chn(&config.h264.h264_attr);
	if (stream.h264 < 0) {
		log_qcy(DEBUG_SERIOUS, "fail to create h264 chn, ret = %d", stream.h264);
		return -1;
	}
	log_qcy(DEBUG_SERIOUS, "h264 chnno:%d", stream.h264);
	config.profile.profile[config.profile.quality].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	ret = rts_av_set_profile(stream.isp, &config.profile.profile[config.profile.quality]);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "set isp profile fail, ret = %d", ret);
		return -1;
	}
	if( config.osd.enable ) {
        stream.osd = rts_av_create_osd_chn();
        if (stream.osd < 0) {
        	log_qcy(DEBUG_SERIOUS, "fail to create osd chn, ret = %d\n", stream.osd);
        	return -1;
        }
        log_qcy(DEBUG_SERIOUS, "osd chnno:%d", stream.osd);
        ret = rts_av_bind(stream.isp, stream.osd);
    	if (ret) {
    		log_qcy(DEBUG_SERIOUS, "fail to bind isp and osd, ret %d", ret);
    		return -1;
    	}
    	ret = rts_av_bind(stream.osd, stream.h264);
    	if (ret) {
    		log_qcy(DEBUG_SERIOUS, "fail to bind osd and h264, ret %d", ret);
    		return -1;
    	}
	}
	else {
    	ret = rts_av_bind(stream.isp, stream.h264);
    	if (ret) {
    		log_qcy(DEBUG_SERIOUS, "fail to bind isp and h264, ret %d", ret);
    		return -1;
    	}
	}
	video2_isp_init(&config.isp);
	return 0;
}

static int video2_main(void)
{
	int ret = 0;
	struct rts_av_buffer *buffer = NULL;
	usleep(1000);
	if (rts_av_poll(stream.h264))
		return 0;
	if (rts_av_recv(stream.h264, &buffer))
		return 0;
	if (buffer) {
		if( misc_get_bit(info.status2, RUN_MODE_SEND_MISS) ) {
			if( write_video2_buffer(buffer, MSG_MISS_VIDEO_DATA, SERVER_MISS, 0) != 0 )
				log_qcy(DEBUG_SERIOUS, "Miss ring buffer push failed!");
		}
		if( misc_get_bit(info.status2, RUN_MODE_SAVE) ) {
			if( write_video2_buffer(buffer, MSG_RECORDER_VIDEO_DATA, SERVER_RECORDER, RECORDER_TYPE_NORMAL) != 0 )
				log_qcy(DEBUG_SERIOUS, "Recorder ring buffer push failed!");
		}
		if( misc_get_bit(info.status2, RUN_MODE_SEND_MICLOUD) ) {
/*	wait for other server
 * 			if( write_video2_buffer(buffer, MSG_MICLOUD_VIDEO2_DATA, SERVER_MICLOUD, 0) != 0 )
				log_qcy(DEBUG_SERIOUS, "Micloud ring buffer push failed!");
*/
		}
		stream.frame++;
		rts_av_put_buffer(buffer);
	}
    return ret;
}

static int write_video2_buffer(struct rts_av_buffer *data, int id, int target, int type)
{
	int ret=0;
	message_t msg;
	av_data_info_t	info;
    /********message body********/
	msg_init(&msg);
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.arg_in.cat = type;
	msg.message = id;
	msg.extra = data->vm_addr;
	msg.extra_size = data->bytesused;
	info.flag = data->flags;
	info.frame_index = data->frame_idx;
	info.timestamp = data->timestamp / 1000;
	info.fps = config.profile.profile[config.profile.quality].video.denominator;
	info.width = config.profile.profile[config.profile.quality].video.width;
	info.height = config.profile.profile[config.profile.quality].video.height;
   	info.flag |= FLAG_STREAM_TYPE_LIVE << 11;
   	if(config.osd.enable)
   		info.flag |= FLAG_WATERMARK_TIMESTAMP_EXIST << 13;
   	else
   		info.flag |= FLAG_WATERMARK_TIMESTAMP_NOT_EXIST << 13;
    if( misc_get_bit(data->flags, 0/*RTSTREAM_PKT_FLAG_KEY*/) )// I frame
    	info.flag |= FLAG_FRAME_TYPE_IFRAME << 0;
    else
    	info.flag |= FLAG_FRAME_TYPE_PBFRAME << 0;
    if( config.profile.quality==0 )
        info.flag |= FLAG_RESOLUTION_VIDEO_360P << 17;
    else if( config.profile.quality==1 )
        info.flag |= FLAG_RESOLUTION_VIDEO_480P << 17;
    else if( config.profile.quality==2 )
        info.flag |= FLAG_RESOLUTION_VIDEO_1080P << 17;
	msg.arg = &info;
	msg.arg_size = sizeof(av_data_info_t);
	if( target == SERVER_MISS )
		ret = server_miss_video_message(&msg);
//	else if( target == SERVER_MICLOUD )
//		ret = server_micloud_video_message(&msg);
	else if( target == SERVER_RECORDER )
		ret = server_recorder_video_message(&msg);
	/****************************/
	return ret;
}

static int server_set_status(int type, int st, int value)
{
	int ret=-1;
	ret = pthread_rwlock_wrlock(&info.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add lock fail, ret = %d", ret);
		return ret;
	}
	if(type == STATUS_TYPE_STATUS)
		info.status = st;
	else if(type==STATUS_TYPE_EXIT)
		info.exit = st;
	else if(type==STATUS_TYPE_CONFIG)
		config.status = st;
	else if(type==STATUS_TYPE_THREAD_START)
		misc_set_bit(&info.thread_start, st, value);
	ret = pthread_rwlock_unlock(&info.lock);
	if (ret)
		log_qcy(DEBUG_SERIOUS, "add unlock fail, ret = %d", ret);
	return ret;
}

static int server_get_status(int type, int value)
{
	int st;
	int ret;
	ret = pthread_rwlock_wrlock(&info.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add lock fail, ret = %d", ret);
		return ret;
	}
	if(type == STATUS_TYPE_STATUS)
		st = info.status;
	else if(type== STATUS_TYPE_EXIT)
		st = info.exit;
	else if(type==STATUS_TYPE_CONFIG)
		st = config.status;
	else if(type==STATUS_TYPE_THREAD_START)
		st = misc_get_bit(info.thread_start, value);
	ret = pthread_rwlock_unlock(&info.lock);
	if (ret)
		log_qcy(DEBUG_SERIOUS, "add unlock fail, ret = %d", ret);
	return st;
}

static void server_thread_termination(int sign)
{
    /********message body********/
	message_t msg;
	msg_init(&msg);
	msg.message = MSG_VIDEO2_SIGINT;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	manager_message(&msg);
	/****************************/
}

static int server_restart(void)
{
	int ret = 0;
	stream_stop();
	stream_destroy();
	return ret;
}

static int server_release_1(void)
{
	int ret = 0;
	stream_stop();
	stream_destroy();
	usleep(1000*10);
	return ret;
}

static int server_release_2(void)
{
	int ret = 0;
	msg_buffer_release(&message);
	msg_free(&info.task.msg);
	memset(&info,0,sizeof(server_info_t));
	memset(&config,0,sizeof(video2_config_t));
	memset(&stream,0,sizeof(video2_stream_t));
	return ret;
}

static int server_message_proc(void)
{
	int ret = 0, ret1 = 0;
	message_t msg, send_msg;
	msg_init(&msg);
	ret = pthread_rwlock_wrlock(&message.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add message lock fail, ret = %d\n", ret);
		return ret;
	}
	if( info.msg_lock ) {
		ret1 = pthread_rwlock_unlock(&message.lock);
		return 0;
	}
	ret = msg_buffer_pop(&message, &msg);
	ret1 = pthread_rwlock_unlock(&message.lock);
	if (ret1) {
		log_qcy(DEBUG_SERIOUS, "add message unlock fail, ret = %d\n", ret1);
	}
	if( ret == -1) {
		msg_free(&msg);
		return -1;
	}
	else if( ret == 1)
		return 0;

    /********message body********/
	msg_init(&send_msg);
	memcpy(&(send_msg.arg_pass), &(msg.arg_pass),sizeof(message_arg_t));
	send_msg.message = msg.message | 0x1000;
	send_msg.sender = send_msg.receiver = SERVER_VIDEO2;
	send_msg.result = 0;
	/***************************/
	switch(msg.message) {
		case MSG_VIDEO2_START:
			if( msg.sender == SERVER_MISS) misc_set_bit(&info.status2, RUN_MODE_SEND_MISS, 1);
			if( msg.sender == SERVER_MICLOUD) misc_set_bit(&info.status2, RUN_MODE_SEND_MICLOUD, 1);
			if( msg.sender == SERVER_RECORDER) misc_set_bit(&info.status2, RUN_MODE_SAVE, 1);
			if( info.status == STATUS_RUN ) {
				ret = send_message(msg.receiver, &send_msg);
				break;
			}
			info.task.func = task_start;
			info.task.start = info.status;
			memcpy(&info.task.msg, &msg,sizeof(message_t));
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_STOP:
			if( msg.sender == SERVER_MISS) misc_set_bit(&info.status2, RUN_MODE_SEND_MISS, 0);
			if( msg.sender == SERVER_MICLOUD) misc_set_bit(&info.status2, RUN_MODE_SEND_MICLOUD, 0);
			if( msg.sender == SERVER_RECORDER) misc_set_bit(&info.status2, RUN_MODE_SAVE, 0);
			if( info.status != STATUS_RUN ) {
				ret = send_message(msg.receiver, &send_msg);
				break;
			}
			if( info.status2 > 0 ) {
				ret = send_message(msg.receiver, &send_msg);
				break;
			}
			info.task.func = task_stop;
			info.task.start = info.status;
			msg_deep_copy(&info.task.msg, &msg);
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_PROPERTY_SET:
			if(msg.arg_in.cat == VIDEO2_PROPERTY_QUALITY) {
				int temp = *((int*)(msg.arg));
				if( temp == config.profile.quality) {
					ret = send_message(msg.receiver, &send_msg);
					break;
				}
			}
			info.task.func = task_control;
			info.task.start = info.status;
			msg_deep_copy(&info.task.msg, &msg);
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_PROPERTY_SET_EXT:
			if( msg.arg_in.cat == VIDEO2_PROPERTY_TIME_WATERMARK ) {
				int temp = *((int*)(msg.arg));
				if( temp  == config.osd.enable) {
					ret = send_message(msg.receiver, &send_msg);
					break;
				}
			}
			else if( msg.arg_in.cat == VIDEO2_PROPERTY_IMAGE_ROLLOVER ) {
				int temp =  *((int*)(msg.arg));
				if( ( (temp == 0) && (config.h264.h264_attr.rotation == RTS_AV_ROTATION_0) ) ||
					( ( temp == 180) && (config.h264.h264_attr.rotation == RTS_AV_ROTATION_180)) ||
					( (temp == 90) && (config.h264.h264_attr.rotation == RTS_AV_ROTATION_90R)) ||
					( (temp == 270) && (config.h264.h264_attr.rotation == RTS_AV_ROTATION_90L))
				) {
					ret = send_message(msg.receiver, &send_msg);
					break;
				}

			}
			info.task.func = task_control_ext;
			info.task.start = info.status;
			msg_deep_copy(&info.task.msg, &msg);
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_PROPERTY_SET_DIRECT:
			video2_set_property(&msg);
			break;
		case MSG_VIDEO2_PROPERTY_GET:
			ret = video2_get_property(&msg);
			break;
		case MSG_MANAGER_EXIT:
			info.exit = 1;
			break;
		case MSG_MANAGER_TIMER_ACK:
			((HANDLER)msg.arg_in.handler)();
			break;
		case MSG_MIIO_PROPERTY_NOTIFY:
		case MSG_MIIO_PROPERTY_GET_ACK:
			if( msg.arg_in.cat == MIIO_PROPERTY_TIME_SYNC ) {
				if( msg.arg_in.dog == 1 )
					misc_set_bit( &info.thread_exit, VIDEO2_INIT_CONDITION_MIIO_TIME, 1);
			}
			break;
		case MSG_REALTEK_PROPERTY_NOTIFY:
		case MSG_REALTEK_PROPERTY_GET_ACK:
			if( msg.arg_in.cat == REALTEK_PROPERTY_AV_STATUS ) {
				if( msg.arg_in.dog == 1 )
					misc_set_bit(&info.thread_exit, VIDEO2_INIT_CONDITION_REALTEK, 1);
			}
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "not processed message = %x", msg.message);
			break;
	}
	msg_free(&msg);
	return ret;
}

static int heart_beat_proc(void)
{
	int ret = 0;
	message_t msg;
	long long int tick = 0;
	tick = time_get_now_stamp();
	if( (tick - info.tick3) > SERVER_HEARTBEAT_INTERVAL ) {
		info.tick3 = tick;
	    /********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_HEARTBEAT;
		msg.sender = msg.receiver = SERVER_VIDEO2;
		msg.arg_in.cat = info.status;
		msg.arg_in.dog = info.thread_start;
		msg.arg_in.duck = info.thread_exit;
		ret = manager_message(&msg);
		/***************************/
	}
	return ret;
}

/*
 * task
 */
/*
 * task error: error->5 seconds->shut down server->msg manager
 */
static void task_error(void)
{
	unsigned int tick=0;
	switch( info.status ) {
		case STATUS_ERROR:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!!error in video2, restart in 5 s!");
			info.tick = time_get_now_stamp();
			info.status = STATUS_NONE;
			break;
		case STATUS_NONE:
			tick = time_get_now_stamp();
			if( (tick - info.tick) > SERVER_RESTART_PAUSE ) {
				info.exit = 1;
				info.tick = tick;
			}
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_error = %d", info.status);
			break;
	}
	usleep(1000);
	return;
}
/*
 * task control: restart->wait->change->setup->start->run
 */
static void task_control_ext(void)
{
	static int para_set = 0;
	message_t msg;
	int ret = 0;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_RUN:
			if( !para_set ) info.status = STATUS_RESTART;
			else
				goto success_exit;
			break;
		case STATUS_IDLE:
			if( !para_set ) info.status = STATUS_RESTART;
			else {
				if( info.task.start == STATUS_IDLE ) goto success_exit;
				else info.status = STATUS_START;
			}
			break;
		case STATUS_WAIT:
			if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_TIME_WATERMARK ) {
				config.osd.enable = *((int*)(info.task.msg.arg));
				log_qcy(DEBUG_SERIOUS, "changed the osd switch = %d", config.osd.enable);
				video2_config_video_set(CONFIG_VIDEO2_OSD,  &config.osd);
			}
			else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_IMAGE_ROLLOVER ) {
				int temp = *((int*)(info.task.msg.arg));
				if( temp == 0 ) config.h264.h264_attr.rotation = RTS_AV_ROTATION_0;
				else if( temp == 90 ) config.h264.h264_attr.rotation = RTS_AV_ROTATION_90R;
				else if( temp == 270 ) config.h264.h264_attr.rotation = RTS_AV_ROTATION_90L;
				else if( temp == 180 ) config.h264.h264_attr.rotation = RTS_AV_ROTATION_180;
				log_qcy(DEBUG_SERIOUS, "changed the rotation = %d", config.h264.h264_attr.rotation );
				video2_config_video_set(CONFIG_VIDEO2_H264,  &config.h264);
			}
			para_set = 1;
			if( info.task.start == STATUS_WAIT ) goto success_exit;
			else info.status = STATUS_SETUP;
			break;
		case STATUS_START:
			if( stream_start()==0 ) info.status = STATUS_RUN;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_SETUP:
			if( video2_init() == 0) info.status = STATUS_IDLE;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_RESTART:
			server_restart();
			info.status = STATUS_WAIT;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			ret = send_message(info.task.msg.receiver, &msg);
			goto exit;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_control_ext = %d", info.status);
			break;
	}
	usleep(1000);
	return;
success_exit:
	ret = send_message(info.task.msg.receiver, &msg);
exit:
	para_set = 0;
	info.task.func = &task_default;
	info.msg_lock = 0;
	msg_free(&info.task.msg);
	return;
}
/*
 * task control: stop->idle->change->start->run
 */
static void task_control(void)
{
	static int para_set = 0;
	message_t msg;
	int ret = 0;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_RUN:
			if( !para_set ) info.status = STATUS_STOP;
			else goto success_exit;
			break;
		case STATUS_STOP:
			if( stream_stop()==0 ) info.status = STATUS_IDLE;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_IDLE:
			if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_QUALITY ) {
				config.profile.quality = *((int*)(info.task.msg.arg));
				log_qcy(DEBUG_SERIOUS, "changed the quality = %d", config.profile.quality);
				video2_config_video_set(CONFIG_VIDEO2_PROFILE, &config.profile);
			}
			para_set = 1;
			if( info.task.start == STATUS_IDLE ) goto success_exit;
			else info.status = STATUS_START;
			break;
		case STATUS_START:
			if( stream_start()==0 ) info.status = STATUS_RUN;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			ret = send_message( info.task.msg.receiver, &msg);
			if( !ret ) goto exit;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_control = %d", info.status);
			break;
	}
	usleep(1000);
	return;
success_exit:
	ret = send_message( info.task.msg.receiver, &msg);
exit:
	para_set = 0;
	info.task.func = &task_default;
	info.msg_lock = 0;
	msg_free(&info.task.msg);
	return;
}
/*
 * task start: idle->start
 */
static void task_start(void)
{
	message_t msg;
	int ret = 0;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	/***************************/
	switch(info.status){
	case STATUS_NONE:
		if( !misc_get_bit( info.thread_exit, VIDEO2_INIT_CONDITION_CONFIG ) ) {
			ret = video2_config_video_read(&config);
			if( !ret && misc_full_bit( config.status, CONFIG_VIDEO2_MODULE_NUM) ) {
				misc_set_bit(&info.thread_exit, VIDEO2_INIT_CONDITION_CONFIG, 1);
			}
			else {
				info.status = STATUS_ERROR;
				break;
			}
		}
		if( !misc_get_bit( info.thread_exit, VIDEO2_INIT_CONDITION_REALTEK )
				&& ((time_get_now_stamp() - info.tick2 ) > MESSAGE_RESENT) ) {
				info.tick2 = time_get_now_stamp();
		    /********message body********/
			msg_init(&msg);
			msg.message = MSG_REALTEK_PROPERTY_GET;
			msg.sender = msg.receiver = SERVER_VIDEO2;
			msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
			server_realtek_message(&msg);
			/****************************/
		}
		if( !misc_get_bit( info.thread_exit, VIDEO2_INIT_CONDITION_MIIO_TIME )
				&& ((time_get_now_stamp() - info.tick2 ) > MESSAGE_RESENT) ) {
				info.tick2 = time_get_now_stamp();
		    /********message body********/
			msg_init(&msg);
			msg.message = MSG_MIIO_PROPERTY_GET;
			msg.sender = msg.receiver = SERVER_VIDEO2;
			msg.arg_in.cat = MIIO_PROPERTY_TIME_SYNC;
			server_miio_message(&msg);
			/****************************/
		}
		if( misc_full_bit( info.thread_exit, VIDEO2_INIT_CONDITION_NUM ) )
			info.status = STATUS_WAIT;
		break;
	case STATUS_WAIT:
		info.status = STATUS_SETUP;
		break;
		case STATUS_RUN:
			ret = send_message(info.task.msg.receiver, &msg);
			goto exit;
			break;
		case STATUS_IDLE:
			info.status = STATUS_START;
			break;
		case STATUS_START:
			if( stream_start()==0 ) info.status = STATUS_RUN;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			ret = send_message(info.task.msg.receiver, &msg);
			goto exit;
			break;
		case STATUS_SETUP:
			if( video2_init() == 0) info.status = STATUS_IDLE;
			else info.status = STATUS_ERROR;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_start = %d", info.status);
			break;
	}
	usleep(1000);
	return;
exit:
	info.task.func = &task_default;
	info.msg_lock = 0;
	msg_free(&info.task.msg);
	return;
}
/*
 * task start: run->stop->idle
 */
static void task_stop(void)
{
	message_t msg;
	int ret = 0;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_IDLE:
			ret = send_message(info.task.msg.receiver, &msg);
			goto exit;
			break;
		case STATUS_RUN:
			if( stream_stop()==0 ) info.status = STATUS_IDLE;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			ret = send_message(info.task.msg.receiver, &msg);
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_stop = %d", info.status);
			break;
	}
	usleep(1000);
	return;
exit:
	info.task.func = &task_default;
	info.msg_lock = 0;
	msg_free(&info.task.msg);
	return;
}
/*
 * default task: none->run
 */
static void task_default(void)
{
	int ret = 0;
	message_t msg;
	switch( info.status ){
		case STATUS_NONE:
			if( !misc_get_bit( info.thread_exit, VIDEO2_INIT_CONDITION_CONFIG ) ) {
				ret = video2_config_video_read(&config);
				if( !ret && misc_full_bit( config.status, CONFIG_VIDEO2_MODULE_NUM) ) {
					misc_set_bit(&info.thread_exit, VIDEO2_INIT_CONDITION_CONFIG, 1);
				}
				else {
					info.status = STATUS_ERROR;
					break;
				}
			}
			if( !misc_get_bit( info.thread_exit, VIDEO2_INIT_CONDITION_REALTEK )
					&& ((time_get_now_stamp() - info.tick2 ) > MESSAGE_RESENT) ) {
					info.tick2 = time_get_now_stamp();
			    /********message body********/
				msg_init(&msg);
				msg.message = MSG_REALTEK_PROPERTY_GET;
				msg.sender = msg.receiver = SERVER_VIDEO2;
				msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
				server_realtek_message(&msg);
				/****************************/
			}
			if( !misc_get_bit( info.thread_exit, VIDEO2_INIT_CONDITION_MIIO_TIME )
					&& ((time_get_now_stamp() - info.tick2 ) > MESSAGE_RESENT) ) {
					info.tick2 = time_get_now_stamp();
			    /********message body********/
				msg_init(&msg);
				msg.message = MSG_MIIO_PROPERTY_GET;
				msg.sender = msg.receiver = SERVER_VIDEO2;
				msg.arg_in.cat = MIIO_PROPERTY_TIME_SYNC;
				server_miio_message(&msg);
				/****************************/
			}
			if( misc_full_bit( info.thread_exit, VIDEO2_INIT_CONDITION_NUM ) )
				info.status = STATUS_WAIT;
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			if( video2_init() == 0) info.status = STATUS_IDLE;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_IDLE:
//			info.status = STATUS_START;
			break;
		case STATUS_START:
			if( stream_start()==0 ) info.status = STATUS_RUN;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_RUN:
			if(video2_main()!=0) info.status = STATUS_STOP;
			break;
		case STATUS_STOP:
			if( stream_stop()==0 ) info.status = STATUS_IDLE;
			else info.status = STATUS_ERROR;
			break;
		case STATUS_ERROR:
			info.task.func = task_error;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_default = %d", info.status);
			break;
		}
	usleep(1000);
	return;
}

/*
 * server entry point
 */
static void *server_func(void)
{
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
	misc_set_thread_name("server_video2");
	pthread_detach(pthread_self());
	if( !message.init ) {
		msg_buffer_init(&message, MSG_BUFFER_OVERFLOW_NO);
	}
	//default task
	info.task.func = task_default;
	info.task.start = STATUS_NONE;
	info.task.end = STATUS_RUN;
	while( !info.exit ) {
		info.task.func();
		server_message_proc();
		heart_beat_proc();
	}
	server_release_1();
	if( info.exit ) {
		while( info.thread_start ) {
			log_qcy(DEBUG_INFO, "---------------locked video2---- %d", info.thread_start);
		}
		server_release_2();
	    /********message body********/
		message_t msg;
		msg_init(&msg);
		msg.message = MSG_MANAGER_EXIT_ACK;
		msg.sender = SERVER_VIDEO2;
		manager_message(&msg);
		/***************************/
	}
	log_qcy(DEBUG_SERIOUS, "-----------thread exit: server_video2-----------");
	pthread_exit(0);
}

/*
 * internal interface
 */

/*
 * external interface
 */
int server_video2_start(void)
{
	int ret=-1;
	ret = pthread_create(&info.id, NULL, server_func, NULL);
	if(ret != 0) {
		log_qcy(DEBUG_SERIOUS, "video2 server create error! ret = %d",ret);
		 return ret;
	 }
	else {
		log_qcy(DEBUG_SERIOUS, "video2 server create successful!");
		return 0;
	}
}

int server_video2_message(message_t *msg)
{
	int ret=0,ret1;
	if( !message.init ) {
		log_qcy(DEBUG_SERIOUS, "video2 server is not ready for message processing!");
		return -1;
	}
	ret = pthread_rwlock_wrlock(&message.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add message lock fail, ret = %d\n", ret);
		return ret;
	}
	ret = msg_buffer_push(&message, msg);
	log_qcy(DEBUG_SERIOUS, "push into the video2 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg->sender, msg->message, ret,
			message.head, message.tail);
	if( ret!=0 )
		log_qcy(DEBUG_SERIOUS, "message push in video2 error =%d", ret);
	ret1 = pthread_rwlock_unlock(&message.lock);
	if (ret1)
		log_qcy(DEBUG_SERIOUS, "add message unlock fail, ret = %d\n", ret1);
	return ret;
}
