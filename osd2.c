/*
 * osd2.c
 *
 *  Created on: Sep 9, 2020
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtscolor.h>
#include <rtscamkit.h>
#include <getopt.h>
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../tools/tools_interface.h"
//server header
#include "osd2.h"
#include "video2_interface.h"


/*
 * static
 */
//variable
static osd2_run_t					osd2_run;
static char cnum = 13;
static char patt[]     = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', ' ', ':'};
static char offset_x[] = { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   2,   0,   2};
static char offset_y[] = { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   1};
//function
static int osd2_image_to_8888(unsigned char *src, unsigned char *dst, unsigned int len);
static int osd2_draw_image_pattern(FT_Bitmap *bitmap, FT_Int x, FT_Int y, unsigned char *buf, int flag_rotate, int flag_ch);
static int osd2_get_picture_from_pattern(osd2_text_info_t *txt);
static int osd2_load_char(unsigned short c, unsigned char *pdata);
static int osd2_set_osd_timedate(osd2_text_info_t *text, int blkidx);

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
 */
static int osd2_image_to_8888(unsigned char *src, unsigned char *dst, unsigned int len)
{
	int i, j, ret = 0;
	for (i = 0; i < len; i++) {
		j = i * 4;
		dst[j] = (src[i] << 6) & 0xC0;
		dst[j + 1] = (src[i] << 4) & 0xC0;
		dst[j + 2] = (src[i] << 2) & 0xC0;
		if (osd2_run.alpha != 0)
			dst[j + 3] = osd2_run.alpha;
		else
			dst[j + 3] = (src[i] & 0xC0) ? (src[i] & 0xC0) : 0;
	}
	return ret;
}

static int osd2_draw_image_pattern(FT_Bitmap *bitmap, FT_Int x, FT_Int y,
		unsigned char *buf, int flag_rotate, int flag_ch)
{
	int ret = 0;
	FT_Int  i, j, p, q;
	FT_Int  x_max = x + bitmap->width;
	FT_Int  y_max = y + bitmap->rows;
	int width;
	int height;
	unsigned int len;
	if (flag_rotate) {
		width = osd2_run.pixel_size;
		height = osd2_run.pixel_size / 2;
	} else {
		width = osd2_run.pixel_size / 2;
		height = osd2_run.pixel_size;
	}
	if (flag_ch) {
		if (flag_rotate)
			height = osd2_run.pixel_size;
		else
			width = osd2_run.pixel_size;
	}
	for (i = x, p = 0; i < x_max; i++, p++) {
		for (j = y, q = 0; j < y_max; j++, q++) {
			if (i < 0 || j < 0 || i >= width || j >= height)
				continue;
			buf[j * width + i] |= bitmap->buffer[q * bitmap->width + p];
		}
	}
	len = width * height;
	for (i = 0; i < len; i++)
		buf[i] = buf[i] > 0 ? ((buf[i] & 0xC0) | osd2_run.color) : buf[i];
	return ret;
}

static int osd2_get_picture_from_pattern(osd2_text_info_t *txt)
{
	int i;
	int val;
	char *text = txt->text;
	int len = strlen(text);
	for (i = 0; i < len; i++) {
		if (text[i] == '-')
			val = 10;
		else if(text[i] == ' ')
			val = 11;
		else if(text[i] == ':')
			val = 12;
		else
			val = (int)(text[i] - '0');
		if (osd2_run.rotate) {
			memcpy((osd2_run.image2222 + (len - i - 1) * osd2_run.pixel_size * osd2_run.pixel_size / 2),
				   (osd2_run.ipattern + val * osd2_run.pixel_size * osd2_run.pixel_size / 2),
				   osd2_run.pixel_size * osd2_run.pixel_size / 2);
		} else {
			int p;
			int q;
			unsigned char *src = osd2_run.ipattern + val * osd2_run.pixel_size * osd2_run.pixel_size / 2;
			for (p = 0; p < osd2_run.pixel_size; p++) {
				for (q = 0; q < osd2_run.pixel_size/2; q++)
					osd2_run.image2222[p * osd2_run.pixel_size / 2 * txt->cnt + i * osd2_run.pixel_size / 2 + q] = src[p * osd2_run.pixel_size / 2 + q];
			}
		}
	}
	osd2_image_to_8888(osd2_run.image2222, osd2_run.image8888, osd2_run.pixel_size * osd2_run.pixel_size / 2 * txt->cnt);
	txt->pdata = osd2_run.image8888;
	txt->len = osd2_run.pixel_size * osd2_run.pixel_size / 2 * txt->cnt * 4;
	return 0;
}

static int osd2_load_char(unsigned short c, unsigned char *pdata)
{
	int ret=0, j=0;
	FT_GlyphSlot  	slot;
	FT_Matrix     	matrix;                 /* transformation matrix */
	FT_Vector     	pen;
	FT_Error      	error;
	double        	angle;
	int           	target_height;
	double        	angle_tmp;
	int           	origin_x;
	int           	origin_y;
	unsigned int	width;
	if ( !pdata )
		return -1;
	int flag_ch = c > 127 ? 1 : 0;
	if (flag_ch)
		width = osd2_run.pixel_size;
	else
		width = osd2_run.pixel_size/2;
	if ( osd2_run.rotate ) {
		angle_tmp = 90.0;
		target_height = width;
		origin_x = osd2_run.pixel_size;// - osd2_run.offset_x;
		if (flag_ch)
				origin_y = osd2_run.pixel_size / 2 * 2;
		else
				origin_y = osd2_run.pixel_size / 2;
	}
	else {
		angle_tmp = 0.0;
		target_height = osd2_run.pixel_size;
		angle_tmp = 0.0;
		target_height = osd2_run.pixel_size;
		origin_x = 0;
		origin_y = osd2_run.pixel_size;
		for(j=0;j<cnum;j++) {
			if( patt[j] == c ) {
				origin_x += offset_x[j] * (int)(osd2_run.pixel_size / 16 );
				origin_y -= offset_y[j] * (int)(osd2_run.pixel_size / 16 );
				break;
			}
		}
	}
	FT_Face *pface = &osd2_run.face;
	angle = (angle_tmp / 360) * 3.14159 * 2;
	slot = (*pface)->glyph;
	/* set up matrix */
	matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
	matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
	matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
	matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);
	/* the pen position in 26.6 cartesian space coordinates; */
	/* start at (origin_x, origin_y) relative to the upper left corner  */
	pen.x = origin_x * 64;
	pen.y = (target_height - origin_y) * 64;
	/* set transformation */
	FT_Set_Transform(*pface, &matrix, &pen);
	/* load glyph image into the slot (erase previous one) */
	error = FT_Load_Char(*pface, c, FT_LOAD_RENDER);
	if (error)
		return -1;           /* ignore errors */
	/* now, draw to our target surface (convert position) */
	osd2_draw_image_pattern(&slot->bitmap, slot->bitmap_left,
						 target_height - slot->bitmap_top,
						 pdata, osd2_run.rotate, flag_ch);
	return ret;
}

static int osd2_set_osd_timedate(osd2_text_info_t *text, int blkidx)
{
	int ret = 0;
	struct rts_video_osd2_block *block;
	if ( !osd2_run.osd_attr ) {
		log_qcy(DEBUG_SERIOUS, "osd attribute isn't initialized!");
		return -1;
	}
	ret = osd2_get_picture_from_pattern(text);
	if (ret < 0) {
		log_qcy(DEBUG_SERIOUS, "%s, get blk pict fail\n", __func__);
		return ret;
	}
	block = &osd2_run.osd_attr->blocks[blkidx];
	block->picture.length = text->len;
	block->picture.pdata = text->pdata;
	block->picture.pixel_fmt = RTS_OSD2_BLK_FMT_RGBA8888;
	block->rect.left = text->x;
	block->rect.top = text->y;
	if (osd2_run.rotate) {
		block->rect.right = text->x + osd2_run.pixel_size;
		block->rect.bottom = text->y + osd2_run.pixel_size / 2 * text->cnt;
	} else {
		block->rect.right = text->x + osd2_run.pixel_size / 2 * text->cnt;
		block->rect.bottom = text->y + osd2_run.pixel_size;
	}
	block->enable = 1;
	ret = rts_av_set_osd2_single(osd2_run.osd_attr, blkidx);
	if (ret < 0)
		log_qcy(DEBUG_INFO, "set osd2 fail, ret = %d\n", ret);
	return ret;
}

/*
 * interface
 */
int video2_osd_proc(video2_osd_config_t *ctrl)
{
	char now_time[20] = "";
	int ret;
	osd2_text_info_t text_tm;
	time_t now;
	struct tm tm = {0};
	//**color
	now = time(NULL);
	localtime_r(&now, &tm);
//	if( (tm.tm_hour >= 19) || (tm.tm_hour <= 7) )
		osd2_run.color = 0xFF;
//	else
//		osd2_run.color = 0x00;
	//***
	sprintf(now_time, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	text_tm.text = now_time;
	text_tm.cnt = strlen(now_time);
	if (osd2_run.rotate) {
		text_tm.x = osd2_run.offset_x;
		text_tm.y = osd2_run.offset_y;
	} else {
		text_tm.x = osd2_run.offset_x;
		text_tm.y = osd2_run.offset_y;
	}
	ret = osd2_set_osd_timedate(&text_tm, 0);
	if (ret < 0) {
		log_qcy(DEBUG_INFO, "%s, set osd2 attr fail\n", __func__);
		return -1;
	}
	return ret;
}

int video2_osd_init(video2_osd_config_t *ctrl, int stream, int width, int height)
{
	time_t now;
	struct tm tm = {0};
	int i, ret = 0;
	//***
	osd2_run.stream = stream;
	osd2_run.rotate = ctrl->time_rotate;
	osd2_run.alpha = ctrl->time_alpha;
	osd2_run.color = ctrl->time_color;
	//**color
	now = time(NULL);
	localtime_r(&now, &tm);
//	if( (tm.tm_hour >= 19) || (tm.tm_hour <= 7) )
	osd2_run.color = 0xFF;
//	else
//		osd2_run.color = 0x00;
	//***
	osd2_run.width = width;
	osd2_run.height = height;
	if( width >= 1920 ) {
		osd2_run.pixel_size = 48;
		osd2_run.offset_x = 12;
		osd2_run.offset_y = 10;
	}
	else if( width >= 1280 ) {
		osd2_run.pixel_size = 32;
		osd2_run.offset_x = 8;
		osd2_run.offset_y = 6;
	}
	else if( width >= 864 ){
		osd2_run.pixel_size = 22;
		osd2_run.offset_x = 6;
		osd2_run.offset_y = 4;
	}
	else {
		osd2_run.pixel_size = 16;
		osd2_run.offset_x = 4;
		osd2_run.offset_y = 4;
	}
	FT_Set_Pixel_Sizes(osd2_run.face, osd2_run.pixel_size, 0);
	osd2_run.ipattern = (unsigned char *)calloc( osd2_run.pixel_size * osd2_run.pixel_size / 2, sizeof(patt) );
	if (!osd2_run.ipattern) {
		log_qcy(DEBUG_WARNING, "%s calloc fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	osd2_run.image2222 = (unsigned char *)calloc( 20 * osd2_run.pixel_size * osd2_run.pixel_size / 2, 1 );
	if (!osd2_run.image2222) {
		log_qcy(DEBUG_WARNING, "%s calloc fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	osd2_run.image8888 = (unsigned char *)calloc( 20 * 4 * osd2_run.pixel_size * osd2_run.pixel_size / 2, 1);
	if (!osd2_run.image8888) {
		log_qcy(DEBUG_WARNING, "%s calloc fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	for (i = 0; i < sizeof(patt); i++) {
		osd2_load_char( (unsigned short)patt[i], osd2_run.ipattern + osd2_run.pixel_size * osd2_run.pixel_size / 2 * i);
	}
    RTS_SAFE_RELEASE(osd2_run.osd_attr, rts_av_release_osd2);
	ret = rts_av_query_osd2(osd2_run.stream, &osd2_run.osd_attr);
	if (ret < 0) {
		log_qcy(DEBUG_SERIOUS, "%s, query osd2 attr fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	return ret;
}

int video2_osd_release(void)
{
	if( osd2_run.ipattern ) {
		free( osd2_run.ipattern);
		osd2_run.ipattern = NULL;
	}
	if( osd2_run.image2222 ) {
		free( osd2_run.image2222);
		osd2_run.image2222 = NULL;
	}
	if( osd2_run.image8888 ) {
		free( osd2_run.image8888);
		osd2_run.image8888 = NULL;
	}
    RTS_SAFE_RELEASE(osd2_run.osd_attr, rts_av_release_osd2);
}

int video2_osd_font_init(video2_osd_config_t *ctrl)
{
	char face_path[32];
	//init freetype
	FT_Init_FreeType(&osd2_run.library);
	memset(face_path, 0, sizeof(face_path));
	snprintf(face_path, 32, "%sfont/%s%s", _config_.qcy_path, ctrl->time_font_face, ".ttf");
	FT_New_Face(osd2_run.library, face_path, 0, &osd2_run.face);
}

int video2_osd_font_release(void)
{
	int ret;
	ret = FT_Done_Face(osd2_run.face);
   	ret = FT_Done_FreeType(osd2_run.library);
    return ret;
}


