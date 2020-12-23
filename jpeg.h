/*
 * jpeg.h
 *
 *  Created on: Dec 9, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO_JPEG_H_
#define SERVER_VIDEO_JPEG_H_

/*
 * header
 */
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>

/*
 * define
 */


/*
 * structure
 */
struct my_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */
  jmp_buf setjmp_buffer;    /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

/*
 * function
 */
int video_jpeg_thumbnail(const char* input, int w, int h);

#endif /* SERVER_VIDEO_JPEG_H_ */
