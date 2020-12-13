/*
 * osd.c
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
//program header
#include "../../tools/log.h"
//server header
#include "osd.h"
#include "video2_interface.h"


/*
 * static
 */
//variable
static struct rts_video_osd2_attr 	*osd_attr;
static osd_run_t					osd_run;
static char cnum = 13;
static char patt[]     = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', ' ', ':'};
static char offset_x[] = { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   2,   0,   2};
static char offset_y[] = { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   1};
//function
static int osd_image_to_8888(unsigned char *src, unsigned char *dst, unsigned int len);
static int osd_draw_image_pattern(FT_Bitmap *bitmap, FT_Int x, FT_Int y, unsigned char *buf, int flag_rotate, int flag_ch);
static int osd_get_picture_from_pattern(osd2_text_info_t *txt);
static int osd_load_char(unsigned short c, unsigned char *pdata);
static int osd_set_osd2_timedate(osd2_text_info_t *text, int blkidx);
static int osd_adjust_txt_picture(char *txt, unsigned char *dst, unsigned int len);
static int osd_set_osd2_text(void);
static int osd_set_osd2_color_table(void);

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
 */
static int osd_image_to_8888(unsigned char *src, unsigned char *dst, unsigned int len)
{
	int i, j, ret = 0;
	for (i = 0; i < len; i++) {
		j = i * 4;
		dst[j] = (src[i] << 6) & 0xC0;
		dst[j + 1] = (src[i] << 4) & 0xC0;
		dst[j + 2] = (src[i] << 2) & 0xC0;
		if (osd_run.alpha != 0)
			dst[j + 3] = osd_run.alpha;
		else
			dst[j + 3] = (src[i] & 0xC0) ? (src[i] & 0xC0) : 0;
	}
	return ret;
}

static int osd_draw_image_pattern(FT_Bitmap *bitmap, FT_Int x, FT_Int y,
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
		width = osd_run.pixel_size;
		height = osd_run.pixel_size / 2;
	} else {
		width = osd_run.pixel_size / 2;
		height = osd_run.pixel_size;
	}
	if (flag_ch) {
		if (flag_rotate)
			height = osd_run.pixel_size;
		else
			width = osd_run.pixel_size;
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
		buf[i] = buf[i] > 0 ? ((buf[i] & 0xC0) | osd_run.color) : buf[i];
	return ret;
}

static int osd_get_picture_from_pattern(osd2_text_info_t *txt)
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
		if (osd_run.rotate) {
			memcpy((osd_run.image2222 + (len - i - 1) * osd_run.pixel_size * osd_run.pixel_size / 2),
				   (osd_run.ipattern + val * osd_run.pixel_size * osd_run.pixel_size / 2),
				   osd_run.pixel_size * osd_run.pixel_size / 2);
		} else {
			int p;
			int q;
			unsigned char *src = osd_run.ipattern + val * osd_run.pixel_size * osd_run.pixel_size / 2;
			for (p = 0; p < osd_run.pixel_size; p++) {
				for (q = 0; q < osd_run.pixel_size/2; q++)
					osd_run.image2222[p * osd_run.pixel_size / 2 * txt->cnt + i * osd_run.pixel_size / 2 + q] = src[p * osd_run.pixel_size / 2 + q];
			}
		}
	}
	osd_image_to_8888(osd_run.image2222, osd_run.image8888, osd_run.pixel_size * osd_run.pixel_size / 2 * txt->cnt);
	txt->pdata = osd_run.image8888;
	txt->len = osd_run.pixel_size * osd_run.pixel_size / 2 * txt->cnt * 4;
	return 0;
}

static int osd_load_char(unsigned short c, unsigned char *pdata)
{
	int ret=0,j;
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
		width = osd_run.pixel_size;
	else
		width = osd_run.pixel_size/2;
	if ( osd_run.rotate ) {
		angle_tmp = 90.0;
		target_height = width;
		origin_x = osd_run.pixel_size;// - osd_run.offset_x;
		if (flag_ch)
				origin_y = osd_run.pixel_size / 2 * 2;
		else
				origin_y = osd_run.pixel_size / 2;
	}
	else {
		angle_tmp = 0.0;
		target_height = osd_run.pixel_size;
		angle_tmp = 0.0;
		target_height = osd_run.pixel_size;
		origin_x = 0;
		origin_y = osd_run.pixel_size;
		for(j=0;j<cnum;j++) {
			if( patt[j] == c ) {
				origin_x += offset_x[j] * (int)(osd_run.pixel_size / 16 );
				origin_y -= offset_y[j] * (int)(osd_run.pixel_size / 16 );
				break;
			}
		}
	}
	FT_Face *pface = &osd_run.face;
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
	osd_draw_image_pattern(&slot->bitmap, slot->bitmap_left,
						 target_height - slot->bitmap_top,
						 pdata, osd_run.rotate, flag_ch);
	return ret;
}

static int osd_set_osd2_timedate(osd2_text_info_t *text, int blkidx)
{
	int ret = 0;
	struct rts_video_osd2_block *block;
	if ( !osd_run.osd_attr ) {
		log_qcy(DEBUG_SERIOUS, "osd2 attribute isn't initialized!");
		return -1;
	}
	ret = osd_get_picture_from_pattern(text);
	if (ret < 0) {
		log_qcy(DEBUG_SERIOUS, "%s, get blk pict fail\n", __func__);
		return ret;
	}
	block = &osd_run.osd_attr->blocks[blkidx];
	block->picture.length = text->len;
	block->picture.pdata = text->pdata;
	block->picture.pixel_fmt = RTS_OSD2_BLK_FMT_RGBA8888;
	block->rect.left = text->x;
	block->rect.top = text->y;
	if (osd_run.rotate) {
		block->rect.right = text->x + osd_run.pixel_size;
		block->rect.bottom = text->y + osd_run.pixel_size / 2 * text->cnt;
	} else {
		block->rect.right = text->x + osd_run.pixel_size / 2 * text->cnt;
		block->rect.bottom = text->y + osd_run.pixel_size;
	}
	block->enable = 1;
	ret = rts_av_set_osd2_single(osd_run.osd_attr, blkidx);
	if (ret < 0)
		log_qcy(DEBUG_SERIOUS, "set osd2 fail, ret = %d\n", ret);
	return ret;
}

static int osd_adjust_txt_picture(char *txt, unsigned char *dst, unsigned int len)
{
	int i;
	unsigned char *tmp;
	unsigned int width;
	int w = 0;
	if ( !txt || !dst) {
		log_qcy(DEBUG_SERIOUS, "empty pointer %s", __func__);
		return -1;
	}
	tmp = (unsigned char *)malloc(len);
	if (!tmp) {
		log_qcy(DEBUG_SERIOUS, "malloc error: %s", __func__);
		return -1;
	}
	memcpy(tmp, dst, len);
	for (i = 0; i < strlen(txt); i++) {
		if (0x0e == ((unsigned char)txt[i]) >> 4) {
			width = osd_run.pixel_size;
			i += 2;
		} else {
			width = osd_run.pixel_size / 2;
		}
		if (osd_run.rotate) {
			memcpy((dst + (len / osd_run.pixel_size - w - width) * osd_run.pixel_size),
				   (tmp + w * osd_run.pixel_size),
				   width * osd_run.pixel_size);
		} else {
			int p;
			int q;
			unsigned char *src = tmp + w * osd_run.pixel_size;
			for (p = 0; p < osd_run.pixel_size; p++) {
				for (q = 0; q < width; q++)
					dst[p * len / osd_run.pixel_size + w + q] = src[p * width + q];
			}
		}
		w += width;
	}
	rts_free(tmp);
	return 0;
}

static int osd_set_osd2_text(void)
{
	struct rts_video_osd2_block *block;
	char show_txt[] = "QCY";
	int ret = -1;
	unsigned char *pbuf;
	int i;
	unsigned int len = 0;
	unsigned short ch = 0;
	unsigned char *p;//[WIDTH_CH * HEIGHT];
	if( osd_run.osd_attr == NULL ) {
		log_qcy(DEBUG_SERIOUS, "%s osd attribute empty!",__func__);
		return -1;
	}
	p = malloc( osd_run.pixel_size * osd_run.pixel_size );
	if( p == NULL ) {
		log_qcy(DEBUG_SERIOUS, "%s memory allocation failed!",__func__);
		return -1;
	}
	for (i = 0; i < strlen(show_txt); i++) {
		if (0x0e == ((unsigned char)show_txt[i]) >> 4) {
			i += 2;
			len += osd_run.pixel_size * osd_run.pixel_size;
		} else {
			len += osd_run.pixel_size * osd_run.pixel_size / 2;
		}
	}
	block = &osd_run.osd_attr->blocks[3];
	block->picture.length = len;
	pbuf = (unsigned char *)malloc(len);
	if (!pbuf) {
		free(p);
		log_qcy(DEBUG_SERIOUS, "%s memory allocation failed!",__func__);
		return -1;
	}
	unsigned char *ptmp = pbuf;
	for (i = 0; i < strlen(show_txt); i++) {
		unsigned int char_len;
		if (0x0e == ((unsigned char)show_txt[i]) >> 4) {
			ch = (unsigned short)(show_txt[i] & 0x0f) << 12 |
				 (unsigned short)(show_txt[i + 1] & 0x3f) << 6 |
				 (unsigned short)(show_txt[i + 2] & 0x3f);
			i += 2;
			char_len = osd_run.pixel_size * osd_run.pixel_size;
		} else {
			ch = (unsigned short)show_txt[i];
			char_len = osd_run.pixel_size / 2 * osd_run.pixel_size;
		}
		memset(p, 0, osd_run.pixel_size * osd_run.pixel_size);
		osd_load_char(ch, p);
		memcpy(ptmp, p, char_len);
		ptmp += char_len;
	}
	ret = osd_adjust_txt_picture(show_txt, pbuf, len);
	if( ret < 0 ) {
		log_qcy(DEBUG_SERIOUS, "error from adjust txt picture!");
		free(p);
		free(pbuf);
		return -1;
	}
	block->picture.pixel_fmt = RTS_OSD2_BLK_FMT_RGBA2222;
	block->picture.pdata = pbuf;
	if (osd_run.rotate) {
		block->rect.left = 10;
		block->rect.top = 10;
	} else {
		block->rect.left = 10;
		block->rect.top = 10;
	}
	if (osd_run.rotate) {
		block->rect.right = block->rect.left + osd_run.pixel_size;
		block->rect.bottom = block->rect.top + len / osd_run.pixel_size;
	} else {
		block->rect.right = block->rect.left + len / osd_run.pixel_size;
		block->rect.bottom = block->rect.top + osd_run.pixel_size;
	}
	block->enable = 1;
	ret = rts_av_set_osd2_single(osd_run.osd_attr, 3);
	if (ret < 0) {
		log_qcy(DEBUG_SERIOUS, "set osd2 fail, ret = %d\n", ret);
	}
	free(p);
	free(pbuf);
	return ret;
}

static int osd_set_osd2_color_table(void)
{
	int ret;
	unsigned int val = 0xffffffff; /*green(from high to low: r, g, b, a)*/
	unsigned char r = 3; /*0b11*/
	unsigned char g = 3; /*0b11*/
	unsigned char b = 3; /*0b11*/
	unsigned char a = 3; /*0b11*/
	if (!osd_run.osd_attr)
		return -1;
	ret = rts_av_set_osd2_color_table(osd_run.osd_attr, RTS_OSD2_BLK_FMT_RGBA2222, val, r, g, b, a);
	if ( ret!=0 ) {
		log_qcy(DEBUG_SERIOUS, "set osd2 color table fail\n");
		return -1;
	}
	if (rts_av_get_osd2_color_table(osd_run.osd_attr, RTS_OSD2_BLK_FMT_RGBA2222, r, g, b, a) != val) {
		log_qcy(DEBUG_SERIOUS, "get and compare osd2 color table fail\n");
		return -1;
	}
	return 0;
}

/*
 * interface
 */
int video2_osd_proc(video2_osd_config_t *ctrl, int frame, int enable)
{
	char now_time[20] = "00:00:00";
	int ret;
	osd2_text_info_t text_tm;
	time_t now;
	struct tm tm = {0};
	int i;
	if (enable) {
		osd_run.osd_attr->blocks[0].enable = 1;
	}
	else {
		osd_run.osd_attr->blocks[0].enable = 0;
		return 0;
	}
	now = time(NULL);
	localtime_r(&now, &tm);
	sprintf(now_time, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	text_tm.text = now_time;
	text_tm.cnt = strlen(now_time);
	if (osd_run.rotate) {
		text_tm.x = osd_run.offset_x;
		text_tm.y = osd_run.offset_y;
	} else {
		text_tm.x = osd_run.offset_x;
		text_tm.y = osd_run.offset_y;
	}
	ret = osd_set_osd2_timedate(&text_tm, 0);
	if (ret < 0) {
		log_qcy(DEBUG_SERIOUS, "%s, set osd2 attr fail\n", __func__);
		return -1;
	}
	return ret;
}

int video2_osd_init(video2_osd_config_t *ctrl, int stream, int width, int height)
{
	int ret=0;
	char face_path[32];
	time_t now;
	struct tm tm = {0};
	int i;
	now = time(NULL);
	localtime_r(&now, &tm);
	osd_run.stream = stream;
	osd_run.rotate = ctrl->time_rotate;
	osd_run.alpha = ctrl->time_alpha;
	osd_run.width = width;
	osd_run.height = height;
	osd_run.color = ctrl->time_color;
	if( width >= 1920 ) {
		osd_run.pixel_size = 48;
		osd_run.offset_x = 12;
		osd_run.offset_y = 10;
	}
	else if( width >= 1280 ) {
		osd_run.pixel_size = 32;
		osd_run.offset_x = 8;
		osd_run.offset_y = 6;
	}
	else if( width >= 864 ){
		osd_run.pixel_size = 22;
		osd_run.offset_x = 6;
		osd_run.offset_y = 4;
	}
	else {
		osd_run.pixel_size = 16;
		osd_run.offset_x = 4;
		osd_run.offset_y = 4;
	}
	if( tm.tm_hour >= 18 ) osd_run.color = 0xFF;
	else osd_run.color = 0x00;
	//init freetype
	FT_Init_FreeType(&osd_run.library);
	snprintf(face_path, 32, "%sfont/%s%s", _config_.qcy_path, ctrl->time_font_face, ".ttf");
	FT_New_Face(osd_run.library, face_path, 0, &osd_run.face);
	FT_Set_Pixel_Sizes(osd_run.face, osd_run.pixel_size, 0);
	osd_run.ipattern = (unsigned char *)calloc( osd_run.pixel_size * osd_run.pixel_size / 2, sizeof(patt) );
	if (!osd_run.ipattern) {
		log_qcy(DEBUG_SERIOUS, "%s calloc fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	osd_run.image2222 = (unsigned char *)calloc( 20 * osd_run.pixel_size * osd_run.pixel_size / 2, 1 );
	if (!osd_run.image2222) {
		log_qcy(DEBUG_SERIOUS, "%s calloc fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	osd_run.image8888 = (unsigned char *)calloc( 20 * 4 * osd_run.pixel_size * osd_run.pixel_size / 2, 1);
	if (!osd_run.image8888) {
		log_qcy(DEBUG_SERIOUS, "%s calloc fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	for (i = 0; i < sizeof(patt); i++) {
		osd_load_char( (unsigned short)patt[i], osd_run.ipattern + osd_run.pixel_size * osd_run.pixel_size / 2 * i);
	}
	ret = rts_av_query_osd2(osd_run.stream, &osd_run.osd_attr);
	if (ret < 0) {
		log_qcy(DEBUG_SERIOUS, "%s, query osd2 attr fail\n", __func__);
		video2_osd_release();
		return -1;
	}
	return ret;
}

int video2_osd_release(void)
{
	int ret=0;
	if( osd_run.ipattern ) {
		free( osd_run.ipattern);
		osd_run.ipattern = NULL;
	}
	if( osd_run.image2222 ) {
		free( osd_run.image2222);
		osd_run.image2222 = NULL;
	}
	if( osd_run.image8888 ) {
		free( osd_run.image8888);
		osd_run.image8888 = NULL;
	}
	FT_Done_Face(osd_run.face);
   	FT_Done_FreeType(osd_run.library);
    RTS_SAFE_RELEASE(osd_run.osd_attr, rts_av_release_osd2);
	return ret;
}
