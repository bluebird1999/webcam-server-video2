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
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
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
#include "config.h"
#include "video2_interface.h"
/*
 * static
 */
//variable
static 	message_buffer_t	message;
static 	server_info_t 		info;
static	video_stream_t		stream={-1,-1,-1,-1};
static	video2_config_t		config;
static 	av_buffer_t			v2buffer;
static  pthread_rwlock_t	ilock = PTHREAD_RWLOCK_INITIALIZER;
static	pthread_rwlock_t	vlock = PTHREAD_RWLOCK_INITIALIZER;
static	pthread_mutex_t		mutex = PTHREAD_MUTEX_INITIALIZER;
static	pthread_cond_t		cond = PTHREAD_COND_INITIALIZER;
static 	miss_session_t		*session[MAX_SESSION_NUMBER];

//function
//common
static void *server_func(void);
static int server_message_proc(void);
static void server_release_1(void);
static void server_release_2(void);
static void server_release_3(void);
static int server_restart(void);
static void task_default(void);
static void task_start(void);
static void task_stop(void);
static void task_control(void);
static void task_control_ext(void);
static void task_exit(void);
static int server_set_status(int type, int st, int value);
static void server_thread_termination(int sign);
//specific
static int write_video2_buffer(av_packet_t *data, int id, int target, int channel);
static void write_video2_info(struct rts_av_buffer *data, av_data_info_t	*info);
static int *video2_osd_func(void *arg);
static int stream_init(void);
static int stream_destroy(void);
static int stream_start(void);
static int stream_stop(void);
static int video2_init(void);
/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
 */
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
	ret = manager_common_send_message( msg->receiver, &send_msg);
	return ret;
}

static int video2_set_property(message_t *msg)
{
	int ret= 0, mode = -1;
	message_t send_msg;
	int temp;
    /********message body********/
	msg_init(&send_msg);
	memcpy(&(send_msg.arg_pass), &(msg->arg_pass),sizeof(message_arg_t));
	send_msg.message = msg->message | 0x1000;
	send_msg.sender = send_msg.receiver = SERVER_VIDEO2;
	send_msg.arg_in.cat = msg->arg_in.cat;
	send_msg.arg_in.wolf = 0;
	/****************************/
	/***************************/
	send_msg.result = ret;
	ret = manager_common_send_message(msg->receiver, &send_msg);
	/***************************/
	return ret;
}

static int video2_qos_check(av_qos_t *qos)
{
	double ratio = 0.0;
	int ret = REALTEK_OQS_NORMAL;
	if( (qos->buffer_overrun + qos->buffer_success) < AV_BUFFER_MIN_SAMPLE)
		return ret;
	qos->buffer_total = qos->buffer_overrun + qos->buffer_success;
	qos->buffer_ratio = (double) ((double)qos->buffer_success / (double)qos->buffer_total );
	if( qos->buffer_ratio > AV_BUFFER_SUCCESS) {
		ret = REALTEK_QOS_UPGRADE;
	}
	else if( qos->buffer_ratio < REALTEK_QOS_DOWNGRADE ) {
		ret = REALTEK_QOS_DOWNGRADE;
	}
	if( qos->buffer_total > AV_BUFFER_MAX_SAMPLE) {
		qos->buffer_overrun = 0;
		qos->buffer_success = 0;
		qos->buffer_total = 0;
	}
	return ret;
}

static int video2_quality_downgrade(av_qos_t *qos)
{
	message_t msg;
	int ret = 0;
	int vq = -1;
	if( config.profile.quality > AUTO_PROFILE_START )
		vq = config.profile.quality - 1;
	else
		return 0;
    /********message body********/
	msg_init(&msg);
	msg.message = MSG_VIDEO2_PROPERTY_SET_EXT;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.arg_in.cat = VIDEO2_PROPERTY_QUALITY;
	msg.arg = &vq;
	msg.arg_size = sizeof(vq);
	ret = server_video2_message(&msg);
	/****************************/
	log_qcy(DEBUG_INFO, "-----------------sample number = %d, success ratio = %f, will downgrade!", qos->buffer_total, qos->buffer_ratio);
	log_qcy(DEBUG_INFO, "------------------downdgrade current video qulity to %d", vq);
	return ret;
}

static int video2_quality_upgrade(av_qos_t *qos)
{
	message_t msg;
	int ret = 0;
	int vq = -1;
	if( config.profile.quality < (AUTO_PROFILE_START + AUTO_PROFILE_NUM - 1) )
		vq = config.profile.quality + 1;
	else
		return 0;
    /********message body********/
	msg_init(&msg);
	msg.message = MSG_VIDEO2_PROPERTY_SET_EXT;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.arg_in.cat = VIDEO2_PROPERTY_QUALITY;
	msg.arg = &vq;
	msg.arg_size = sizeof(vq);
	ret = server_video2_message(&msg);
	/****************************/
	log_qcy(DEBUG_INFO, "-----------------sample number = %d, success ratio = %f, will upgrade!", qos->buffer_total, qos->buffer_ratio);
	log_qcy(DEBUG_INFO, "------------------upgrade current video qulity to %d", vq);
	return ret;
}

static int video2_quit_send(int server, int channel)
{
	int ret = 0;
	message_t msg;
	msg_init(&msg);
	msg.sender = msg.receiver = server;
	msg.arg_in.wolf = channel;
	msg.message = MSG_VIDEO2_STOP;
	manager_common_send_message(SERVER_VIDEO2, &msg);
	return ret;
}

static int *video2_osd_func(void *arg)
{
	int ret=0, st;
	video2_osd_config_t ctrl;
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGBUS,  signal_handler);
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
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_OSD2, 1 );
    manager_common_send_dummy(SERVER_VIDEO2);
    while( 1 ) {
    	if( info.exit ) break;
    	if( misc_get_bit(info.thread_exit, THREAD_OSD2) ) break;
    	st = info.status;
    	if( (st != STATUS_START) && (st != STATUS_RUN) )
    		break;
    	else if( st == STATUS_START )
    		continue;
   		ret = video2_osd_proc(&ctrl);
    	usleep(1000*500);
    }
    //release
exit:
    video2_osd_release();
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_OSD2, 0 );
    manager_common_send_dummy(SERVER_VIDEO2);
    log_qcy(DEBUG_INFO, "-----------thread exit: server_video2_osd-----------");
    pthread_exit(0);
}

static int *video2_main_func(void* arg)
{
	int ret=0, st, i;
	video_stream_t ctrl;
	av_packet_t	*packet = NULL;
	av_qos_t qos;
	struct rts_av_buffer *buffer = NULL;
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGBUS,  signal_handler);
    misc_set_thread_name("server_video2_main");
    pthread_detach(pthread_self());
    //init
    memset( &qos, 0, sizeof(qos));
    memcpy( &ctrl,(video_stream_t*)arg, sizeof(video_stream_t));
    av_buffer_init(&v2buffer, &vlock);
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_VIDEO2, 1 );
    manager_common_send_dummy(SERVER_VIDEO2);
    while( 1 ) {
    	if( info.exit ) break;
    	st = info.status;
    	if( st != STATUS_START && st != STATUS_RUN )
    		break;
    	else if( st == STATUS_START )
    		continue;
    	usleep(1000);
    	ret = rts_av_poll(ctrl.h264);
    	if ( ret )
    		continue;
    	ret = rts_av_recv(ctrl.h264, &buffer);
    	if ( ret ) {
    		if( buffer )
    			rts_av_put_buffer(buffer);
    		continue;
    	}
    	if ( buffer ) {
        	packet = av_buffer_get_empty(&v2buffer, &qos.buffer_overrun, &qos.buffer_success);
        	if( (buffer->bytesused > 200*1024) ) {
    			log_qcy(DEBUG_WARNING, "realtek video2 frame size=%d!!!!!!", buffer->bytesused);
    			rts_av_put_buffer(buffer);
    			continue;
        	}
        	if( misc_mips_address_check((unsigned int)buffer->vm_addr) ) {
    			log_qcy(DEBUG_WARNING, "realtek video2 memory address anomity =%p!!!!!!", buffer->vm_addr);
    			rts_av_put_buffer(buffer);
    			continue;
        	}
    		packet->data = malloc( buffer->bytesused );
    		if( packet->data == NULL) {
    			log_qcy(DEBUG_WARNING, "allocate memory failed in video buffer, size=%d", buffer->bytesused);
    			rts_av_put_buffer(buffer);
    			continue;
    		}
    		memcpy(packet->data, buffer->vm_addr, buffer->bytesused);
    		if( (stream.realtek_stamp == 0) && (stream.unix_stamp == 0) ) {
    			stream.realtek_stamp = buffer->timestamp;
    			stream.unix_stamp = time_get_now_stamp();
    		}
    		write_video2_info( buffer, &packet->info);
    		rts_av_put_buffer(buffer);
    		for(i=0;i<MAX_SESSION_NUMBER;i++) {
				if( misc_get_bit(info.status2, RUN_MODE_MISS2+i) ) {
					ret = write_video2_buffer(packet, MSG_MISS_VIDEO_DATA, SERVER_MISS, i);
					if( (ret == MISS_LOCAL_ERR_MISS_GONE) || (ret == MISS_LOCAL_ERR_SESSION_GONE) ) {
						log_qcy(DEBUG_WARNING, "Miss video ring buffer send failed due to non-existing miss server or session");
						video2_quit_send(SERVER_MISS, i);
						log_qcy(DEBUG_WARNING, "----shut down video miss stream due to session lost!------");
					}
					else if( ret == MISS_LOCAL_ERR_AV_NOT_RUN) {
						qos.failed_send[RUN_MODE_MISS2+i]++;
						if( qos.failed_send[RUN_MODE_MISS2+i] > VIDEO_MAX_FAILED_SEND) {
							qos.failed_send[RUN_MODE_MISS2+i] = 0;
							video2_quit_send(SERVER_MISS, i);
							log_qcy(DEBUG_WARNING, "----shut down video miss stream due to long overrun!------");
						}
					}
					else if( ret == 0) {
						av_packet_add(packet);
						qos.failed_send[RUN_MODE_MISS2+i] = 0;
					}
				}
    		}
/*    		if( config.profile.quality >= AUTO_PROFILE_START ) { //auto mode
				if( video2_qos_check(&qos) == REALTEK_QOS_UPGRADE )
					video2_quality_upgrade(&qos);
				else if( video2_qos_check(&qos) == REALTEK_QOS_DOWNGRADE )
					video2_quality_downgrade(&qos);
    		}
*/
			av_packet_check(packet);
    	}
    }
    //release
exit:
	av_buffer_release(&v2buffer);
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_VIDEO2, 0 );
    manager_common_send_dummy(SERVER_VIDEO2);
    log_qcy(DEBUG_INFO, "-----------thread exit: server_video2_main-----------");
    pthread_exit(0);
}

static int stream_init(void)
{
	stream.isp = -1;
	stream.h264 = -1;
	stream.jpg = -1;
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
	if (stream.jpg >= 0) {
		rts_av_destroy_chn(stream.jpg);
		stream.jpg = -1;
	}
	return ret;
}

static int stream_start(void)
{
	int ret=0;
	pthread_t osd_id, main_id;
	config.profile.profile[config.profile.quality].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	ret = rts_av_set_profile(stream.isp, &config.profile.profile[config.profile.quality]);
	info.tick = 0;
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
	if( config.osd.enable && stream.osd != -1 ) {
		//start the osd thread
		ret = pthread_create(&osd_id, NULL, video2_osd_func, (void*)&config.osd);
		if(ret != 0) {
			log_qcy(DEBUG_SERIOUS, "video2 osd thread create error! ret = %d",ret);
		 }
		else {
			misc_set_bit(&info.tick, THREAD_OSD2, 1);
			log_qcy(DEBUG_INFO, "video2 osd thread create successful!");
		}
	}
	ret = pthread_create(&main_id, NULL, video2_main_func, (void*)&stream);
	if(ret != 0) {
		log_qcy(DEBUG_INFO, "video2 main thread create error! ret = %d",ret);
		return -1;
	 }
	else {
		misc_set_bit(&info.tick, THREAD_VIDEO2, 1);
		log_qcy(DEBUG_SERIOUS, "video2 main thread create successful!");
	}
    return 0;
}

static int stream_stop(void)
{
	int ret=0;
	if(stream.h264!=-1)
		ret = rts_av_stop_recv(stream.h264);
	if(stream.jpg!=-1)
		ret = rts_av_disable_chn(stream.jpg);
	if( stream.osd != -1 )
		ret = rts_av_disable_chn(stream.osd);
	if(stream.h264!=-1)
		ret = rts_av_disable_chn(stream.h264);
	if(stream.isp!=-1)
		ret = rts_av_disable_chn(stream.isp);
	stream.frame = 0;
	stream.realtek_stamp = 0;
	stream.unix_stamp = 0;
	return ret;
}

static void video2_init_profile(void)
{
	int id;
	if( config.profile.quality == 0 )
		config.profile.quality = 3;		//middle range auto-quality
	id = config.profile.quality;
	config.h264.h264_attr.bps = config.h264.h264_bitrate[id];
	config.h264.h264_attr.gop = config.h264.h264_gop[id];
	config.h264.h264_attr.level = config.h264.h264_level[id];
	config.h264.h264_attr.qp = config.h264.h264_qp[id];
}

static int video2_init(void)
{
	int ret;
	stream_init();
	stream.isp = rts_av_create_isp_chn(&config.isp);
	if (stream.isp < 0) {
		log_qcy(DEBUG_SERIOUS, "fail to create isp chn, ret = %d", stream.isp);
		return -1;
	}
	log_qcy(DEBUG_INFO, "isp chnno:%d", stream.isp);
	video2_init_profile();
	config.h264.h264_attr.gop = 2 * config.profile.profile[config.profile.quality].video.denominator;
	stream.h264 = rts_av_create_h264_chn(&config.h264.h264_attr);
	if (stream.h264 < 0) {
		log_qcy(DEBUG_SERIOUS, "fail to create h264 chn, ret = %d", stream.h264);
		return -1;
	}
	log_qcy(DEBUG_INFO, "h264 chnno:%d", stream.h264);
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
        log_qcy(DEBUG_INFO, "osd chnno:%d", stream.osd);
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
	return 0;
}

static void write_video2_info(struct rts_av_buffer *data, av_data_info_t	*info)
{
	info->flag = data->flags;
	info->frame_index = data->frame_idx;
//	info->timestamp = data->timestamp / 1000;
	info->timestamp = ( ( data->timestamp - stream.realtek_stamp ) / 1000) + stream.unix_stamp * 1000;
	info->fps = config.profile.profile[config.profile.quality].video.denominator;
	info->width = config.profile.profile[config.profile.quality].video.width;
	info->height = config.profile.profile[config.profile.quality].video.height;
   	info->flag |= FLAG_STREAM_TYPE_LIVE << 11;
   	if(config.osd.enable)
   		info->flag |= FLAG_WATERMARK_TIMESTAMP_EXIST << 13;
   	else
   		info->flag |= FLAG_WATERMARK_TIMESTAMP_NOT_EXIST << 13;
   	info->flag &= ~(0xF);
    if( misc_get_bit(data->flags, 0/*RTSTREAM_PKT_FLAG_KEY*/) )// I frame
    	info->flag |= FLAG_FRAME_TYPE_IFRAME << 0;
    else
    	info->flag |= FLAG_FRAME_TYPE_PBFRAME << 0;
    if( (config.profile.quality==1) || (config.profile.quality==3) || (config.profile.quality==4) )
        info->flag |= FLAG_RESOLUTION_VIDEO_360P << 17;
    else if( (config.profile.quality==5) || (config.profile.quality==6))
        info->flag |= FLAG_RESOLUTION_VIDEO_480P << 17;
    else if( (config.profile.quality==2) || (config.profile.quality==7) || (config.profile.quality==8) )
        info->flag |= FLAG_RESOLUTION_VIDEO_1080P << 17;
    info->size = data->bytesused;
}

static int write_video2_buffer(av_packet_t *data, int id, int target, int channel)
{
	int ret=0;
	message_t msg;
    /********message body********/
	msg_init(&msg);
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.arg_in.wolf = channel;
	msg.arg_in.handler = session[channel];
	msg.message = id;
	msg.arg = data;
	msg.arg_size = 0;	//make sure this is 0 for non-deep-copy
	msg.extra_size = 0;
	if( target == SERVER_MISS )
		ret = server_miss_video_message(&msg);
	/****************************/
	return ret;
}

static int video2_add_session(miss_session_t *ses, int sid)
{
	session[sid] = ses;
	return 0;
}

static int video2_remove_session(miss_session_t *ses, int sid)
{
	session[sid] = NULL;
	return 0;
}

static int server_set_status(int type, int st, int value)
{
	int ret=-1;
	pthread_rwlock_wrlock(&ilock);
	if(type == STATUS_TYPE_STATUS)
		info.status = st;
	else if(type==STATUS_TYPE_EXIT)
		info.exit = st;
	else if(type==STATUS_TYPE_CONFIG)
		config.status = st;
	else if(type==STATUS_TYPE_THREAD_START)
		misc_set_bit(&info.thread_start, st, value);
	pthread_rwlock_unlock(&ilock);
	return ret;
}

static void server_thread_termination(int sign)
{
    /********message body********/
	message_t msg;
	msg_init(&msg);
	msg.message = MSG_VIDEO2_SIGINT;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	manager_common_send_message(SERVER_MANAGER, &msg);
	/****************************/
}

static void video2_broadcast_thread_exit(void)
{
}

static void server_release_1(void)
{
	message_t msg;
	stream_stop();
	stream_destroy();
	usleep(1000*10);
}

static void server_release_2(void)
{
	msg_buffer_release2(&message, &mutex);
	memset(&config,0,sizeof(video2_config_t));
	memset(&stream,0,sizeof(video_stream_t));
	video2_osd_font_release();
}

static void server_release_3(void)
{
	msg_free(&info.task.msg);
	memset(&info, 0, sizeof(server_info_t));
}

/*
 *
 */
static int video2_message_filter(message_t  *msg)
{
	int ret = 0;
	if( info.task.func == task_exit) { //only system message
		if( !msg_is_system(msg->message) && !msg_is_response(msg->message) )
			return 1;
	}
	return ret;
}

static int server_message_proc(void)
{
	int ret = 0, ret1 = 0;
	message_t msg;
//condition
	pthread_mutex_lock(&mutex);
	if( message.head == message.tail ) {
		if( info.status == info.old_status	)
			pthread_cond_wait(&cond,&mutex);
	}
	if( info.msg_lock ) {
		pthread_mutex_unlock(&mutex);
		return 0;
	}
	msg_init(&msg);
	ret = msg_buffer_pop(&message, &msg);
	pthread_mutex_unlock(&mutex);
	if( ret == 1)
		return 0;
	if( video2_message_filter(&msg) ) {
		msg_free(&msg);
		log_qcy(DEBUG_VERBOSE, "VIDEO2 message--- sender=%d, message=%x, ret=%d, head=%d, tail=%d was screened, the current task is %p", msg.sender, msg.message,
				ret, message.head, message.tail, info.task.func);
		return -1;
	}
	log_qcy(DEBUG_VERBOSE, "-----pop out from the VIDEO2 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg.sender, msg.message,
			ret, message.head, message.tail);
	switch(msg.message) {
		case MSG_VIDEO2_START:
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.func = task_start;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_STOP:
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.msg.arg_in.cat = info.status2;
			if( msg.sender == SERVER_MISS) misc_set_bit(&info.task.msg.arg_in.cat, (RUN_MODE_MISS2 + msg.arg_in.wolf), 0);
			info.task.func = task_stop;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_PROPERTY_SET:
			msg_init(&info.task.msg);
			msg_deep_copy(&info.task.msg, &msg);
			info.task.func = task_control;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_PROPERTY_SET_EXT:
			msg_init(&info.task.msg);
			msg_deep_copy(&info.task.msg, &msg);
			info.task.func = task_control_ext;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_PROPERTY_SET_DIRECT:
			video2_set_property(&msg);
			break;
		case MSG_VIDEO2_PROPERTY_GET:
			ret = video2_get_property(&msg);
			break;
		case MSG_MANAGER_EXIT:
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.func = task_exit;
			info.status = EXIT_INIT;
			info.msg_lock = 0;
			break;
		case MSG_MANAGER_TIMER_ACK:
			((HANDLER)msg.arg_in.handler)();
			break;
		case MSG_MIIO_PROPERTY_NOTIFY:
		case MSG_MIIO_PROPERTY_GET_ACK:
			if( msg.arg_in.cat == MIIO_PROPERTY_TIME_SYNC ) {
				if( msg.arg_in.dog == 1 )
					misc_set_bit( &info.init_status, VIDEO2_INIT_CONDITION_MIIO_TIME, 1);
			}
			break;
		case MSG_REALTEK_PROPERTY_NOTIFY:
		case MSG_REALTEK_PROPERTY_GET_ACK:
			if( msg.arg_in.cat == REALTEK_PROPERTY_AV_STATUS ) {
				if( msg.arg_in.dog == 1 )
					misc_set_bit(&info.init_status, VIDEO2_INIT_CONDITION_REALTEK, 1);
			}
			break;
		case MSG_MANAGER_EXIT_ACK:
			misc_set_bit(&info.error, msg.sender, 0);
			break;
		case MSG_MANAGER_DUMMY:
			break;
		case MSG_MISS_BUFFER_FULL:
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "not processed message = %x", msg.message);
			break;
	}
	msg_free(&msg);
	return ret;
}

/*
 *
 */
static int server_none(void)
{
	int ret = 0;
	message_t msg;
	if( !misc_get_bit( info.init_status, VIDEO2_INIT_CONDITION_CONFIG ) ) {
		ret = video2_config_video_read(&config);
		if( !ret && misc_full_bit( config.status, CONFIG_VIDEO2_MODULE_NUM) ) {
			misc_set_bit(&info.init_status, VIDEO2_INIT_CONDITION_CONFIG, 1);
		}
		else {
			info.status = STATUS_ERROR;
			return -1;
		}
	}
	if( !misc_get_bit( info.init_status, VIDEO2_INIT_CONDITION_REALTEK ) ) {
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_REALTEK_PROPERTY_GET;
		msg.sender = msg.receiver = SERVER_VIDEO2;
		msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
		manager_common_send_message(SERVER_REALTEK,    &msg);
		/****************************/
		usleep(MESSAGE_RESENT_SLEEP);
	}
	if( !misc_get_bit( info.init_status, VIDEO2_INIT_CONDITION_MIIO_TIME)) {
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_MIIO_PROPERTY_GET;
		msg.sender = msg.receiver = SERVER_VIDEO2;
		msg.arg_in.cat = MIIO_PROPERTY_TIME_SYNC;
		ret = manager_common_send_message(SERVER_MIIO, &msg);
		/***************************/
		usleep(MESSAGE_RESENT_SLEEP);
	}
	if( misc_full_bit( info.init_status, VIDEO2_INIT_CONDITION_NUM ) ) {
		info.status = STATUS_WAIT;
		video2_osd_font_init(&config.osd);
	}
	return ret;
}

static int server_setup(void)
{
	int ret = 0;
	if( video2_init() == 0) {
		info.status = STATUS_IDLE;
	}
	else
		info.status = STATUS_ERROR;
	return ret;
}

static int server_start(void)
{
	int ret = 0;
	if( stream_start()==0 )
		info.status = STATUS_RUN;
	else
		info.status = STATUS_ERROR;
	return ret;
}

static int server_stop(void)
{
	int ret = 0;
	if( stream_stop()==0 )
		info.status = STATUS_IDLE;
	else
		info.status = STATUS_ERROR;
	return ret;
}

static int server_restart(void)
{
	int ret = 0;
	stream_stop();
	stream_destroy();
	return ret;
}
/*
 * task
 */
/*
 * task control: restart->wait->change->setup->start->run
 */
static void task_control_ext(void)
{
	static int para_set = 0;
	int temp = 0;
	message_t msg;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	msg.arg_in.wolf = 0;
	/***************************/
	switch(info.status){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			if( info.thread_start == 0 ) {
				if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_TIME_WATERMARK ) {
					temp = *((int*)(info.task.msg.arg));
					if( temp != config.osd.enable) {
						config.osd.enable = temp;
						log_qcy(DEBUG_INFO, "changed the osd2 switch = %d", config.osd.enable);
						video2_config_video_set(CONFIG_VIDEO2_OSD,  &config.osd);
						msg.arg_in.wolf = 1;
						msg.arg = &temp;
						msg.arg_size = sizeof(temp);
					}
				}
				else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_QUALITY ) {
					temp = *((int*)(info.task.msg.arg));
					if( temp == 0 )
						temp = 3;	//choose a middle range quality for auto mode
					config.profile.quality = temp;
					log_qcy(DEBUG_INFO, "changed the video quality = %d", config.profile.quality);
					video2_config_video_set(CONFIG_VIDEO2_PROFILE, &config.profile);
					msg.arg_in.wolf = 1;
					msg.arg = &temp;
					msg.arg_size = sizeof(temp);
				}
				para_set = 1;
				if( info.task.start == STATUS_WAIT )
					goto exit;
				else
					info.status = STATUS_SETUP;
			}
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			if( !para_set )
				info.status = STATUS_RESTART;
			else {
				if( info.task.start == STATUS_IDLE )
					goto exit;
				else
					info.status = STATUS_START;
			}
			break;
		case STATUS_START:
			server_start();
		case STATUS_RUN:
			if( !para_set ) {
				if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_TIME_WATERMARK ) {
					temp = *((int*)(info.task.msg.arg));
					if( temp == config.osd.enable ) {
						msg.arg_in.wolf = 0;
						msg.arg = &temp;
						msg.arg_size = sizeof(temp);
						goto exit;
					}
				}
				else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_QUALITY ) {
					temp = *((int*)(info.task.msg.arg));
					if( ( (temp == 0) && (config.profile.quality >= AUTO_PROFILE_START) ) ||
						( temp == config.profile.quality) ) {	//not change if quality is the same
						msg.arg_in.wolf = 0;
						msg.arg = &temp;
						msg.arg_size = sizeof(temp);
						goto exit;
					}
				}
				info.status = STATUS_RESTART;
			}
			else {
				if( misc_get_bit( info.thread_start, THREAD_VIDEO2 ) )
					goto exit;
			}
			break;
		case STATUS_RESTART:
			server_restart();
			info.status = STATUS_WAIT;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			goto exit;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_control_exit = %d", info.status);
			break;
	}
	return;
exit:
	if( info.task.msg.arg_in.cat != VIDEO2_PROPERTY_TIME_WATERMARK)	//not reply watermark setting for miio response integrity
		manager_common_send_message(info.task.msg.receiver, &msg);
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
	int temp = 0;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	msg.arg_in.cat = 0;
	/***************************/
	switch(info.status){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			info.status = STATUS_WAIT;
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			if( info.thread_start == 0) {
				if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_QUALITY ) {
					temp = *((int*)(info.task.msg.arg));
					if( temp == 0 )
						temp = 3;	//choose a middle range quality for auto mode
					config.profile.quality = temp;
					log_qcy(DEBUG_INFO, "changed the quality = %d", config.profile.quality);
					video2_config_video_set(CONFIG_VIDEO2_PROFILE, &config.profile);
					msg.arg_in.wolf = 1;
					msg.arg = &temp;
					msg.arg_size = sizeof(temp);
				}
				para_set = 1;
				if( info.task.start == STATUS_IDLE )
					goto exit;
				else
					info.status = STATUS_START;
			}
			break;
		case STATUS_START:
			server_start();
			break;
		case STATUS_RUN:
			if( !para_set ) {
				if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_QUALITY ) {
					temp = *((int*)(info.task.msg.arg));
					if( ( (temp == 0) && (config.profile.quality >= AUTO_PROFILE_START) ) ||
						( temp == config.profile.quality) ) {	//not change if quality is the same
						msg.arg_in.wolf = 0;
						msg.arg = &temp;
						msg.arg_size = sizeof(temp);
						goto exit;
					}
				}
				info.status = STATUS_STOP;
			}
			else {
				if( misc_get_bit( info.thread_start, THREAD_VIDEO2 ) )
					goto exit;
			}
			break;
		case STATUS_STOP:
			server_stop();
			break;
		case STATUS_ERROR:
			msg.result = -1;
			goto exit;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_control = %d", info.status);
			break;
	}
	return;
exit:
	manager_common_send_message( info.task.msg.receiver, &msg);
	msg_free(&info.task.msg);
	para_set = 0;
	info.task.func = task_default;
	info.msg_lock = 0;
	return;
}
/*
 * task start: idle->start
 */
static void task_start(void)
{
	message_t msg;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			info.status = STATUS_START;
			break;
		case STATUS_START:
			server_start();
			break;
		case STATUS_RUN:
			if( misc_get_bit( info.thread_start, THREAD_VIDEO2 ) )
				goto exit;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			goto exit;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_start = %d", info.status);
			break;
	}
	return;
exit:
	if( msg.result == 0 ) {
		if( info.task.msg.sender == SERVER_MISS ) {
			video2_add_session(info.task.msg.arg_in.handler, info.task.msg.arg_in.wolf);
		}
		if( info.task.msg.sender == SERVER_MISS ) misc_set_bit(&info.status2, (RUN_MODE_MISS2 + info.task.msg.arg_in.wolf), 1);
	}
	manager_common_send_message(info.task.msg.receiver, &msg);
	msg_free(&info.task.msg);
	info.task.func = &task_default;
	info.msg_lock = 0;
	return;
}
/*
 * task start: run->stop->idle
 */
static void task_stop(void)
{
	message_t msg;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_NONE:
		case STATUS_WAIT:
		case STATUS_SETUP:
			goto exit;
			break;
		case STATUS_IDLE:
			if( info.thread_start == 0 )
				goto exit;
			break;
		case STATUS_START:
		case STATUS_RUN:
			if( info.task.msg.arg_in.cat > 0 ) {
				goto exit;
				break;
			}
			else
				server_stop();
			break;
		case STATUS_ERROR:
			msg.result = -1;
			goto exit;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_stop = %d", info.status);
			break;
	}
	return;
exit:
	if( msg.result==0 ) {
		if( info.task.msg.sender == SERVER_MISS ) {
			video2_remove_session(info.task.msg.arg_in.handler, info.task.msg.arg_in.wolf);
		}
		if( info.task.msg.sender == SERVER_MISS) misc_set_bit(&info.status2, (RUN_MODE_MISS2 + info.task.msg.arg_in.wolf), 0);
	}
	manager_common_send_message(info.task.msg.receiver, &msg);
	msg_free(&info.task.msg);
	info.task.func = &task_default;
	info.msg_lock = 0;
	return;
}
/*
 * task
 */
/*
 * default exit: *->exit
 */
static void task_exit(void)
{
	switch( info.status ){
		case EXIT_INIT:
			log_qcy(DEBUG_INFO,"VIDEO2: switch to exit task!");
			if( info.task.msg.sender == SERVER_MANAGER) {
				info.error = VIDEO2_EXIT_CONDITION;
				info.error &= (info.task.msg.arg_in.cat);
			}
			else {
				info.error = 0;
			}
			info.status = EXIT_SERVER;
			break;
		case EXIT_SERVER:
			if( !info.error )
				info.status = EXIT_STAGE1;
			break;
		case EXIT_STAGE1:
			server_release_1();
			info.status = EXIT_THREAD;
			break;
		case EXIT_THREAD:
			info.thread_exit = info.thread_start;
			video2_broadcast_thread_exit();
			if( !info.thread_start )
				info.status = EXIT_STAGE2;
			break;
			break;
		case EXIT_STAGE2:
			server_release_2();
			info.status = EXIT_FINISH;
			break;
		case EXIT_FINISH:
			info.exit = 1;
		    /********message body********/
			message_t msg;
			msg_init(&msg);
			msg.message = MSG_MANAGER_EXIT_ACK;
			msg.sender = SERVER_VIDEO2;
			manager_common_send_message(SERVER_MANAGER, &msg);
			/***************************/
			info.status = STATUS_NONE;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_exit = %d", info.status);
			break;
		}
	return;
}

/*
 * default task: none->run
 */
static void task_default(void)
{
	switch( info.status ){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			break;
		case STATUS_START:
			break;
		case STATUS_RUN:
			break;
		case STATUS_ERROR:
			info.task.func = task_exit;
			info.status = EXIT_INIT;
			info.msg_lock = 0;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_default = %d", info.status);
			break;
		}
	return;
}

/*
 * server entry point
 */
static void *server_func(void)
{
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGBUS,  signal_handler);
	misc_set_thread_name("server_video2");
	pthread_detach(pthread_self());
	msg_buffer_init2(&message, MSG_BUFFER_OVERFLOW_NO, &mutex);
	info.init = 1;
	//default task
	info.task.func = task_default;
	while( !info.exit ) {
		info.old_status = info.status;
		info.task.func();
		server_message_proc();
	}
	server_release_3();
	log_qcy(DEBUG_INFO, "-----------thread exit: server_video2-----------");
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
		log_qcy(DEBUG_INFO, "video2 server create successful!");
		return 0;
	}
}

int server_video2_message(message_t *msg)
{
	int ret=0;
	pthread_mutex_lock(&mutex);
	if( !message.init ) {
		log_qcy(DEBUG_SERIOUS, "video2 server is not ready for message processing!");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	ret = msg_buffer_push(&message, msg);
	log_qcy(DEBUG_VERBOSE, "push into the video2 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg->sender, msg->message, ret,
			message.head, message.tail);
	if( ret!=0 )
		log_qcy(DEBUG_WARNING, "message push in video2 error =%d", ret);
	else {
		pthread_cond_signal(&cond);
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}
