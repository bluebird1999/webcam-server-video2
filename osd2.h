/*
 * osd2.h
 *
 *  Created on: Sep 9, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO2_OSD_H_
#define SERVER_VIDEO2_OSD_H_

/*
 * header
 */
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "config.h"

/*
 * define
 */

/*
 * structure
 */
typedef struct osd2_run_t {
    int							stream;
	struct rts_video_osd2_attr 	*osd_attr;
	unsigned char				*ipattern;
	unsigned char				*image2222;
	unsigned char				*image8888;
	FT_Library 					library;
    FT_Face 					face;
    int							color;
    int							rotate;
    int							alpha;
    int							pixel_size;
    int							offset_x;
    int							offset_y;
    int							width;
    int							height;
} osd2_run_t;
/*
 * function
 */
int video2_osd_init(video2_osd_config_t *ctrl, int stream, int width, int height);
int video2_osd_release(void);
int video2_osd_proc(video2_osd_config_t *ctrl);
int video2_osd_stop(video2_osd_config_t *ctrl);
int video2_osd_font_release(void);
int video2_osd_font_init(video2_osd_config_t *ctrl);


#endif /* SERVER_VIDEO2_OSD_H_ */
