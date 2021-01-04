/*
 * jpeg.c
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
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../manager/manager_interface.h"
#include "../../tools/tools_interface.h"
//server header
#include "jpeg.h"
/*
 * static
 */

//variable
//function

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static int jpeg_check_sd(void)
{
	if( access("/mnt/media/normal", F_OK) ) {
		log_qcy(DEBUG_INFO, "SD card access failed!, quit all snapshot!----");
		return -1;
	}
	else
		return 0;
}

METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;
  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);
  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

static unsigned char* video_jpeg_read(const char* path, int *width, int *height)
{
	FILE *file = fopen( path, "rb" );
	if ( file == NULL ) {
		return NULL;
	}
	struct jpeg_decompress_struct info; //for our jpeg info
	struct my_error_mgr my_err;
	info.err = jpeg_std_error(&my_err.pub);
	my_err.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(my_err.setjmp_buffer)) {
		 /* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		log_qcy(DEBUG_INFO, "Error occured\n");
		jpeg_destroy_decompress(&info);
		fclose(file);
		return NULL;
	}
	jpeg_create_decompress( &info ); //fills info structure
	jpeg_stdio_src( &info, file );        //void
	int ret_Read_Head = jpeg_read_header( &info, 1 ); //int
	if(ret_Read_Head != JPEG_HEADER_OK) {
		log_qcy(DEBUG_INFO, "jpeg_read_header failed\n");
		fclose(file);
		jpeg_destroy_decompress(&info);
		return NULL;
	}
	int bStart = jpeg_start_decompress( &info );
	if(!bStart){
		log_qcy(DEBUG_INFO, "jpeg_start_decompress failed\n");
		fclose(file);
		jpeg_destroy_decompress(&info);
		return NULL;
	}
	int w = *width = info.output_width;
	int h = *height = info.output_height;
	int numChannels = info.num_components; // 3 = RGB, 4 = RGBA
	unsigned long dataSize = w * h * numChannels;
	// read RGB(A) scanlines one at a time into jdata[]
	unsigned char *data = (unsigned char *)malloc( dataSize );
	if(!data) {
		log_qcy(DEBUG_INFO, "jpeg_start_decompress failed\n");
		fclose(file);
		jpeg_destroy_decompress(&info);
		return NULL;
	}
	unsigned char* rowptr;
	while ( info.output_scanline < h ) {
		rowptr = data + info.output_scanline * w * numChannels;
		jpeg_read_scanlines( &info, &rowptr, 1 );
	}
	jpeg_finish_decompress( &info );
	fclose( file );
	return data;
}

static unsigned char* video_jpeg_stretch_linear(int w_Dest,int h_Dest,int bit_depth,unsigned char *src,int w_Src,int h_Src)
{
    int sw = w_Src-1, sh = h_Src-1, dw = w_Dest-1, dh = h_Dest-1;
    int B, N, x, y;
    int nPixelSize = bit_depth/8;
    unsigned char *pLinePrev,*pLineNext;
    unsigned char *pDest = NULL;
    unsigned char *tmp;
    unsigned char *pA,*pB,*pC,*pD;

    pDest = malloc( w_Dest*h_Dest*bit_depth/8 );
    for(int i=0;i<=dh;++i) {
        tmp =pDest + i*w_Dest*nPixelSize;
        y = i*sh/dh;
        N = dh - i*sh%dh;
        pLinePrev = src + (y++)*w_Src*nPixelSize;
        //pLinePrev =(unsigned char *)aSrc->m_bitBuf+((y++)*aSrc->m_width*nPixelSize);
        pLineNext = (N==dh) ? pLinePrev : src+y*w_Src*nPixelSize;
        //pLineNext = ( N == dh ) ? pLinePrev : (unsigned char *)aSrc->m_bitBuf+(y*aSrc->m_width*nPixelSize);
        for(int j=0;j<=dw;++j) {
            x = j*sw/dw*nPixelSize;
            B = dw-j*sw%dw;
            pA = pLinePrev+x;
            pB = pA+nPixelSize;
            pC = pLineNext + x;
            pD = pC + nPixelSize;
            if(B == dw) {
                pB=pA;
                pD=pC;
            }
            for(int k=0;k<nPixelSize;++k) {
                *tmp++ = ( unsigned char )( int )(
                    ( B * N * ( *pA++ - *pB - *pC + *pD ) + dw * N * *pB++
                    + dh * B * *pC++ + ( dw * dh - dh * B - dw * N ) * *pD++
                    + dw * dh / 2 ) / ( dw * dh ) );
            }
        }
    }
    return pDest;
}

static int video_jpeg_write(const char * filename, unsigned char* image_buffer, int quality,int image_height, int image_width)
{
	int ret = 0;
    if(filename == NULL || image_buffer == NULL)
    	return 1;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;        /* target file */
    JSAMPROW row_pointer[1];    /* pointer to JSAMPLE row[s] */
    int row_stride;        /* physical row width in image buffer */
    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);
    if ((outfile = fopen(filename, "wb")) == NULL) {
        log_qcy(DEBUG_INFO, stderr, "can't open %s\n", filename);
        return 1;
    }
    jpeg_stdio_dest(&cinfo, outfile);
    cinfo.image_width = image_width;     /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 3;        /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;     /* colorspace of input image */
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, 1 /* limit to baseline-JPEG values */);
    jpeg_start_compress(&cinfo, 1);
    row_stride = image_width * 3;    /* JSAMPLEs per row in image_buffer */
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
        if( !jpeg_check_sd() ) {
        	(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
        else {
        	ret = 1;
        	goto exit;
        }
    }
    jpeg_finish_compress(&cinfo);
exit:
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    return ret;
}

 int video_jpeg_thumbnail(const char* input, int w, int h)
{
	 char fname[MAX_SYSTEM_STRING_SIZE*4];
	 char temp[MAX_SYSTEM_STRING_SIZE*4];
	 int ret = 0;
	 int width = 0, height = 0;
	 unsigned char *buff = NULL;
	 unsigned char *img_buf = NULL;
	 if(input == NULL)
    	return -1;
	 memset(fname, 0, sizeof(fname));
	 memset(temp, 0, sizeof(temp));
	 memcpy(temp, input, strlen(input)-6); //remove "_f.jpg"
	 sprintf(fname, "%s.jpg",temp);
     buff = video_jpeg_read(input, &width, &height);
     if(buff == NULL) {
    	 log_qcy(DEBUG_WARNING, "ReadJpeg Failed\n");
    	 return -1;
     }
    img_buf = video_jpeg_stretch_linear(w, h, 24, buff,  width, height);
    free(buff);
    ret = video_jpeg_write(fname, img_buf, 60, h, w);
   	free(img_buf);
    if(!ret){
        return 0;
    }
    else{
        log_qcy(DEBUG_INFO, "GenerateImageThumbnail: write failed!");
        return -1;
    }
}
