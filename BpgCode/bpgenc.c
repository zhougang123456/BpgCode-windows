
#include "windows.h"
#include "stdio.h"
#include "winsock.h"
#include "bpgenc.h"
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define COMPRESS_BUF_SIZE (1024 * 64)

typedef struct CompressBuf CompressBuf;

struct CompressBuf {
    CompressBuf* send_next;
    union {
        UINT8  bytes[COMPRESS_BUF_SIZE];
    } buf;
};

typedef struct EncoderData {
    CompressBuf* bufs_head;
    CompressBuf* bufs_tail;
    int buf_size;
}EncoderData;

static void more_buffer(BPGEncoderContext * enc_ctx)
{   
    EncoderData* enc_data = (EncoderData*)enc_ctx->client_data;
    CompressBuf* buf = (CompressBuf*)malloc(sizeof(CompressBuf));
    enc_data->bufs_tail->send_next = buf;
    enc_data->bufs_tail = buf;
    buf->send_next = NULL;
    enc_ctx->output->out_buf = buf->buf.bytes;
    enc_ctx->output->buf_free_size = sizeof(buf->buf);
    enc_data->buf_size += enc_ctx->output->buf_free_size;
    printf("buffer size is %d\n", enc_data->buf_size);

}

static void init_buffer(BPGEncoderContext* enc_ctx)
{
    EncoderData* enc_data = (EncoderData*)enc_ctx->client_data;
    CompressBuf* buf = (CompressBuf*)malloc(sizeof(CompressBuf));
    enc_data->bufs_tail = buf;
    enc_data->bufs_head = enc_data->bufs_tail;
    enc_data->bufs_head->send_next = NULL;
    enc_ctx->output->out_buf = buf->buf.bytes;
    enc_ctx->output->buf_free_size = sizeof(buf->buf);
    enc_data->buf_size = enc_ctx->output->buf_free_size;
    printf("buffer size is %d\n", enc_data->buf_size);

}

static void term_buffer(BPGEncoderContext* enc_ctx)
{
    EncoderData* enc_data = (EncoderData*)enc_ctx->client_data;
    enc_data->buf_size -= enc_ctx->output->buf_free_size;
    printf("buffer size is %d\n", enc_data->buf_size);

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
int main_test(int argc, char** argv)
{
    const char* infilename, * outfilename, * frame_delay_file;
    FILE* f;
    int c, option_index;
    int keep_metadata;
    int bit_depth, i, limited_range, premultiplied_alpha;
    BPGColorSpaceEnum color_space;
    BPGMetaData* md;
    BPGEncoderContext* enc_ctx;

    outfilename = "out.bpg";
    color_space = BPG_CS_YCbCr;
    keep_metadata = 0;
    bit_depth = 8;
    limited_range = 0;
    premultiplied_alpha = 0;
    frame_delay_file = NULL;

    infilename = "out.bmp";

    f = fopen(outfilename, "wb");
    if (!f) {
        perror(outfilename);
        exit(1);
    }
    enc_ctx = bpg_encoder_open(35, 9);

    BPGEncoderOutput output;
    output.buf_free_size = 0;
    output.out_buf = NULL;
    output.init_output_buffer = init_buffer;
    output.more_output_buffer = more_buffer;
    output.term_output_buffer = term_buffer;
    enc_ctx->output = &output;
    EncoderData* enc_data = (EncoderData * )malloc(sizeof(EncoderData));
    enc_ctx->client_data = enc_data;
    if (!enc_ctx) {
        fprintf(stderr, "Could not open BPG encoder\n");
        exit(1);
    }

    int has_alpha = 1;
    FILE* fp;
    if ((fp = fopen(infilename, "rb")) == NULL)  //以二进制的方式打开文件
    {

        return FALSE;
    }
    if (fseek(fp, sizeof(BITMAPFILEHEADER), 0))  //跳过BITMAPFILEHEADE
    {
        return FALSE;
    }
    BITMAPINFOHEADER infoHead;
    fread(&infoHead, sizeof(BITMAPINFOHEADER), 1, fp);   //从fp中读取BITMAPINFOHEADER信息到infoHead中,同时fp的指针移动
    int bmpwidth = infoHead.biWidth;

    int bmpheight = infoHead.biHeight;

    if (bmpheight < 0) {
        bmpheight *= -1;
    }
    int linebyte = bmpwidth * 4; //计算每行的字节数，24：该图片是24位的bmp图，3：确保不丢失像素

    unsigned char* pBmpBuf = (unsigned char*)malloc(linebyte * bmpheight);

    fread(pBmpBuf, sizeof(char), linebyte * bmpheight, fp);

    bpg_encoder_start(enc_ctx, pBmpBuf, bmpwidth, bmpheight, 8, 1);

    if (!enc_ctx->img) {
        fprintf(stderr, "Could not read '%s'\n", infilename);
        exit(1);
    }
    printf("encoder start!");

    uint64_t start = get_time();

    bpg_encoder_encode(enc_ctx);

    uint64_t time = get_time() - start;
    printf("encode time once is %u ms\n", time / 1000);

    int max = enc_data->buf_size;
    CompressBuf* buf = enc_data->bufs_head;
    size_t now;
    do {
        if (buf == NULL) {
            break;
        }
        now = MIN(sizeof(buf->buf), max);
        max -= now;
        fwrite(buf->buf.bytes, 1, now, f);
        
        buf = buf->send_next;
    } while (max);

    fclose(f);

    bpg_encoder_close(enc_ctx);
    printf("encoder stop!");

    return 0;
}