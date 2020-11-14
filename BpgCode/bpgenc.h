/*
 * BPG encoder
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
#ifdef __cplusplus
extern "C" {
#endif

#include "libbpg.h"

typedef struct {
    int w, h;
    BPGImageFormatEnum format; /* x_VIDEO values are forbidden here */
    uint8_t c_h_phase; /* 4:2:2 or 4:2:0 : give the horizontal chroma
                          position. 0=MPEG2, 1=JPEG. */
    uint8_t has_alpha;
    uint8_t has_w_plane;
    uint8_t limited_range;
    uint8_t premultiplied_alpha;
    BPGColorSpaceEnum color_space;
    uint8_t bit_depth;
    uint8_t pixel_shift; /* (1 << pixel_shift) bytes per pixel */
    uint8_t *data[4];
    int linesize[4];
} Image;

typedef struct {
    int width;
    int height;
    int chroma_format; /* 0-3 */
    int bit_depth; /* 8-14 */
    int intra_only; /* 0-1 */

    int qp; /* quantizer 0-51 */
    int lossless; /* 0-1 lossless mode */
    int sei_decoded_picture_hash; /* 0=no hash, 1=MD5 hash */
    int compress_level; /* 1-9 */
    int verbose;
} HEVCEncodeParams;

typedef struct HEVCEncoderContext HEVCEncoderContext;

typedef struct BPGEncoderOutput {
    uint8_t* out_buf;
    int      buf_free_size;
    void    (*init_output_buffer) (void* ctx);
    void    (*more_output_buffer) (void* ctx);
    void    (*term_output_buffer) (void* ctx);
}BPGEncoderOutput;

typedef struct {
    HEVCEncoderContext *(*open)(const HEVCEncodeParams *params);
    int (*encode)(HEVCEncoderContext *s, Image *img);
    int (*close)(HEVCEncoderContext *s, uint8_t **pbuf);
} HEVCEncoder;

#define USE_X265
typedef enum {
#if defined(USE_X265)
    HEVC_ENCODER_X265,
#endif
#if defined(USE_JCTVC)
    HEVC_ENCODER_JCTVC,
#endif

    HEVC_ENCODER_COUNT,
} HEVCEncoderEnum;

typedef struct BPGEncoderParameters {
    int qp; /* 0 ... 51 */
    int alpha_qp; /* -1 ... 51. -1 means same as qp */
    int lossless; /* true if lossless compression (qp and alpha_qp are
                     ignored) */
    BPGImageFormatEnum preferred_chroma_format;
    int sei_decoded_picture_hash; /* 0, 1 */
    int compress_level; /* 1 ... 9 */
    int verbose;
    HEVCEncoderEnum encoder_type;
    int animated; /* 0 ... 1: if true, encode as animated image */
    uint16_t loop_count; /* animations: number of loops. 0=infinite */
    /* animations: the frame delay is a multiple of
       frame_delay_num/frame_delay_den seconds */
    uint16_t frame_delay_num;
    uint16_t frame_delay_den;
} BPGEncoderParameters;

typedef struct BPGMetaData {
    uint32_t tag;
    uint8_t* buf;
    int buf_len;
    struct BPGMetaData* next;
} BPGMetaData;

typedef struct BPGEncoderContext {
    void* client_data;
    BPGEncoderOutput* output;
    BPGEncoderParameters params;
    BPGMetaData* first_md;
    HEVCEncoder* encoder;
    int frame_count;
    HEVCEncoderContext* enc_ctx;
    HEVCEncoderContext* alpha_enc_ctx;
    int frame_ticks;
    uint16_t* frame_duration_tab;
    int frame_duration_tab_size;
    Image* img;
}BPGEncoderContext;

extern HEVCEncoder jctvc_encoder;
extern HEVCEncoder x265_hevc_encoder;

void save_yuv1(Image *img, FILE *f);
void save_yuv(Image *img, const char *filename);

BPGEncoderContext* bpg_encoder_open(int qp, int compress_level);
int bpg_encoder_encode(BPGEncoderContext* s);
void bpg_encoder_start(BPGEncoderContext* enc_ctx, uint8_t* buf, int width, int height, int bit_depth, int has_alpha);
void bpg_encoder_close(BPGEncoderContext* s);


#ifdef __cplusplus
}
#endif

