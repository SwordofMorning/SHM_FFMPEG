#include <chrono>
#include <thread>
#include <iostream>
extern "C"
{
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdint.h>

using namespace std;

static int video_is_eof;

#define STREAM_FRAME_RATE 30
#define GOP_SIZE 4
#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */
#define VIDEO_CODEC_ID AV_CODEC_ID_H264

static uint8_t *yuv_buffer = NULL;

#define SHM_KEY 1234  
#define SEM_KEY 5678

static int shmid;
static int semid; 

/* video output */
static AVFrame *frame;
static AVPicture src_picture, dst_picture;

/* Add an output stream. */
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id)
{
    AVCodecContext *c;
    AVStream *st;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        av_log(NULL, AV_LOG_ERROR, "Could not find encoder for '%s'.\n", avcodec_get_name(codec_id));
    }
    else {
        st = avformat_new_stream(oc, *codec);
        if (!st) {
            av_log(NULL, AV_LOG_ERROR, "Could not allocate stream.\n");
        }
        else {
            st->id = oc->nb_streams - 1;
            st->time_base.den = STREAM_FRAME_RATE;
            st->time_base.num = 1;

            c = st->codec;
            c->codec_id = codec_id;
            c->bit_rate = 16000000;
            c->width = 640;
            c->height = 512;
            c->time_base.den = STREAM_FRAME_RATE;
            c->time_base.num = 1;
            c->gop_size = GOP_SIZE; /* with out inter frame, only have intra frame */
            c->max_b_frames = 1;
            c->pix_fmt = STREAM_PIX_FMT;
        }
    }

    return st;
}

static int open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st)
{
    int ret;
    AVCodecContext *c = st->codec;

    /* open the codec */
    AVDictionary *param = NULL;
    // av_dict_set(&param, "preset", "ultrafast", 0);
    // av_dict_set(&param, "tune", "zerolatency", 0);
    ret = avcodec_open2(c, codec, &param);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open video codec.\n", avcodec_get_name(c->codec_id));
    }
    else {

        /* allocate and init a re-usable frame */
        frame = av_frame_alloc();
        if (!frame) {
            av_log(NULL, AV_LOG_ERROR, "Could not allocate video frame.\n");
            ret = -1;
        }
        else {
            frame->format = c->pix_fmt;
            frame->width = c->width;
            frame->height = c->height;

            /* Allocate the encoded raw picture. */
            ret = avpicture_alloc(&dst_picture, c->pix_fmt, c->width, c->height);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Could not allocate picture.\n");
            }
            else {
                /* copy data and linesize picture pointers to frame */
                *((AVPicture *)frame) = dst_picture;
            }
        }
    }

    return ret;
}

static void fill_av_frame(AVFrame *frame, int width, int height) 
{
    struct sembuf sem_op;
    
    // 等待信号量
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(semid, &sem_op, 1);
    
    // 从共享内存复制YUV数据到AVFrame
    for (int y = 0; y < height; y++) {
        memcpy(frame->data[0] + y * frame->linesize[0],
               yuv_buffer + y * width, 
               width);
    }
    
    for (int y = 0; y < height / 2; y++) {
        memcpy(frame->data[1] + y * frame->linesize[1],
               yuv_buffer + width * height + y * width / 2,
               width / 2);
    }
    
    for (int y = 0; y < height / 2; y++) {
        memcpy(frame->data[2] + y * frame->linesize[2],
               yuv_buffer + width * height * 5 / 4 + y * width / 2, 
               width / 2);
    }
    
    // 释放信号量 
    sem_op.sem_num = 0;
    sem_op.sem_op = 1; 
    sem_op.sem_flg = 0;
    semop(semid, &sem_op, 1);
}

static int write_video_frame(AVFormatContext *oc, AVStream *st, int64_t frameCount)
{
    int ret = 0;
    AVCodecContext *c = st->codec;
    
    // 填充AVFrame
    fill_av_frame(frame, c->width, c->height); 

    AVPacket pkt = { 0 };
    int got_packet;
    av_init_packet(&pkt);

    /* encode the image */
    frame->pts = frameCount;
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error encoding video frame.\n");
    }
    else {
        if (got_packet) {
            pkt.stream_index = st->index;
            pkt.pts = av_rescale_q_rnd(pkt.pts, c->time_base, st->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            ret = av_write_frame(oc, &pkt);

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while writing video frame.\n");
            }
        }
    }

    av_usleep(1000000 / STREAM_FRAME_RATE);

    return ret;
}

int main(int argc, char* argv[])
{
    printf("starting...\n");

    // 分配yuv缓冲区    
    int video_width = 640;
    int video_height = 512;
    int yuv_buffer_size = video_width * video_height * 3 / 2;
    yuv_buffer = (uint8_t*)malloc(yuv_buffer_size);

    // 创建共享内存
    shmid = shmget(SHM_KEY, yuv_buffer_size, IPC_CREAT | 0666);
    yuv_buffer = (uint8_t *)shmat(shmid, NULL, 0);
   
    // 获取信号量
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);

    const char *url = "rtsp://127.0.0.1:8554/stream";

    AVFormatContext *outContext;
    AVStream *video_st;
    AVCodec *video_codec;
    int ret = 0;
    int64_t frameCount = 0;

    av_log_set_level(AV_LOG_WARNING);

    av_register_all();
    avformat_network_init();

    avformat_alloc_output_context2(&outContext, NULL, "rtsp", url);

    if (!outContext) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate an output context for '%s'.\n", url);
    }

    if (!outContext->oformat) {
        av_log(NULL, AV_LOG_FATAL, "Could not create the output format for '%s'.\n", url);
    }

    video_st = add_stream(outContext, &video_codec, VIDEO_CODEC_ID);

    /* Now that all the parameters are set, we can open the video codec and allocate the necessary encode buffers. */
    if (video_st) {
        av_log(NULL, AV_LOG_DEBUG, "Video stream codec %s.\n ", avcodec_get_name(video_st->codec->codec_id));

        ret = open_video(outContext, video_codec, video_st);
        if (ret < 0) {
            av_log(NULL, AV_LOG_FATAL, "Open video stream failed.\n");
        }
    }
    else {
        av_log(NULL, AV_LOG_FATAL, "Add video stream for the codec '%s' failed.\n", avcodec_get_name(VIDEO_CODEC_ID));
    }

    av_dump_format(outContext, 0, url, 1);

    ret = avformat_write_header(outContext, NULL);
    if (ret != 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to connect to RTSP server for '%s'.\n", url);
    }

    while (video_st) {
        frameCount++;

        ret = write_video_frame(outContext, video_st, frameCount);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Write video frame failed.\n", url);
            goto end;
        }
    }

    if (video_st) {
        avcodec_close(video_st->codec);
        av_free(src_picture.data[0]);
        av_free(dst_picture.data[0]);
        av_frame_free(&frame);
    }

    avformat_free_context(outContext);

end:
    printf("finished.\n");
    free(yuv_buffer);

    getchar();

    return 0;
}