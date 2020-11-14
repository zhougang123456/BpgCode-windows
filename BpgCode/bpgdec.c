/*
 * BPG decoder command line utility
 *
 * Copyright (c) 2014 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>

/* define it to include PNG output */
#define USE_PNG

#ifdef USE_PNG
#include <png.h>
#endif

#include "libbpg.h"
#include "string.h"

#include "windows.h"

static void bmp_save(BPGDecoderContext* img, const char* filename)
{
    BPGImageInfo img_info_s, * img_info = &img_info_s;
    FILE* f;
    int w, h, y, imgLength;
    uint8_t* rgb_line;

    bpg_decoder_get_info(img, img_info);

    w = img_info->width;
    h = img_info->height;
    imgLength = w * h * 4;

    rgb_line = malloc(4 * w);

    BITMAPFILEHEADER bmheader;
    memset(&bmheader, 0, sizeof(bmheader));
    bmheader.bfType = 0x4d42;     //图像格式。必须为'BM'格式。  
    bmheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); //从文件开头到数据的偏移量  
    bmheader.bfSize = imgLength + bmheader.bfOffBits;//文件大小  

    BITMAPINFOHEADER bmInfo;
    memset(&bmInfo, 0, sizeof(bmInfo));
    bmInfo.biSize = sizeof(bmInfo);
    bmInfo.biWidth = w;
    bmInfo.biHeight = h;
    bmInfo.biPlanes = 1;
    bmInfo.biBitCount = 32;
    bmInfo.biCompression = 0L;

    bpg_decoder_start(img, BPG_OUTPUT_FORMAT_RGBA32);

    HANDLE hFile = CreateFileA(filename, 0x40000000L, 0, 0, 2, 0, 0);
    
    DWORD dwWritten;
    BOOL bRet = WriteFile(hFile, &bmheader, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
    bRet = WriteFile(hFile, &bmInfo, sizeof(BITMAPINFOHEADER), &dwWritten, NULL);
    for (y = 0; y < h; y++) {
        bpg_decoder_get_line(img, rgb_line);
        bRet = WriteFile(hFile, rgb_line, 4 * w, &dwWritten, NULL);
    }
        
    CloseHandle(hFile);
   
    free(rgb_line);
}

static void ppm_save(BPGDecoderContext *img, const char *filename)
{
    BPGImageInfo img_info_s, *img_info = &img_info_s;
    FILE *f;
    int w, h, y;
    uint8_t *rgb_line;

    bpg_decoder_get_info(img, img_info);
    
    w = img_info->width;
    h = img_info->height;

    rgb_line = malloc(3 * w);

    f = fopen(filename,"wb");
    if (!f) {
        fprintf(stderr, "%s: I/O error\n", filename);
        exit(1);
    }
        
    fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);
    
    bpg_decoder_start(img, BPG_OUTPUT_FORMAT_RGB24);
    for (y = 0; y < h; y++) {
        bpg_decoder_get_line(img, rgb_line);
        fwrite(rgb_line, 1, w * 3, f);
    }
    fclose(f);

    free(rgb_line);
}

#ifdef USE_PNG
static void png_write_data (png_structp png_ptr, png_bytep data,
                            png_size_t length)
{
    FILE *f;
    int ret;

    f = png_get_io_ptr(png_ptr);
    ret = fwrite(data, 1, length, f);
    if (ret != length)
	png_error(png_ptr, "PNG Write Error");
}

static void png_save(BPGDecoderContext *img, const char *filename, int bit_depth)
{
    BPGImageInfo img_info_s, *img_info = &img_info_s;
    FILE *f;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep row_pointer;
    int y, color_type, bpp;
    BPGDecoderOutputFormat out_fmt;

    if (bit_depth != 8 && bit_depth != 16) {
        fprintf(stderr, "Only bit_depth = 8 or 16 are supported for PNG output\n");
        exit(1);
    }

    bpg_decoder_get_info(img, img_info);

    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "%s: I/O error\n", filename);
        exit(1);
    }

    png_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING,
                                        NULL,
                                        NULL,  /* error */
                                        NULL, /* warning */
                                        NULL,
                                        NULL,
                                        NULL);
    info_ptr = png_create_info_struct(png_ptr);
    png_set_write_fn(png_ptr, (png_voidp)f, &png_write_data, NULL);

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        fprintf(stderr, "PNG write error\n");
        exit(1);
    }

    if (img_info->has_alpha)
        color_type = PNG_COLOR_TYPE_RGB_ALPHA;
    else
        color_type = PNG_COLOR_TYPE_RGB;
        
    png_set_IHDR(png_ptr, info_ptr, img_info->width, img_info->height,
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

#if __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
    if (bit_depth == 16) {
        png_set_swap(png_ptr);
    }
#endif
    
    if (bit_depth == 16) {
        if (img_info->has_alpha)
            out_fmt = BPG_OUTPUT_FORMAT_RGBA64;
        else
            out_fmt = BPG_OUTPUT_FORMAT_RGB48;
    } else {
        if (img_info->has_alpha)
            out_fmt = BPG_OUTPUT_FORMAT_RGBA32;
        else
            out_fmt = BPG_OUTPUT_FORMAT_RGB24;
    }
    
    bpg_decoder_start(img, out_fmt);

    bpp = (3 + img_info->has_alpha) * (bit_depth / 8);
    row_pointer = (png_bytep)png_malloc(png_ptr, img_info->width * bpp);
    for (y = 0; y < img_info->height; y++) {
        bpg_decoder_get_line(img, row_pointer);
        png_write_row(png_ptr, row_pointer);
    }
    png_free(png_ptr, row_pointer);
    
    png_write_end(png_ptr, NULL);
    
    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(f);
}
#endif /* USE_PNG */

int main_test(int argc, char **argv)
{
    FILE *f;
    BPGDecoderContext *img;
    uint8_t *buf;
    int buf_len, bit_depth;
    const char *outfilename, *filename, *p = NULL;
    
    outfilename = "out.bmp";
    bit_depth = 8;
    filename = "out.bpg";

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = malloc(buf_len);
    if (fread(buf, 1, buf_len, f) != buf_len) {
        fprintf(stderr, "Error while reading file\n");
        exit(1);
    }
    
    fclose(f);

    img = bpg_decoder_open();

    if (bpg_decoder_decode(img, buf, buf_len) < 0) {
        fprintf(stderr, "Could not decode image\n");
        exit(1);
    }
    free(buf);

#ifdef USE_PNG

    p = strstr(outfilename, ".");
    if (p)
        p++;

    if (p && strcmp(p, "ppm") != 0) {
        if (p && strcmp(p, "bmp") != 0) {
            png_save(img, outfilename, bit_depth);
        }
        bmp_save(img, outfilename);
    }  else 
#endif
    {
        ppm_save(img, outfilename);
    }

    bpg_decoder_close(img);

    return 0;
}
