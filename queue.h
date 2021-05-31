//
// Created by heyafei on 2021/6/1.
//

#ifndef BJY_QUEUE_H
#define BJY_QUEUE_H

#include <libavcodec/avcodec.h>
#include <libavutil/fifo.h>
#include <SDL_thread.h>

typedef struct MyAVPacketList {
    AVPacket *pkt;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    AVFifoBuffer *pkt_list;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
} Frame;

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex *mutex;
    SDL_cond *cond;
    PacketQueue *pktq;
} FrameQueue;


// packet queue
int packet_queue_init(PacketQueue *q);

void packet_queue_flush(PacketQueue *q);

void packet_queue_destroy(PacketQueue *q);

void packet_queue_abort(PacketQueue *q);

void packet_queue_start(PacketQueue *q);

int packet_queue_put_nullpacket(PacketQueue *q, AVPacket *pkt, int stream_index);

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);

int packet_queue_put(PacketQueue *q, AVPacket *pkt);

int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);

// frame queue

void frame_queue_unref_item(Frame *vp);

int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);

void frame_queue_destory(FrameQueue *f);

void frame_queue_signal(FrameQueue *f);

Frame *frame_queue_peek(FrameQueue *f);

Frame *frame_queue_peek_next(FrameQueue *f);

Frame *frame_queue_peek_last(FrameQueue *f);

Frame *frame_queue_peek_writable(FrameQueue *f);
//
Frame *frame_queue_peek_readable(FrameQueue *f);
//
void frame_queue_push(FrameQueue *f);
//
void frame_queue_next(FrameQueue *f);
//
int frame_queue_nb_remaining(FrameQueue *f);
//
int64_t frame_queue_last_pos(FrameQueue *f);

#endif //BJY_QUEUE_H
