//
// Created by dandy on 2021/6/1.
//

#ifndef WEBRTC_FFPLAYER_H
#define WEBRTC_FFPLAYER_H

#include <string>
#include "rtc_base/platform_thread.h"
#include "rtc_base/event.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>

#include "queue.h"
#include "clock.h"
}

typedef struct Decoder {
    AVPacket *pkt;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    pthread_cond_t empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    pthread_t decoder_tid;
} Decoder;

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;

class FileBroadCaster {

public:
    FileBroadCaster(const std::string& input,int64_t start_time = 0);
    ~FileBroadCaster();

public:
    int start(int loop = 1);
    int pause();
    int destory();
    int seek(int64_t pos, int64_t rel, int seek_by_bytes = 0);

private:
    int stream_open();
    int stream_component_open(int stream_index);
    void read();

private:
    std::string mInputFile;

    struct AudioParams audio_tgt;
    struct AudioParams audio_src;
    int audio_hw_buf_size;
    unsigned int audio_buf_size; /* in bytes */
    int audio_buf_index; /* in bytes */

    double audio_diff_threshold;
    double audio_diff_cum; /* used for AV difference average computation */
    int audio_diff_avg_count;
    double audio_diff_avg_coef;

    AVFormatContext *ctx{NULL};

    rtc::PlatformThread mReadThread;
    rtc::Event mReadEvent;

    int64_t duration{AV_NOPTS_VALUE};

    int audio_clock_serial;
    double max_frame_duration;
    int64_t start_time{AV_NOPTS_VALUE};
    int video_stream{-1};
    int audio_stream{-1};

    AVStream *video_st{NULL};
    AVStream *audio_st{NULL};

    Decoder auddec;
    Decoder viddec;

    int seek_req{0};
    int seek_pos{0};
    int seek_rel{0};
    int seek_flags{0};
    int eof{0};

    int loop{1};

    int queue_attachments_req{0};

    Clock mAudclk;
    Clock mVidclk;

    pthread_cond_t mReadCond;

    PacketQueue mVideoq;
    PacketQueue mAudioq;

    FrameQueue mPictq;
    FrameQueue mSampq;
};


#endif //WEBRTC_FFPLAYER_H
