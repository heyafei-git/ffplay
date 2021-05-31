//
// Created by heyafei on 2021/6/1.
//

#include "opt.h"

/* current context */
int is_full_screen;

int audio_disable;
int video_disable;
int subtitle_disable;
int display_disable;

int seek_by_bytes = -1;
float seek_interval = 10;

int borderless;
int alwaysontop;
int startup_volume = 100;
int show_status = -1;

int fast = 0;
int genpts = 0;
int lowres = 0;

int decoder_reorder_pts = -1;
int autoexit;
int exit_on_keydown;
int exit_on_mousedown;
int loop = 1;
int framedrop = -1;
int infinite_buffer = -1;

const char *audio_codec_name;
const char *subtitle_codec_name;
const char *video_codec_name;

char *input_filename;
const char *window_title;
int screen_width  = 0;
int screen_height = 0;
int screen_left = SDL_WINDOWPOS_CENTERED;
int screen_top = SDL_WINDOWPOS_CENTERED;

double rdftspeed = 0.02;

int autorotate = 1;
int find_stream_info = 1;
int filter_nbthreads = 0;

const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};

int dummy;

enum ShowMode show_mode = SHOW_MODE_NONE;

/* options specified by the user */
const AVInputFormat *file_iformat;
int av_sync_type = AV_SYNC_AUDIO_MASTER;

int64_t start_time = AV_NOPTS_VALUE;
int64_t duration = AV_NOPTS_VALUE;

// static
static FILE *report_file;
static int report_file_level = AV_LOG_DEBUG;


// func
static int opt_frame_size(void *optctx, const char *opt, const char *arg)
{
    av_log(NULL, AV_LOG_WARNING, "Option -s is deprecated, use -video_size.\n");
    return opt_default(NULL, "video_size", arg);
}

static int opt_width(void *optctx, const char *opt, const char *arg)
{
    screen_width = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

static int opt_height(void *optctx, const char *opt, const char *arg)
{
    screen_height = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

static int opt_format(void *optctx, const char *opt, const char *arg)
{
    file_iformat = av_find_input_format(arg);
    if (!file_iformat) {
        av_log(NULL, AV_LOG_FATAL, "Unknown input format: %s\n", arg);
        return AVERROR(EINVAL);
    }
    return 0;
}

static int opt_frame_pix_fmt(void *optctx, const char *opt, const char *arg)
{
    av_log(NULL, AV_LOG_WARNING, "Option -pix_fmt is deprecated, use -pixel_format.\n");
    return opt_default(NULL, "pixel_format", arg);
}

static int opt_sync(void *optctx, const char *opt, const char *arg)
{
    if (!strcmp(arg, "audio"))
        av_sync_type = AV_SYNC_AUDIO_MASTER;
    else if (!strcmp(arg, "video"))
        av_sync_type = AV_SYNC_VIDEO_MASTER;
    else if (!strcmp(arg, "ext"))
        av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
    else {
        av_log(NULL, AV_LOG_ERROR, "Unknown value for %s: %s\n", opt, arg);
        exit(1);
    }
    return 0;
}

static int opt_seek(void *optctx, const char *opt, const char *arg)
{
    start_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_duration(void *optctx, const char *opt, const char *arg)
{
    duration = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_show_mode(void *optctx, const char *opt, const char *arg)
{
    show_mode = !strcmp(arg, "video") ? SHOW_MODE_VIDEO :
                !strcmp(arg, "waves") ? SHOW_MODE_WAVES :
                !strcmp(arg, "rdft" ) ? SHOW_MODE_RDFT  :
                parse_number_or_die(opt, arg, OPT_INT, 0, SHOW_MODE_NB-1);
    return 0;
}

void opt_input_file(void *optctx, const char *filename)
{
    if (input_filename) {
        av_log(NULL, AV_LOG_FATAL,
               "Argument '%s' provided as input filename, but '%s' was already specified.\n",
               filename, input_filename);
        exit(1);
    }
    if (!strcmp(filename, "-"))
        filename = "pipe:";
    input_filename = filename;
}

static int opt_codec(void *optctx, const char *opt, const char *arg)
{
    const char *spec = strchr(opt, ':');
    if (!spec) {
        av_log(NULL, AV_LOG_ERROR,
               "No media specifier was specified in '%s' in option '%s'\n",
               arg, opt);
        return AVERROR(EINVAL);
    }
    spec++;
    switch (spec[0]) {
        case 'a' :    audio_codec_name = arg; break;
        case 's' : subtitle_codec_name = arg; break;
        case 'v' :    video_codec_name = arg; break;
        default:
            av_log(NULL, AV_LOG_ERROR,
                   "Invalid media specifier '%s' in option '%s'\n", spec, opt);
            return AVERROR(EINVAL);
    }
    return 0;
}

#if CONFIG_AVFILTER
const char **vfilters_list = NULL;
int nb_vfilters = 0;
char *afilters = NULL;
#endif

#if CONFIG_AVFILTER
int opt_add_vfilter(void *optctx, const char *opt, const char *arg)
{
    GROW_ARRAY(vfilters_list, nb_vfilters);
    vfilters_list[nb_vfilters - 1] = arg;
    return 0;
}
#endif

const OptionDef options[] = {
        CMDUTILS_COMMON_OPTIONS
        { "x", HAS_ARG, { .func_arg = opt_width }, "force displayed width", "width" },
        { "y", HAS_ARG, { .func_arg = opt_height }, "force displayed height", "height" },
        { "s", HAS_ARG | OPT_VIDEO, { .func_arg = opt_frame_size }, "set frame size (WxH or abbreviation)", "size" },
        { "fs", OPT_BOOL, { &is_full_screen }, "force full screen" },
        { "an", OPT_BOOL, { &audio_disable }, "disable audio" },
        { "vn", OPT_BOOL, { &video_disable }, "disable video" },
        { "sn", OPT_BOOL, { &subtitle_disable }, "disable subtitling" },
        { "ast", OPT_STRING | HAS_ARG | OPT_EXPERT, { &wanted_stream_spec[AVMEDIA_TYPE_AUDIO] }, "select desired audio stream", "stream_specifier" },
        { "vst", OPT_STRING | HAS_ARG | OPT_EXPERT, { &wanted_stream_spec[AVMEDIA_TYPE_VIDEO] }, "select desired video stream", "stream_specifier" },
        { "sst", OPT_STRING | HAS_ARG | OPT_EXPERT, { &wanted_stream_spec[AVMEDIA_TYPE_SUBTITLE] }, "select desired subtitle stream", "stream_specifier" },
        { "ss", HAS_ARG, { .func_arg = opt_seek }, "seek to a given position in seconds", "pos" },
        { "t", HAS_ARG, { .func_arg = opt_duration }, "play  \"duration\" seconds of audio/video", "duration" },
        { "bytes", OPT_INT | HAS_ARG, { &seek_by_bytes }, "seek by bytes 0=off 1=on -1=auto", "val" },
        { "seek_interval", OPT_FLOAT | HAS_ARG, { &seek_interval }, "set seek interval for left/right keys, in seconds", "seconds" },
        { "nodisp", OPT_BOOL, { &display_disable }, "disable graphical display" },
        { "noborder", OPT_BOOL, { &borderless }, "borderless window" },
        { "alwaysontop", OPT_BOOL, { &alwaysontop }, "window always on top" },
        { "volume", OPT_INT | HAS_ARG, { &startup_volume}, "set startup volume 0=min 100=max", "volume" },
        { "f", HAS_ARG, { .func_arg = opt_format }, "force format", "fmt" },
        { "pix_fmt", HAS_ARG | OPT_EXPERT | OPT_VIDEO, { .func_arg = opt_frame_pix_fmt }, "set pixel format", "format" },
        { "stats", OPT_BOOL | OPT_EXPERT, { &show_status }, "show status", "" },
        { "fast", OPT_BOOL | OPT_EXPERT, { &fast }, "non spec compliant optimizations", "" },
        { "genpts", OPT_BOOL | OPT_EXPERT, { &genpts }, "generate pts", "" },
        { "drp", OPT_INT | HAS_ARG | OPT_EXPERT, { &decoder_reorder_pts }, "let decoder reorder pts 0=off 1=on -1=auto", ""},
        { "lowres", OPT_INT | HAS_ARG | OPT_EXPERT, { &lowres }, "", "" },
        { "sync", HAS_ARG | OPT_EXPERT, { .func_arg = opt_sync }, "set audio-video sync. type (type=audio/video/ext)", "type" },
        { "autoexit", OPT_BOOL | OPT_EXPERT, { &autoexit }, "exit at the end", "" },
        { "exitonkeydown", OPT_BOOL | OPT_EXPERT, { &exit_on_keydown }, "exit on key down", "" },
        { "exitonmousedown", OPT_BOOL | OPT_EXPERT, { &exit_on_mousedown }, "exit on mouse down", "" },
        { "loop", OPT_INT | HAS_ARG | OPT_EXPERT, { &loop }, "set number of times the playback shall be looped", "loop count" },
        { "framedrop", OPT_BOOL | OPT_EXPERT, { &framedrop }, "drop frames when cpu is too slow", "" },
        { "infbuf", OPT_BOOL | OPT_EXPERT, { &infinite_buffer }, "don't limit the input buffer size (useful with realtime streams)", "" },
        { "window_title", OPT_STRING | HAS_ARG, { &window_title }, "set window title", "window title" },
        { "left", OPT_INT | HAS_ARG | OPT_EXPERT, { &screen_left }, "set the x position for the left of the window", "x pos" },
        { "top", OPT_INT | HAS_ARG | OPT_EXPERT, { &screen_top }, "set the y position for the top of the window", "y pos" },
#if CONFIG_AVFILTER
        { "vf", OPT_EXPERT | HAS_ARG, { .func_arg = opt_add_vfilter }, "set video filters", "filter_graph" },
        { "af", OPT_STRING | HAS_ARG, { &afilters }, "set audio filters", "filter_graph" },
#endif
        { "rdftspeed", OPT_INT | HAS_ARG| OPT_AUDIO | OPT_EXPERT, { &rdftspeed }, "rdft speed", "msecs" },
        { "showmode", HAS_ARG, { .func_arg = opt_show_mode}, "select show mode (0 = video, 1 = waves, 2 = RDFT)", "mode" },
        { "default", HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, { .func_arg = opt_default }, "generic catch all option", "" },
        { "i", OPT_BOOL, { &dummy}, "read specified file", "input_file"},
        { "codec", HAS_ARG, { .func_arg = opt_codec}, "force decoder", "decoder_name" },
        { "acodec", HAS_ARG | OPT_STRING | OPT_EXPERT, {    &audio_codec_name }, "force audio decoder",    "decoder_name" },
        { "scodec", HAS_ARG | OPT_STRING | OPT_EXPERT, { &subtitle_codec_name }, "force subtitle decoder", "decoder_name" },
        { "vcodec", HAS_ARG | OPT_STRING | OPT_EXPERT, {    &video_codec_name }, "force video decoder",    "decoder_name" },
        { "autorotate", OPT_BOOL, { &autorotate }, "automatically rotate video", "" },
        { "find_stream_info", OPT_BOOL | OPT_INPUT | OPT_EXPERT, { &find_stream_info },
          "read and decode the streams to fill missing information with heuristics" },
        { "filter_threads", HAS_ARG | OPT_INT | OPT_EXPERT, { &filter_nbthreads }, "number of filter threads per graph" },
        { NULL, },
};

static void check_options(const OptionDef *po)
{
    while (po->name) {
        if (po->flags & OPT_PERFILE)
            av_assert0(po->flags & (OPT_INPUT | OPT_OUTPUT));
        po++;
    }
}

static void expand_filename_template(AVBPrint *bp, const char *template,
                                    struct tm *tm)
{
    int c;

    while ((c = *(template++))) {
        if (c == '%') {
            if (!(c = *(template++)))
                break;
            switch (c) {
                case 'p':
                    av_bprintf(bp, "%s", program_name);
                    break;
                case 't':
                    av_bprintf(bp, "%04d%02d%02d-%02d%02d%02d",
                               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                               tm->tm_hour, tm->tm_min, tm->tm_sec);
                    break;
                case '%':
                    av_bprint_chars(bp, c, 1);
                    break;
            }
        } else {
            av_bprint_chars(bp, c, 1);
        }
    }
}

static void log_callback_report(void *ptr, int level, const char *fmt, va_list vl)
{
    va_list vl2;
    char line[1024];
    static int print_prefix = 1;


    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);
    if (report_file_level >= level) {
        fputs(line, report_file);
        fflush(report_file);
    }
}

static int init_report(const char *env)
{
    char *filename_template = NULL;
    char *key, *val;
    int ret, count = 0;
    int prog_loglevel, envlevel = 0;
    time_t now;
    struct tm *tm;
    AVBPrint filename;

    if (report_file) /* already opened */
        return 0;
    time(&now);
    tm = localtime(&now);

    while (env && *env) {
        if ((ret = av_opt_get_key_value(&env, "=", ":", 0, &key, &val)) < 0) {
            if (count)
                av_log(NULL, AV_LOG_ERROR,
                       "Failed to parse FFREPORT environment variable: %s\n",
                       av_err2str(ret));
            break;
        }
        if (*env)
            env++;
        count++;
        if (!strcmp(key, "file")) {
            av_free(filename_template);
            filename_template = val;
            val = NULL;
        } else if (!strcmp(key, "level")) {
            char *tail;
            report_file_level = strtol(val, &tail, 10);
            if (*tail) {
                av_log(NULL, AV_LOG_FATAL, "Invalid report file level\n");
                exit_program(1);
            }
            envlevel = 1;
        } else {
            av_log(NULL, AV_LOG_ERROR, "Unknown key '%s' in FFREPORT\n", key);
        }
        av_free(val);
        av_free(key);
    }

    av_bprint_init(&filename, 0, AV_BPRINT_SIZE_AUTOMATIC);
    expand_filename_template(&filename,
                             av_x_if_null(filename_template, "%p-%t.log"), tm);
    av_free(filename_template);
    if (!av_bprint_is_complete(&filename)) {
        av_log(NULL, AV_LOG_ERROR, "Out of memory building report file name\n");
        return AVERROR(ENOMEM);
    }

    prog_loglevel = av_log_get_level();
    if (!envlevel)
        report_file_level = FFMAX(report_file_level, prog_loglevel);

    report_file = fopen(filename.str, "w");
    if (!report_file) {
        int ret = AVERROR(errno);
        av_log(NULL, AV_LOG_ERROR, "Failed to open report \"%s\": %s\n",
               filename.str, strerror(errno));
        return ret;
    }
    av_log_set_callback(log_callback_report);
    av_log(NULL, AV_LOG_INFO,
           "%s started on %04d-%02d-%02d at %02d:%02d:%02d\n"
           "Report written to \"%s\"\n"
           "Log level: %d\n",
           program_name,
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec,
           filename.str, report_file_level);
    av_bprint_finalize(&filename, NULL);
    return 0;
}

static void dump_argument(const char *a)
{
    const unsigned char *p;

    for (p = a; *p; p++)
        if (!((*p >= '+' && *p <= ':') || (*p >= '@' && *p <= 'Z') ||
              *p == '_' || (*p >= 'a' && *p <= 'z')))
            break;
    if (!*p) {
        fputs(a, report_file);
        return;
    }
    fputc('"', report_file);
    for (p = a; *p; p++) {
        if (*p == '\\' || *p == '"' || *p == '$' || *p == '`')
            fprintf(report_file, "\\%c", *p);
        else if (*p < ' ' || *p > '~')
            fprintf(report_file, "\\x%02x", *p);
        else
            fputc(*p, report_file);
    }
    fputc('"', report_file);
}

void parse_loglevel(int argc, char **argv, const OptionDef *options)
{
    int idx = locate_option(argc, argv, options, "loglevel");
    const char *env;

    check_options(options);

    if (!idx)
        idx = locate_option(argc, argv, options, "v");
    if (idx && argv[idx + 1])
        opt_loglevel(NULL, "loglevel", argv[idx + 1]);
    idx = locate_option(argc, argv, options, "report");
    if ((env = getenv("FFREPORT")) || idx) {
        init_report(env);
        if (report_file) {
            int i;
            fprintf(report_file, "Command line:\n");
            for (i = 0; i < argc; i++) {
                dump_argument(argv[i]);
                fputc(i < argc - 1 ? ' ' : '\n', report_file);
            }
            fflush(report_file);
        }
    }
    idx = locate_option(argc, argv, options, "hide_banner");
    if (idx)
        hide_banner = 1;
}

void show_usage(void)
{
    av_log(NULL, AV_LOG_INFO, "Simple media player\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s [options] input_file\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

void show_help_default(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, OPT_EXPERT, 0);
    show_help_options(options, "Advanced options:", OPT_EXPERT, 0, 0);
    printf("\n");
    show_help_children(avcodec_get_class(), AV_OPT_FLAG_DECODING_PARAM);
    show_help_children(avformat_get_class(), AV_OPT_FLAG_DECODING_PARAM);
#if !CONFIG_AVFILTER
    show_help_children(sws_get_class(), AV_OPT_FLAG_ENCODING_PARAM);
#else
    show_help_children(avfilter_get_class(), AV_OPT_FLAG_FILTERING_PARAM);
#endif
    printf("\nWhile playing:\n"
           "q, ESC              quit\n"
           "f                   toggle full screen\n"
           "p, SPC              pause\n"
           "m                   toggle mute\n"
           "9, 0                decrease and increase volume respectively\n"
           "/, *                decrease and increase volume respectively\n"
           "a                   cycle audio channel in the current program\n"
           "v                   cycle video channel\n"
           "t                   cycle subtitle channel in the current program\n"
           "c                   cycle program\n"
           "w                   cycle video filters or show modes\n"
           "s                   activate frame-step mode\n"
           "left/right          seek backward/forward 10 seconds or to custom interval if -seek_interval is set\n"
           "down/up             seek backward/forward 1 minute\n"
           "page down/page up   seek backward/forward 10 minutes\n"
           "right mouse click   seek to percentage in file corresponding to fraction of width\n"
           "left double-click   toggle full screen\n"
    );
}