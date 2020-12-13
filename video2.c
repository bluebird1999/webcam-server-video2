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
//#include "../../server/micloud/micloud_interface.h"
//server header
#include "video2.h"
#include "video2_interface.h"
#include "video2.h"
#include "white_balance.h"
#include "focus.h"
#include "exposure.h"
#include "isp.h"
#include "config.h"
#include "md.h"
#include "jpeg.h"

/*
 * static
 */
//variable
static 	message_buffer_t	message;
static 	server_info_t 		info;
static	video_stream_t		stream={-1,-1,-1,-1};
static	video2_config_t		config;
static 	av_buffer_t			v2buffer;
static	video2_md_run_t		md_run;
static  pthread_rwlock_t	ilock = PTHREAD_MUTEX_INITIALIZER;
static	pthread_rwlock_t	vlock = PTHREAD_RWLOCK_INITIALIZER;
static	pthread_mutex_t		mutex = PTHREAD_MUTEX_INITIALIZER;
static	pthread_cond_t		cond = PTHREAD_COND_INITIALIZER;

//function
//common
static void *server_func(void);
static int server_message_proc(void);
static void server_release_1(void);
static void server_release_2(void);
static void server_release_3(void);
static void task_default(void);
static void task_start(void);
static void task_stop(void);
static void task_exit(void);
static int server_set_status(int type, int st, int value);
static void server_thread_termination(int sign);
//specific
static int write_video2_buffer(av_packet_t *data, int id, int target, int type);
static void write_video2_info(struct rts_av_buffer *data, av_data_info_t *info);
static int *video2_3acontrol_func(void *arg);
static int *video2_osd_func(void *arg);
static int stream_init(void);
static int stream_destroy(void);
static int stream_start(void);
static int stream_stop(void);
static int video2_init(void);
static int md_init_scheduler(void);
static int *video2_md_func(void *arg);
/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
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
	if( send_msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_SWITCH) {
		send_msg.arg = (void*)(&config.md.enable);
		send_msg.arg_size = sizeof(config.md.enable);
	}
	else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_ALARM_INTERVAL) {
		send_msg.arg = (void*)(&config.md.alarm_interval);
		send_msg.arg_size = sizeof(config.md.alarm_interval);
	}
	else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_SENSITIVITY) {
		send_msg.arg = (void*)(&config.md.sensitivity);
		send_msg.arg_size = sizeof(config.md.sensitivity);
	}
	else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_START) {
		send_msg.arg = (void*)(config.md.start);
		send_msg.arg_size = strlen(config.md.start) + 1;
	}
	else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_END) {
		send_msg.arg = (void*)(config.md.end);
		send_msg.arg_size = strlen(config.md.end) + 1;
	}
	else if( send_msg.arg_in.cat == VIDEO2_PROPERTY_CUSTOM_WARNING_PUSH) {
		send_msg.arg = (void*)(&config.md.cloud_report);
		send_msg.arg_size = sizeof(config.md.cloud_report);
	}
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
	if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_SWITCH ) {
		temp = *((int*)(info.task.msg.arg));
		if( temp != config.md.enable ) {
			config.md.enable = temp;
			log_qcy(DEBUG_INFO, "changed the motion switch = %d", config.md.enable);
			video_config_video_set(CONFIG_VIDEO2_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
		}
	}
	else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_ALARM_INTERVAL ) {
		temp = *((int*)(info.task.msg.arg));
		if( temp != config.md.alarm_interval ) {
			config.md.alarm_interval = temp;
			log_qcy(DEBUG_INFO, "changed the motion detection alarm interval = %d", config.md.alarm_interval);
			video_config_video_set(CONFIG_VIDEO2_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
			md_run.changed = 1;
		}
	}
	else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_SENSITIVITY ) {
		temp = *((int*)(info.task.msg.arg));
		if( temp != config.md.sensitivity ) {
			config.md.sensitivity = temp;
			log_qcy(DEBUG_INFO, "changed the motion detection sensitivity = %d", config.md.sensitivity);
			video_config_video_set(CONFIG_VIDEO2_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
			md_run.changed = 1;
		}
	}
	else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_START ) {
		if( strcmp(config.md.start, (char*)(info.task.msg.arg)) ) {
			strcpy( config.md.start, (char*)(info.task.msg.arg) );
			log_qcy(DEBUG_INFO, "changed the motion detection start = %s", config.md.start);
			video_config_video_set(CONFIG_VIDEO2_MD, &config.md);
			md_init_scheduler();
			send_msg.arg_in.wolf = 1;
			send_msg.arg = config.md.start;
			send_msg.arg_size = sizeof(config.md.start);
		}
	}
	else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_MOTION_END ) {
		if( strcmp(config.md.end, (char*)(info.task.msg.arg)) ) {
			strcpy( config.md.end, (char*)(info.task.msg.arg) );
			log_qcy(DEBUG_INFO, "changed the motion detection end = %s", config.md.end);
			video_config_video_set(CONFIG_VIDEO2_MD, &config.md);
			md_init_scheduler();
			send_msg.arg_in.wolf = 1;
			send_msg.arg = config.md.end;
			send_msg.arg_size = sizeof(config.md.end);
		}
	}
	else if( info.task.msg.arg_in.cat == VIDEO2_PROPERTY_CUSTOM_WARNING_PUSH ) {
		temp = *((int*)(info.task.msg.arg));
		if( temp != config.md.cloud_report ) {
			config.md.cloud_report = temp;
			log_qcy(DEBUG_INFO, "changed the motion detection cloud push = %d", config.md.cloud_report);
			video_config_video_set(CONFIG_VIDEO2_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
			md_run.changed = 1;
		}
	}
	/***************************/
	send_msg.result = ret;
	ret = manager_common_send_message(msg->receiver, &send_msg);
	/***************************/
	return ret;
}

static void *video2_jpeg_func(void *arg)
{
	int ret=0;
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    misc_set_thread_name("server_video2_thumb");
    pthread_detach(pthread_self());
    //init
    ret = video2_jpeg_thumbnail((char*)arg, 270, 180);
    //release
    if( arg )
    	free(arg);
    log_qcy(DEBUG_INFO, "-----------thread exit: video2_jpeg_thumbnail-----------");
    pthread_exit(0);
}

static int video2_thumbnail(message_t *msg)
{
    unsigned char* fname = NULL;
    pthread_t tid;
    int ret = 0, size = 0;
    char file[MAX_SYSTEM_STRING_SIZE*4];
    if( msg->arg_in.cat == RECORDER_TYPE_MOTION_DETECTION ) {
    	memset(file, 0, sizeof(file));
    	sprintf(file, "%smotion.jpg", config.jpg.image_path);
    	ret = rename(msg->arg, file);
		if(ret) {
			log_qcy(DEBUG_WARNING, "rename motion snapshot file %s to %s failed.\n", msg->arg, file);
		}
    }
    else {
		fname = malloc( msg->arg_size );
		if( fname == NULL ) {
			log_qcy(DEBUG_WARNING, "jpeg thread memory allocation failed! size = %d",size);
			return -1;
		}
		memset(fname, 0, msg->arg_size);
		strcpy( fname, (char*)msg->arg );
		ret = pthread_create(&tid, NULL, video2_jpeg_func, fname);
		if(ret != 0) {
			log_qcy(DEBUG_WARNING, "jpeg thread create error! ret = %d",ret);
			if( fname )
				free( fname );
		}
    }
	return ret;
}

static void video2_mjpeg_func(void *priv, struct rts_av_profile *profile, struct rts_av_buffer *buffer)
{
    FILE *pfile = NULL;
    pfile = fopen((char*)priv, "wb");
    if (!pfile) {
		log_qcy(DEBUG_WARNING, "open video2 jpg snapshot file %s fail\n", (char*)priv);
		return;
    }
    fwrite(buffer->vm_addr, 1, buffer->bytesused, pfile);
    RTS_SAFE_RELEASE(pfile, fclose);
    return;
}

static int video2_snapshot(message_t *msg)
{
	struct rts_av_callback cb;
    static char filename[4*MAX_SYSTEM_STRING_SIZE];
	int ret = 0;
	cb.func = video2_mjpeg_func;
	cb.start = msg->arg_in.cat;
	cb.times = msg->arg_in.dog;
	cb.interval = msg->arg_in.duck;
	cb.type = msg->arg_in.tiger;
	if( msg->arg_in.chick == RECORDER_TYPE_MOTION_DETECTION ) {
		memset( filename, 0, sizeof(filename) );
		sprintf( filename, "%smotion.jpg", config.jpg.image_path );
	}
	else {
		memset(filename, 0, sizeof(filename));
		strcpy(filename, (char*)msg->arg);
		cb.priv = (void*)filename;
	}
	ret = rts_av_set_callback(stream.jpg, &cb, 0);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "set mjpeg callback fail, ret = %d\n", ret);
		return ret;
	}
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


static int md_init_scheduler(void)
{
	int ret = 0;
	char final[MAX_SYSTEM_STRING_SIZE];
	memset(final, 0, MAX_SYSTEM_STRING_SIZE);
	sprintf(final, "%s-%s", config.md.start, config.md.end);
	ret = video2_md_get_scheduler_time(final, &md_run.scheduler, &md_run.mode);
	return ret;
}

static int md_check_scheduler(void)
{
	int ret;
	message_t msg;
	pthread_t md_id;
	if( config.md.enable ) {
		ret = video2_md_check_scheduler_time(&md_run.scheduler, &md_run.mode);
		if( ret==1 ) {
			if( !md_run.started && !misc_get_bit(info.thread_start, THREAD_MD) ) {
				//start the md thread
				ret = pthread_create(&md_id, NULL, video2_md_func, (void*)&config.md);
				if(ret != 0) {
					log_qcy(DEBUG_SERIOUS, "md thread create error! ret = %d",ret);
					return -1;
				}
				else {
					log_qcy(DEBUG_INFO, "md thread create successful!");
					md_run.started = 1;
				    /********message body********/
					msg_init(&msg);
					msg.message = MSG_VIDEO2_START;
					msg.sender = msg.receiver = SERVER_VIDEO2;
				    manager_common_send_message(SERVER_VIDEO2, &msg);
					/****************************/
				}
			}
			else if( md_run.changed ) {
				md_run.changed = 0;
				goto stop_md;
			}
		}
		else {
			if( md_run.started ) {
				goto stop_md;
			}
		}
	}
	else {
		if( md_run.started ) {
			goto stop_md;
		}
	}
	return ret;
stop_md:
	md_run.started = 0;
	/********message body********/
	msg_init(&msg);
	msg.message = MSG_VIDEO2_STOP;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	manager_common_send_message(SERVER_VIDEO2, &msg);
	/****************************/
	return ret;
}

static int *video2_md_func(void *arg)
{
	video2_md_config_t ctrl;
	int st;
	char fname[MAX_SYSTEM_STRING_SIZE];
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    sprintf(fname, "md-%d",time_get_now_stamp());
    misc_set_thread_name(fname);
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl, (video2_md_config_t*)arg, sizeof(video2_md_config_t) );
    video2_md_init( &ctrl, config.profile.profile[config.profile.quality].video.width,
    		config.profile.profile[config.profile.quality].video.height);
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_MD, 1 );
    manager_common_send_dummy(SERVER_VIDEO2);
    while( 1 ) {
    	st = info.status;
    	if( info.exit ) break;
    	if( !md_run.started ) break;
    	else if( st == STATUS_START )
    		continue;
    	if( misc_get_bit(info.thread_exit, THREAD_MD) ) break;
 		video2_md_proc();
 		usleep(100*1000);
    }
    //release
    video2_md_release();
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_MD, 0 );
    manager_common_send_dummy(SERVER_VIDEO2);
    log_qcy(DEBUG_INFO, "-----------thread exit: %s-----------",fname);
    pthread_exit(0);
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
    	if( misc_get_bit(info.thread_exit, THREAD_3ACTRL) )
    		break;
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
    video2_white_balance_release();
    video2_exposure_release();
    video2_focus_release();
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_3ACTRL, 0 );
    manager_common_send_dummy(SERVER_VIDEO2);
    log_qcy(DEBUG_INFO, "-----------thread exit: server_video2_3a_control-----------");
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
    manager_common_send_dummy(SERVER_VIDEO2);
    while( 1 ) {
    	if( misc_get_bit(info.thread_exit, THREAD_OSD) )
    		break;
    	if( info.exit ) break;
    	st = info.status;
    	if( st == STATUS_RUN )
    		video2_osd_proc(&ctrl,stream.frame, config.osd.enable);
    	usleep(1000*500);
    }
    //release
exit:
    video2_osd_release();
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_OSD, 0 );
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
    misc_set_thread_name("server_video2_main");
    pthread_detach(pthread_self());
    //init
    memset( &qos, 0, sizeof(qos));
    memcpy( &ctrl,(video_stream_t*)arg, sizeof(video_stream_t));
    av_buffer_init(&v2buffer, &vlock);
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_VIDEO, 1 );
    manager_common_send_dummy(SERVER_VIDEO2);
    while( 1 ) {
    	if( info.exit ) break;
    	if( misc_get_bit(info.thread_exit, THREAD_VIDEO) )
    		break;
    	st = info.status;
    	if( st != STATUS_START && st != STATUS_RUN )
    		break;
    	else if( st == STATUS_START )
    		continue;
    	usleep(1000);
    	ret = rts_av_poll(ctrl.h264);
    	if(ret)
    		continue;
    	ret = rts_av_recv(ctrl.h264,  &buffer);
    	if( ret )
    		continue;
    	if ( buffer ) {
        	if( info.status2 == (1<<RUN_MODE_MOTION) ) {
        		rts_av_put_buffer(buffer);
        		continue;
        	}
        	packet = av_buffer_get_empty(&v2buffer, &qos.buffer_overrun, &qos.buffer_success);
    		packet->data = malloc( buffer->bytesused );
    		if( packet->data == NULL) {
    			log_qcy(DEBUG_WARNING, "allocate memory failed in video2 buffer, size=%d", buffer->bytesused);
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
    		for(i=0;i<MAX_RECORDER_JOB;i++) {
				if( misc_get_bit(info.status2, RUN_MODE_SAVE+i) ) {
					ret = write_video2_buffer(packet, MSG_RECORDER_VIDEO_DATA, SERVER_RECORDER, i);
					if( ret ) {
						qos.failed_send[RUN_MODE_SAVE+i]++;
						if( qos.failed_send[RUN_MODE_SAVE+i] > VIDEO_MAX_FAILED_SEND) {
							qos.failed_send[RUN_MODE_SAVE+i] = 0;
							video2_quit_send(SERVER_RECORDER, i);
							log_qcy(DEBUG_WARNING, "----shut down video recorder stream due to long overrun!------");
						}
					}
					else {
						av_packet_add(packet);
						qos.failed_send[RUN_MODE_SAVE+i] = 0;
					}
				}
    		}
/*    		if( misc_get_bit(info.status2, RUN_MODE_MICLOUD) ) {
      			if( write_video2_buffer(buffer, MSG_MICLOUD_VIDEO2_DATA, SERVER_MICLOUD, 0) != 0 )
    				log_qcy(DEBUG_SERIOUS, "Micloud ring buffer push failed!");
    									else
						av_packet_add(packet);
    		}
*/
    	}
    }
    //release
exit:
	av_buffer_release(&v2buffer);
    server_set_status(STATUS_TYPE_THREAD_START, THREAD_VIDEO, 0 );
    manager_common_send_dummy(SERVER_VIDEO2);
    log_qcy(DEBUG_INFO, "-----------thread exit: server_video2_main-----------");
    pthread_exit(0);
}

static int stream_init(void)
{
	stream.isp = -1;
	stream.h264 = -1;
	stream.osd = -1;
	stream.jpg = -1;
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
	if( stream.osd != -1 ) {
		ret = rts_av_enable_chn(stream.osd);
		if (ret) {
			log_qcy(DEBUG_SERIOUS, "enable osd fail, ret = %d", ret);
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
	if( config.jpg.enable ) {
		if( stream.jpg != -1 ) {
			ret = rts_av_enable_chn(stream.jpg);
			if (ret) {
				log_qcy(DEBUG_SERIOUS, "enable jpg fail, ret = %d", ret);
				return -1;
			}
		}
		else {
			return -1;
		}
	}
	ret = pthread_create(&main_id, NULL, video2_main_func, (void*)&stream);
	if(ret != 0) {
		log_qcy(DEBUG_SERIOUS, "video2 main thread create error! ret = %d",ret);
		return -1;
	 }
	else {
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
	if( stream.osd!=-1 )
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

static int video2_init(void)
{
	int ret;
	struct rts_video_mjpeg_ctrl *mjpeg_ctrl = NULL;
	pthread_t osd_id;
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
   	stream.jpg = rts_av_create_mjpeg_chn(&config.jpg.jpg_ctrl);
    if (stream.jpg < 0) {
    	log_qcy(DEBUG_SERIOUS, "fail to create jpg chn, ret = %d\n", stream.jpg);
        return -1;
    }
    log_qcy(DEBUG_INFO, "jpg chnno:%d", stream.jpg);
    ret = rts_av_query_mjpeg_ctrl(stream.jpg, &mjpeg_ctrl);
    if (RTS_IS_ERR(ret)) {
    	log_qcy(DEBUG_SERIOUS, "rts_av_query_mjpeg_ctrl failed, ret = %d\n", ret);
        return -1;
    }
//      mjpeg_ctrl->normal_compress_rate = 15;
//	    ret = rts_av_set_mjpeg_ctrl(mjpeg_ctrl);
    ret = rts_av_bind(stream.isp, stream.jpg);
   	if (ret) {
   		log_qcy(DEBUG_SERIOUS, "fail to bind isp and jpg, ret %d", ret);
   		return -1;
   	}
	if( stream.osd != -1 ) {
		//start the osd thread
		ret = pthread_create(&osd_id, NULL, video2_osd_func, (void*)&config.osd);
		if(ret != 0) {
			log_qcy(DEBUG_SERIOUS, "osd2 thread create error! ret = %d",ret);
		 }
		else {
			log_qcy(DEBUG_SERIOUS, "osd2 thread create successful!");
		}
	}
	md_init_scheduler();
	video2_isp_init(&config.isp);
	return 0;
}

static void write_video2_info(struct rts_av_buffer *data, av_data_info_t *info)
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
    if( misc_get_bit(data->flags, 0/*RTSTREAM_PKT_FLAG_KEY*/) )// I frame
    	info->flag |= FLAG_FRAME_TYPE_IFRAME << 0;
    else
    	info->flag |= FLAG_FRAME_TYPE_PBFRAME << 0;
    if( config.profile.quality==0 )
        info->flag |= FLAG_RESOLUTION_VIDEO_360P << 17;
    else if( config.profile.quality==1 )
        info->flag |= FLAG_RESOLUTION_VIDEO_480P << 17;
    else if( config.profile.quality==2 )
        info->flag |= FLAG_RESOLUTION_VIDEO_1080P << 17;
    info->size = data->bytesused;
}

static int write_video2_buffer( av_packet_t *data, int id, int target, int channel)
{
	int ret=0;
	message_t msg;
    /********message body********/
	msg_init(&msg);
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.arg_in.wolf = channel;
	msg.message = id;
	msg.arg = data;
	msg.arg_size = 0;
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
	ret = pthread_rwlock_wrlock(&ilock);
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
	/********message body********/
	msg_init(&msg);
	msg.message = MSG_MANAGER_TIMER_REMOVE;
	msg.sender = msg.receiver = SERVER_VIDEO2;
	msg.arg_in.handler = md_check_scheduler;
	manager_common_send_message(SERVER_MANAGER, &msg);
	/****************************/
}

static void server_release_2(void)
{
	msg_buffer_release2(&message, &mutex);
	memset(&config,0,sizeof(video2_config_t));
	memset(&stream,0,sizeof(video_stream_t));
	memset(&md_run,0,sizeof(video2_md_run_t));
}

static void server_release_3(void)
{
	memset(&info, 0, sizeof(server_info_t));
}

static int video2_init_routine(void)
{
	message_t msg;
	if( !misc_get_bit( info.init_status, VIDEO2_INIT_CONDITION_REALTEK ) ) {
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_REALTEK_PROPERTY_GET;
		msg.sender = msg.receiver = SERVER_VIDEO2;
		msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
		manager_common_send_message(SERVER_REALTEK,    &msg);
		/****************************/
	}
	if( misc_full_bit( info.init_status, VIDEO2_INIT_CONDITION_NUM ) ) {
		info.status = STATUS_WAIT;
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_TIMER_REMOVE;
		msg.sender = msg.receiver = SERVER_VIDEO2;
		msg.arg_in.handler = video2_init_routine;
		manager_common_send_message(SERVER_MANAGER, &msg);
		/****************************/
	}
	return 0;
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
	int ret = 0;
	message_t msg;
	if( info.msg_lock ) return 0;
	//condition
	pthread_mutex_lock(&mutex);
	if( message.head == message.tail ) {
		if( info.status == info.old_status	) {
			pthread_cond_wait(&cond,&mutex);
		}
	}
	msg_init(&msg);
	ret = msg_buffer_pop(&message, &msg);
	pthread_mutex_unlock(&mutex);
	if( ret == 1)
		return 0;
	if( video2_message_filter(&msg) ) {
		msg_free(&msg);
		log_qcy(DEBUG_VERBOSE, "AUDIO message--- sender=%d, message=%x, ret=%d, head=%d, tail=%d was screened, the current task is %p", msg.sender, msg.message,
				ret, message.head, message.tail, info.task.func);
		return -1;
	}
	log_qcy(DEBUG_VERBOSE, "-----pop out from the VIDEO2 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg.sender, msg.message,
				ret, message.head, message.tail);
	msg_init(&info.task.msg);
	msg_deep_copy(&info.task.msg, &msg);
	switch(msg.message) {
		case MSG_VIDEO2_START:
			info.task.func = task_start;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO2_STOP:
			info.task.msg.arg_in.cat = info.status2;
			if( msg.sender == SERVER_MISS) misc_set_bit(&info.task.msg.arg_in.cat, (RUN_MODE_MISS + msg.arg_in.wolf), 0);
			if( msg.sender == SERVER_MICLOUD) misc_set_bit(&info.task.msg.arg_in.cat, RUN_MODE_MICLOUD, 0);
			if( msg.sender == SERVER_RECORDER) misc_set_bit(&info.task.msg.arg_in.cat, (RUN_MODE_SAVE + msg.arg_in.wolf), 0);
			if( msg.sender == SERVER_VIDEO) misc_set_bit(&info.task.msg.arg_in.cat, RUN_MODE_MOTION, 0);
			info.task.func = task_stop;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_MANAGER_EXIT:
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
		case MSG_VIDE02_SNAPSHOT:
			video2_snapshot(&msg);
			break;
		case MSG_VIDE02_SNAPSHOT_THUMB:
			video2_thumbnail(&msg);
			break;
		case MSG_MANAGER_DUMMY:
			break;
		case MSG_VIDEO2_PROPERTY_SET_DIRECT:
			video2_set_property(&msg);
			break;
		case MSG_VIDEO2_PROPERTY_GET:
			ret = video2_get_property(&msg);
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
 *
 */
static int server_none(void)
{
	int ret;
	message_t msg;
	if( !misc_get_bit( info.init_status, VIDEO2_INIT_CONDITION_CONFIG ) ) {
		ret = video2_config_video_read(&config);
		if( !ret && misc_full_bit( config.status, CONFIG_VIDEO2_MODULE_NUM) ) {
			misc_set_bit(&info.init_status, VIDEO2_INIT_CONDITION_CONFIG, 1);
		    /********message body********/
			msg_init(&msg);
			msg.message = MSG_MANAGER_TIMER_ADD;
			msg.sender = SERVER_VIDEO2;
			msg.arg_in.cat = 100;
			msg.arg_in.dog = 0;
			msg.arg_in.duck = 0;
			msg.arg_in.handler = &video2_init_routine;
			manager_common_send_message(SERVER_MANAGER, &msg);
			/****************************/
		}
		else {
			info.status = STATUS_ERROR;
			return ret;
		}
	}
	return ret;
}

static int server_setup(void)
{
	message_t msg;
	int ret = 0;
	if( video2_init() == 0){
		info.status = STATUS_IDLE;
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_TIMER_ADD;
		msg.sender = SERVER_VIDEO2;
		msg.arg_in.cat = 1000;
		msg.arg_in.dog = 0;
		msg.arg_in.duck = 0;
		msg.arg_in.handler = &md_check_scheduler;
		manager_common_send_message(SERVER_MANAGER, &msg);
		/****************************/
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
/*
 * task
 */
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
			info.status = STATUS_WAIT;
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
			if( misc_get_bit(info.thread_start, THREAD_VIDEO) )
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
		if( info.task.msg.sender == SERVER_MISS) misc_set_bit(&info.status2, (RUN_MODE_MISS + info.task.msg.arg_in.wolf), 1);
		if( info.task.msg.sender == SERVER_MICLOUD) misc_set_bit(&info.status2, RUN_MODE_MICLOUD, 1);
		if( info.task.msg.sender == SERVER_RECORDER) misc_set_bit(&info.status2, (RUN_MODE_SAVE + info.task.msg.arg_in.wolf), 1);
		if( info.task.msg.sender == SERVER_VIDEO) misc_set_bit(&info.status2, RUN_MODE_MOTION, 1);
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
			if( info.thread_start == 0)
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
	if( msg.result == 0 ) {
		if( info.task.msg.sender == SERVER_MISS) misc_set_bit(&info.status2, (RUN_MODE_MISS + info.task.msg.arg_in.wolf), 0);
		if( info.task.msg.sender == SERVER_MICLOUD) misc_set_bit(&info.status2, RUN_MODE_MICLOUD, 0);
		if( info.task.msg.sender == SERVER_RECORDER) misc_set_bit(&info.status2, (RUN_MODE_SAVE + info.task.msg.arg_in.wolf), 0);
		if( info.task.msg.sender == SERVER_VIDEO) misc_set_bit(&info.status2, RUN_MODE_MOTION, 0);
	}
	manager_common_send_message(info.task.msg.receiver, &msg);
	msg_free(&info.task.msg);
	info.task.func = &task_default;
	info.msg_lock = 0;
	return;
}

/*
 * default exit: *->exit
 */
static void task_exit(void)
{
	switch( info.status ){
		case EXIT_INIT:
			info.error = VIDEO2_EXIT_CONDITION;
			if( info.task.msg.sender == SERVER_MANAGER) {
				info.error &= (info.task.msg.arg_in.cat);
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

int server_video2_message( message_t *msg)
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
