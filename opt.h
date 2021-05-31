//
// Created by heyafei on 2021/6/1.
//

#ifndef BJY_OPT_H
#define BJY_OPT_H

#include <SDL.h>
#include <libavutil/avassert.h>
#include <libavutil/bprint.h>
#include <libavutil/opt.h>

#include "cmdutils.h"

extern const OptionDef options[];

/* current context */
extern int is_full_screen;

extern int audio_disable;
extern int video_disable;
extern int subtitle_disable;
extern int display_disable;

extern int seek_by_bytes;
extern float seek_interval;

extern int borderless;
extern int alwaysontop;
extern int startup_volume;
extern int show_status;

extern int fast;
extern int genpts;
extern int lowres;

extern int decoder_reorder_pts;
extern int autoexit;
extern int exit_on_keydown;
extern int exit_on_mousedown;
extern int loop;
extern int framedrop;
extern int infinite_buffer;

extern const char *audio_codec_name;
const char *subtitle_codec_name;
const char *video_codec_name;

extern char *input_filename;
extern const char *window_title;

extern int screen_width;
extern int screen_height;
extern int screen_left;
extern int screen_top;

extern double rdftspeed;

extern int autorotate;
extern int find_stream_info;
extern int filter_nbthreads;

extern const char* wanted_stream_spec[AVMEDIA_TYPE_NB];

extern int dummy;

extern enum ShowMode show_mode;

/* options specified by the user */
extern const AVInputFormat *file_iformat;
extern int av_sync_type;

extern int64_t start_time;
extern int64_t duration;

#if CONFIG_AVFILTER
extern const char **vfilters_list;
extern int nb_vfilters;
extern char *afilters;
#endif

enum ShowMode {
    SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
};

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

void parse_loglevel(int argc, char **argv, const OptionDef *options);

void opt_input_file(void *optctx, const char *filename);

void show_usage(void);

void show_help_default(const char *opt, const char *arg);

#endif //BJY_OPT_H
