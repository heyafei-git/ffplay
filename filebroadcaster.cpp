//
// Created by dandy on 2021/6/1.
//

#include "filebroadcaster.h"

#define MIN_FRAMES 25
#define MAX_QUEUE_SIZE (15 * 1024 * 1024)

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

FileBroadCaster::FileBroadCaster(const std::string &input,int64_t start_time) : mInputFile(input),start_time(start_time) {
}

FileBroadCaster::~FileBroadCaster() {

    if (!mReadThread.empty())
        mReadThread.Finalize();
}

int FileBroadCaster::start(int loop) {
    
    this->loop = loop;

    return stream_open();
}

int FileBroadCaster::pause() {

    return 0;
}

int FileBroadCaster::destory() {

    return 0;
}

int FileBroadCaster::seek(int64_t pos, int64_t rel, int seek_by_bytes) {

    if (!seek_req) {
        seek_pos = pos;
        seek_rel = rel;
        seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            seek_flags |= AVSEEK_FLAG_BYTE;
        seek_req = 1;
        mReadEvent.Set();
    }

    return 0;
}

int FileBroadCaster::stream_open() {

    if (frame_queue_init(&mPictq, &mVideoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0) return -1;
    if (frame_queue_init(&mSampq, &mAudioq, SAMPLE_QUEUE_SIZE, 1) < 0) return -1;

    if (packet_queue_init(&mVideoq) < 0 ||
        packet_queue_init(&mAudioq) < 0) {
        return -1;
    }

    pthread_cond_init(&mReadCond,NULL);

    init_clock(&mAudclk, &mVideoq.serial);
    init_clock(&mVidclk, &mVideoq.serial);

    audio_clock_serial = 1;

    mReadThread = rtc::PlatformThread::SpawnJoinable(
            [this] { read();},
            "filebroadcast_read_thread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kNormal));

    return 0;

}

static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           (queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0));
}

void FileBroadCaster::read() {

    int err,ret;
    AVPacket *pkt = NULL;
    int st_index[AVMEDIA_TYPE_NB];

    int pkt_in_play_range = 0;
    int64_t pkt_ts;
    int64_t stream_start_time;

    memset(st_index, -1, sizeof(st_index));

    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate packet.\n");
        goto fail;
    }

    ctx = avformat_alloc_context();
    if (!ctx) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate ctx.\n");
        goto fail;
    }

    err = avformat_open_input(&ctx,mInputFile.data(),NULL,NULL);
    if (err < 0) {
        goto fail;
    }

    err = avformat_find_stream_info(ctx, NULL);
    if (err < 0) {
        av_log(NULL, AV_LOG_WARNING,
               "%s: could not find codec parameters\n", mInputFile.data());
        goto fail;
    }

    if (ctx->pb)
        ctx->pb->eof_reached = 0;

    max_frame_duration = (ctx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    /* if seeking requested, we execute it */
    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;
        timestamp = start_time;
        ret = avformat_seek_file(ctx, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                   mInputFile.data(), (double)timestamp / AV_TIME_BASE);
        }
    }

    av_dump_format(ctx, 0, mInputFile.data(), 0);

    st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);

    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
    }

    if (video_stream < 0 && audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
               mInputFile.data());
        ret = -1;
        goto fail;
    }

    for (;;) {

        if (seek_req) {
            int64_t seek_target = seek_pos;
            int64_t seek_min    = seek_rel > 0 ? seek_target - seek_rel + 2: INT64_MIN;
            int64_t seek_max    = seek_rel < 0 ? seek_target - seek_rel - 2: INT64_MAX;

            ret = avformat_seek_file(ctx, -1, seek_min, seek_target, seek_max, 0);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", ctx->url);
            } else {
                if (audio_stream >= 0)
                    packet_queue_flush(&mAudioq);
                if (video_stream >= 0)
                    packet_queue_flush(&mVideoq);
            }
            seek_req = 0;
            queue_attachments_req = 1;
            eof = 0;
        }
        if (queue_attachments_req) {
            if (video_st && video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&mVideoq, pkt);
                packet_queue_put_nullpacket(&mVideoq, pkt, video_stream);
            }
            queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if ((mAudioq.size + mVideoq.size  > MAX_QUEUE_SIZE
             || (stream_has_enough_packets(audio_st, audio_stream, &mAudioq) &&
                 stream_has_enough_packets(video_st, video_stream, &mVideoq) ))) {
            /* wait 10 ms */
            mReadEvent.Wait(10);
            continue;
        }
        if ((!audio_st || (auddec.finished == mAudioq.serial && frame_queue_nb_remaining(&mSampq) == 0)) &&
            (!video_st || (viddec.finished == mVideoq.serial && frame_queue_nb_remaining(&mPictq) == 0))) {
            if (loop != 1 && (!loop || --loop)) {
                seek(start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
            } else {
                ret = AVERROR_EOF;
                break;
            }
        }
        ret = av_read_frame(ctx, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ctx->pb)) && !eof) {
                if (video_stream >= 0)
                    packet_queue_put_nullpacket(&mVideoq, pkt, video_stream);
                if (audio_stream >= 0)
                    packet_queue_put_nullpacket(&mAudioq, pkt, audio_stream);
                eof = 1;
            }
            if (ctx->pb && ctx->pb->error) {
                break;
            }
            // wait 10 ms
            mReadEvent.Wait(10); 
            continue;
        } else {
            eof = 0;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        stream_start_time = ctx->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                            av_q2d(ctx->streams[pkt->stream_index]->time_base) -
                            (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000 <= ((double)duration / 1000000);
        if (pkt->stream_index == audio_stream && pkt_in_play_range) {
            packet_queue_put(&mAudioq, pkt);
        } else if (pkt->stream_index == video_stream && pkt_in_play_range
                   && !(video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&mVideoq, pkt);
        } else {
            av_packet_unref(pkt);
        }
    }

    fail:
    if (ctx)
        avformat_close_input(&ctx);

    av_packet_free(&pkt);
}

int FileBroadCaster::stream_component_open(int stream_index) {

    AVCodecContext *avctx;
    const AVCodec *codec;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;

    if (stream_index < 0 || (unsigned int )stream_index >= ctx->nb_streams)
        return -1;

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, ctx->streams[stream_index]->codecpar);
    if (ret < 0) goto fail;

    avctx->pkt_timebase = ctx->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    if (!codec) {
        av_log(NULL, AV_LOG_WARNING,
               "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    avctx->flags2 |= AV_CODEC_FLAG2_FAST;

    eof = 0;
    ctx->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:

            sample_rate    = avctx->sample_rate;
            nb_channels    = avctx->channels;
            channel_layout = avctx->channel_layout;

            /* prepare audio output */
            if ((ret = audio_open(channel_layout, nb_channels, sample_rate, &audio_tgt)) < 0)
                goto fail;
            audio_hw_buf_size = ret;
            audio_src = audio_tgt;
            audio_buf_size  = 0;
            audio_buf_index = 0;

            /* init averaging filter */
            audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
            audio_diff_avg_count = 0;
            /* since we do not have a precise anough audio FIFO fullness,
               we correct audio sync only if larger than this threshold */
            audio_diff_threshold = (double)(audio_hw_buf_size) / audio_tgt.bytes_per_sec;

            audio_stream = stream_index;
            audio_st = ctx->streams[stream_index];

            if ((ret = decoder_init(&auddec, avctx, &mAudioq, mReadEvent)) < 0)
                goto fail;
            if ((ctx->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !ctx->iformat->read_seek) {
                auddec.start_pts = audio_st->start_time;
                auddec.start_pts_tb = audio_st->time_base;
            }
            if ((ret = decoder_start(&auddec, audio_thread, "audio_decoder", is)) < 0)
                goto out;
            break;
        case AVMEDIA_TYPE_VIDEO:
            video_stream = stream_index;
            video_st = ctx->streams[stream_index];

            if ((ret = decoder_init(&viddec, avctx, &mVideoq, mReadEvent)) < 0)
                goto fail;
            if ((ret = decoder_start(&viddec, video_thread, "video_decoder")) < 0)
                goto out;
            queue_attachments_req = 1;
            break;
        default:
            break;
    }
    goto out;

    fail:
    avcodec_free_context(&avctx);
    out:
    av_dict_free(&opts);

    return ret;
}
