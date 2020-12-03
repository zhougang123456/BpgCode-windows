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

#include "libbpg.h"
#include "windows.h"

static void bmp_save(BPGDecoderContext *img, const char *filename)
{
    BPGImageInfo img_info_s, *img_info = &img_info_s;
    FILE *f;
    int w, h, y;
    uint8_t *rgb_line;

    bpg_decoder_get_info(img, img_info);
    
    w = img_info->width;
    h = img_info->height;

    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "%s: I/O error\n", filename);
        exit(1);
    }

    BITMAPFILEHEADER bmheader;
    memset(&bmheader, 0, sizeof(bmheader));
    bmheader.bfType = 0x4d42;     //图像格式。必须为'BM'格式。  
    bmheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); //从文件开头到数据的偏移量  
    bmheader.bfSize = w * h * 4 + bmheader.bfOffBits;//文件大小  
    fwrite(&bmheader, sizeof(BITMAPFILEHEADER), 1, f);

    BITMAPINFOHEADER bmInfo;
    memset(&bmInfo, 0, sizeof(bmInfo));
    bmInfo.biSize = sizeof(bmInfo);
    bmInfo.biWidth = w;
    bmInfo.biHeight = h;
    bmInfo.biPlanes = 1;
    bmInfo.biBitCount = 32;
    bmInfo.biCompression = BI_RGB;
    fwrite(&bmInfo, sizeof(BITMAPINFOHEADER), 1, f);

    rgb_line = malloc(4 * w);    
    bpg_decoder_start(img, BPG_OUTPUT_FORMAT_RGBA32);
    for (y = 0; y < h; y++) {
        bpg_decoder_get_line(img, rgb_line);
        fwrite(rgb_line, 1, w * 4, f);
    }
    fclose(f);

    free(rgb_line);
}

static inline uint64_t get_time()
{
    time_t clock;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    uint64_t sec = wtm.wSecond;
    uint64_t sec_usec = wtm.wMilliseconds * 1000;
    return (uint64_t)sec * 1000000 + (uint64_t)sec_usec;
}

int main_test(int argc, char **argv)
{
    FILE *f;
    BPGDecoderContext *img;
    uint8_t *buf;
    int buf_len, bit_depth, c, show_info;
    const char *outfilename, *filename, *p;
    
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

    uint64_t start = get_time();
    if (bpg_decoder_decode(img, buf, buf_len) < 0) {
        fprintf(stderr, "Could not decode image\n");
        exit(1);
    }
    free(buf);
    uint64_t time = get_time() - start;
    printf("encode time once is %u ms\n", time / 1000);

    bmp_save(img, outfilename);

    bpg_decoder_close(img);

    return 0;
}
