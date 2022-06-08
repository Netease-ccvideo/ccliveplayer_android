/*
 * ff_ffplaye_def.h
 *
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFPLAY__FF_FFPLAY_DEF_H
#define FFPLAY__FF_FFPLAY_DEF_H

#include <stdbool.h>
#include "ff_ffinc.h"
#include "ff_ffplay_config.h"
#include "ff_ffmsg_queue.h"
#include "ff_ffpipenode.h"
#include "ijkmeta.h"
#include "ff_file_saver.h"
#include <limits.h>
#include <libavutil/audio_fifo.h>

#define MAX_QUEUE_SIZE (10 * 1024 * 1024)
#define MAX_ACCURATE_SEEK_TIMEOUT (5000)
#define MIN_FRAMES 50000

#define JITTER_DEFAULT 200
#define JITTER_DEFAULT_FIRST    (50)

//缓冲队列减小缓冲时，使用的比例。
#define BUFFERING_TARGET_REDUCTOR_RATIO  0.91f
#define FORWARD_CHECK_PACKET_COUNT 791
#define SAFE_INTERVAL_SINCE_STUCK (5 * 60 * 1000)
/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 100.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required screen refresh at least this often, should be less than 1/fps */
// 172.0 fps at most (High Level 5.2 1,920×1,080@172.0)
#define REFRESH_RATE 0.01

#define DEFAULT_VIDEO_REFRESH_INTERVAL 50 //fps=20

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

/* read thread ret value */
#define READ_RET_OK              0
#define READ_RET_FAIL           -100
#define READ_RET_CONTINE        -101
#define READ_RET_ABORT          -102
#define READ_RET_QUEUE_FULL     -103
#define READ_RET_EOF            -104
#define READ_RET_NOT_IN_RANGE   -105

#define AVFORMAT_TIMEOUT       "timeout"


#ifdef FFP_MERGE
#define CURSOR_HIDE_DELAY 1000000

static int64_t sws_flags = SWS_BICUBIC;
#endif

//call back function
#ifndef HAVE_DISPLAY_FRAME_CB
#define HAVE_DISPLAY_FRAME_CB
enum FrameFormat
{
    FRAME_FORMAT_NONE = -1,
    FRAME_FORMAT_YUV = 0,
    FRAME_FORMAT_RGB = 1,
    FRAME_FORMAT_BGR = 2,
};

enum RadicalLevel
{
    RadicalLevel_0 = 0,
    RadicalLevel_1 = 1,
    RadicalLevel_2 = 2,
    RadicalLevel_3 = 3,
};

typedef struct DisplayFrame {
    void *data;
    int format;
    int width;
    int height;
    int pitch;
    int bits;
} DisplayFrame;

// callback function to get display frame
typedef void (*OnDisplayFrameCb)(DisplayFrame *frame, void *obj);
#endif

#define MAX_DEVIATION 1200000   // 1200ms

typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
    MyAVPacketList *recycle_pkt;
    int recycle_count;
    int alloc_count;

    int is_buffer_indicator;
} PacketQueue;

// #define VIDEO_PICTURE_QUEUE_SIZE 3
#define VIDEO_PICTURE_QUEUE_SIZE_MIN        (3)
#define VIDEO_PICTURE_QUEUE_SIZE_MAX        (16)
#define VIDEO_PICTURE_QUEUE_SIZE_DEFAULT    (VIDEO_PICTURE_QUEUE_SIZE_MIN)
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE_MIN 9
#define SAMPLE_QUEUE_SIZE 20
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE_MAX, SUBPICTURE_QUEUE_SIZE))

#define VIDEO_MAX_FPS_DEFAULT 30

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    SDL_VoutOverlay *bmp;
    int allocated;
    int reallocate;
    int width;
    int height;
    AVRational sar;
    int frame_format;
    int crop;
    int uploaded;
} Frame;

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

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

typedef struct Decoder {
    AVPacket pkt;
    AVPacket pkt_temp;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    int bfsc_ret;
    uint8_t *bfsc_data;

    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread *decoder_tid;
    SDL_Thread _decoder_tid;
    
    Uint64 first_frame_decoded_time;
    int    first_frame_decoded;
    
} Decoder;

enum PlayState {
        STATE_ERROR = -1, STATE_IDLE = 0, STATE_LOADING, STATE_BUFFER, STATE_PLAYING, STATE_PAUSE, STATE_STOP,
};

#define REDIRECT_URL_LEN 256
#define SERVER_IP_LEN 32
#define STREAM_ID_LEN 64

typedef struct StatInfo {
    int buffer_count;
    double buffer_len;
    double buffer_time;
    long buffer_begin_time;
    long last_buffer_warn_time;
    int buffer_warn_flag;
    int drop_frame;
    int play_state;
    double play_time;
    double playable_time;
    double playable_time_sum;
    double audio_pts_interval;
    int download_bps;
    int download_per_min;
    double buffer_pre;
    double buffer_sum;
    int forward_count;

    char redirect_url[REDIRECT_URL_LEN];
    char server_ip[SERVER_IP_LEN];
    char stream_id[STREAM_ID_LEN];
    double redirect_time;
    int video_bitrate;
    int video_framerate;
    int video_width;
    int video_height;
    double first_buffering_time;
    
    double audio_buffer_duration;
    int audio_buffer_packet_count;
    double video_buffer_duration;
    int video_buffer_packet_count;
    int reconnect_count;
} StatInfo;

typedef struct JitterCalculator {
    int max_jitter;
    int second_max_jitter;
    int forward_period;
    int64_t forward_time;
} JitterCalculator;

typedef struct VideoState {
    SDL_Thread *read_tid;
    SDL_Thread _read_tid;
    SDL_Thread *video_tid;
    SDL_Thread _video_tid;
    SDL_Thread *audio_tid;
    SDL_Thread _audio_tid;
    AVInputFormat *iformat;
    int abort_request;
    int force_refresh;
    int force_videodisplay;
    int64_t force_videodisplay_st;
    int64_t get_first_video_frame_tick;
    int force_initdisplay;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int pause_keep_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
#ifdef FFP_MERGE
    int read_pause_return;
#endif
    AVFormatContext *ic;
    int realtime;

    Clock audclk;
    Clock vidclk;
    Clock extclk;

    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;
    

    int bak_pic_queue_prepared;
    int64_t bak_pic_pts_interval;
    int bak_samp_queue_prepared;
    int64_t bak_samp_pts_interval;
    SDL_Thread *video_sketch_tid;
    SDL_Thread _video_sketch_tid;
    
    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t silence_buf[SDL_AUDIO_MIN_BUFFER_SIZE];
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    short *audio_new_buf;  /* for soundtouch buf */
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    unsigned int audio_new_buf_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    struct AudioParams audio_src;
#if CONFIG_AVFILTER
    struct AudioParams audio_filter_src;
#endif
    struct AudioParams audio_tgt;
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;
    int continuous_frame_drops_early;
    int64_t stream_open_time;
    int64_t open_input_time;
    int64_t max_open_input_time;
    int64_t first_frame_time;

    enum ShowMode {
        SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
    } show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
#ifdef FFP_MERGE
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;
#endif
    double last_vis_time;
    float pause_keep_ms;
    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;

    double frame_timer;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
#if !CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
#endif
#ifdef FFP_MERGE
    SDL_Rect last_display_rect;
#endif

    char filename[4096];
    int width, height, xleft, ytop;
    int step;

#if CONFIG_AVFILTER
    int vfilter_idx;
    AVFilterContext *in_video_filter;   // the first filter in the video chain
    AVFilterContext *out_video_filter;  // the last filter in the video chain
    AVFilterContext *in_audio_filter;   // the first filter in the audio chain
    AVFilterContext *out_audio_filter;  // the last filter in the audio chain
    AVFilterGraph *agraph;              // audio filter graph
#endif

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;

    /* extra fields */
    SDL_mutex  *play_mutex; // only guard state, do not block any long operation
    SDL_Thread *video_refresh_tid;
    SDL_Thread _video_refresh_tid;
    
    int buffering_on;
    int pause_req;

    int dropping_frame;
    int is_video_high_fps; // above 30fps
    int is_video_high_res; // above 1080p

    PacketQueue *buffer_indicator_queue;

    // for statistic information
    StatInfo stat_info;
    double audio_cur_pts;
    int64_t last_time;
    int64_t last_recv_size;
    int64_t cur_recv_size;
    int64_t last_buffer_time;
    int64_t last_stuck_time;
    int first_buffering_ready;//是否首次缓冲
    int64_t first_buffering_ready_time;//首次缓冲就绪，马上开始播放的时间点

    int frame_capturing;//是否正在视频截图
    SDL_Thread *capture_thread;
    SDL_Thread _capture_thread;
    int capture_scale_ratio;//截图缩放比例
    int capture_frame_size;//截图占内存大小
    int capture_frame_width;//截图宽度
    int capture_frame_height;//截图高度
    uint8_t *capture_frame_data;//截图图像数据

	OnDisplayFrameCb onDisplayFrameCb;
    void *display_frame_obj;
    int64_t last_refresh_time;
    
    JitterCalculator jitter_calculator;
    int prepared;
    int64_t enter_background_tick;

    volatile int latest_video_seek_load_serial;
    volatile int latest_audio_seek_load_serial;
    volatile int64_t latest_seek_load_start_at;

    int drop_aframe_count;
    int drop_vframe_count;
    int64_t accurate_seek_start_time;
    volatile int64_t accurate_seek_vframe_pts;
    volatile int64_t accurate_seek_aframe_pts;
    int audio_accurate_seek_req;
    int video_accurate_seek_req;
    SDL_mutex *accurate_seek_mutex;
    SDL_cond  *video_accurate_seek_cond;
    SDL_cond  *audio_accurate_seek_cond;
    int seek_buffering;
    void* handle;

} VideoState;

/* options specified by the user */
#ifdef FFP_MERGE
static AVInputFormat *file_iformat;
static const char *input_filename;
static const char *window_title;
static int fs_screen_width;
static int fs_screen_height;
static int default_width  = 640;
static int default_height = 480;
static int screen_width  = 0;
static int screen_height = 0;
static int audio_disable;
static int video_disable;
static int subtitle_disable;
static const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};
static int seek_by_bytes = -1;
static int display_disable;
static int show_status = 1;
static int av_sync_type = AV_SYNC_AUDIO_MASTER;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int fast = 0;
static int genpts = 0;
static int lowres = 0;
static int decoder_reorder_pts = -1;
static int autoexit;
static int exit_on_keydown;
static int exit_on_mousedown;
static int loop = 1;
static int framedrop = -1;
static int infinite_buffer = -1;
static enum ShowMode show_mode = SHOW_MODE_NONE;
static const char *audio_codec_name;
static const char *subtitle_codec_name;
static const char *video_codec_name;
double rdftspeed = 0.02;
static int64_t cursor_last_shown;
static int cursor_hidden = 0;
#if CONFIG_AVFILTER
static const char **vfilters_list = NULL;
static int nb_vfilters = 0;
static char *afilters = NULL;
#endif
static int autorotate = 1;

/* current context */
static int is_full_screen;
static int64_t audio_callback_time;

static AVPacket flush_pkt;
static AVPacket eof_pkt;

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

static SDL_Surface *screen;
#endif

/*****************************************************************************
 * end at line 330 in ffplay.c
 * near packet_queue_put
 ****************************************************************************/
typedef struct UdpVideoParameters {
    
    enum AVCodecID codecId;
    int width;
    int height;
    int bitRate;
    int frameRate;
    bool inited;
    
} UdpVideoParameters;

typedef struct UdpAudioParameters {
    
    enum AVCodecID codecId;
    int sampleRate;
    int channels;
    bool inited;
    
} UdpAudioParameters;

typedef struct UdpContext {
    
    bool  udpReady;
    bool videoHeadisReady;
    bool audioHeadReady;
    bool videoCtxInited;
    bool audioCtxInited;
    
    int audio_stream_count;
    int pre_percent;
    int64_t audio_packet_count;
    SDL_mutex *wait_mutex;
    bool first_audio_frame;
    bool first_video_frame;
    int64_t last_video_packet_recv_time;
    unsigned int udp_first_pts;
    
} UdpContext;

typedef struct VideoMuxerFileContext {
    
    AVCodecContext * video_codec_ctx;
    struct SwsContext* scale_ctx;
    int video_stream_index;
    uint32_t timestamp;
    
} VideoMuxerFileContext;

typedef struct AudioMuxerFileContext {
    
    AVFormatContext* audio_muxer_fmt_ctx;
    struct AVAudioFifo *audio_fifo;
    AVCodecContext * audio_codec_ctx;
    struct SwrContext* audio_resample_ctx;
    int audio_stream_index;
    uint32_t audioPts;
    
} AudioMuxerFileContext;

typedef struct UserInfo {

    int anchorid;
    uint32_t uid;
} UserInfo;

typedef struct ReportInfo {

    VideoMuxerFileContext *video_file_ctx;
    AudioMuxerFileContext *audio_file_ctx;
    char bak_muxer_tmp_dir[PATH_MAX];
    
    int64_t section_start;
    int64_t section_end;
    int targetPicNum;
    int targetDuration;
    int scaled_width;
    int scaled_height;
    char savePath[PATH_MAX];
    char audioFileName[PATH_MAX];
    char* picFileNames[30];

    int isMuxing;

} ReportInfo;

/* ffplayer */
typedef struct jitter_queue {
    int64_t *buffer;
    int cur_idx;
    int max_size;
    int64_t max_value_1;    //第1大
    int64_t max_value_2;    //第2大
    int max_value1_idx;
    int max_value2_idx;
} jitter_queue;

typedef struct jitter_buffer{
    int max_buffer_msec;
    int min_buffer_msec;
    int buffer_msec;    //can playing time
    
    int cycle_packet_nb_count; //收集多少个包做一个计算周期
    struct jitter_queue jitter_q;
} jitter_buffer_config;

typedef struct FileSaver {
    int save_state; //-1 UNFINISH，-2 SEEK，-3 inSTART， -4 inFILE, -5 REMOVE
    const char *save_path;
    int is_save;
    AVFormatContext *m_ofmt_ctx;
    AVOutputFormat *m_ofmt;
    int videosave_index;
    int audiosave_index;
    int videosave_index_out;
    int audiosave_index_out;
} FileSaver;

typedef struct IjkMediaMeta IjkMediaMeta;
typedef struct IJKFF_Pipeline IJKFF_Pipeline;
typedef struct FFPlayer {
    const AVClass *av_class;
    /* ffplay context */
    VideoState *is;
    void *weak_thiz;

    /* format/codec options */
    AVDictionary *format_opts;
    AVDictionary *codec_opts;
    AVDictionary *sws_opts;
    AVDictionary *player_opts;
    AVDictionary *swr_opts;
    AVDictionary *swr_preset_opts;

    /* ffplay options specified by the user */
#ifdef FFP_MERGE
    AVInputFormat *file_iformat;
#endif
    char *input_filename;
#ifdef FFP_MERGE
    const char *window_title;
    int fs_screen_width;
    int fs_screen_height;
    int default_width;
    int default_height;
    int screen_width;
    int screen_height;
#endif
    int audio_disable;
    int video_disable;
    int enable_subtitle; //字幕功能总控（默认为0，即不可用字幕）
    bool subtitle_display; //字幕显示开关，总控打开前提下，在播放过程中随时显示（true）/关闭（false）
    char *audioLanguage; //音轨语言设置
    char *subtitleLanguage; //字幕语言设置

    FileSaver *saver;

    const char* wanted_stream_spec[AVMEDIA_TYPE_NB];
    int seek_by_bytes;
    int display_disable;
    int display_ready;

    int max_video_buffer_packet_nb;
    bool realtime_play; //是否为实时播放（直播）
    
    int radical_realtime_level; //更加激进的实时策略
    int64_t realtime_last_audio_packet_time;//记录上次收到音频包的时间，关闭快进时清零
    int64_t realtime_audio_packet_count;//开启快进时收到的音频包计数，关闭快进时清零
    int64_t realtime_video_packet_count;//开启快进时收到的视频包计数，关闭快进时清零
    int buffering_target_duration_ms; //当前要缓冲的语音数据（毫秒）
    int buffering_target_duration_ms_first; //第一次要缓冲的最少语音数据（毫秒）
    int buffering_target_duration_ms_min; //当前要缓冲的最少语音数据（毫秒）
    int buffering_target_duration_ms_max; //当前要缓冲的最多语音数据（毫秒）
    int64_t buffering_target_duration_change_time;  //缓冲区更新时间
    int buffering_target_duration_ms_limit;
    
    int show_status;
    int av_sync_type;
    int64_t start_time;
    int64_t duration;
    int fast;
    int genpts;
    int lowres;
    int decoder_reorder_pts;
    int autoexit;
#ifdef FFP_MERGE
    int exit_on_keydown;
    int exit_on_mousedown;
#endif
    int loop;
    int framedrop;
    int64_t seek_at_start;
    int infinite_buffer;
    enum ShowMode show_mode;
    char *audio_codec_name;
    char *subtitle_codec_name;
    char *video_codec_name;
    double rdftspeed;
#ifdef FFP_MERGE
    int64_t cursor_last_shown;
    int cursor_hidden;
#endif
#if CONFIG_AVFILTER
    const char **vfilters_list;
    int nb_vfilters;
    char *afilters;
#endif
    int autorotate;

    int sws_flags;

    /* current context */
#ifdef FFP_MERGE
    int is_full_screen;
#endif
    int64_t audio_callback_time;
#ifdef FFP_MERGE
    SDL_Surface *screen;
#endif

    /* extra fields */
    SDL_Aout *aout;
    SDL_Vout *vout;
    IJKFF_Pipeline *pipeline;
    IJKFF_Pipenode *node_vdec;
    int sar_num;
    int sar_den;

    char *video_codec_info;
    char *audio_codec_info;
    char *subtitle_codec_info;
    Uint32 overlay_format;

    int last_error;
    int prepared;
    int auto_start;
    int error;
    int eof;
    int error_count;

    int start_on_prepared;
    int first_video_frame_rendered;
    int first_audio_frame_rendered;
    int sync_av_start;
    
    int max_open_input_time;
    
    MessageQueue msg_queue;

    int max_buffer_size;

    int64_t playable_duration_ms;

    int pictq_size;
    int sampq_size;
    
    int max_fps;

    double preset_5_1_center_mix_level;
    
    IjkMediaMeta *meta;
    int soundtouch_enable;
    SDL_SpeedSampler vfps_sampler;
    SDL_SpeedSampler vdps_sampler;
    
    void *format_control_opaque;

    bool crop;//渲染时，是否裁剪图片，主要应用于连麦
    int surface_width;
    int surface_height;
    
    int videotoolbox;
    int vtb_max_frame_width;
    int vtb_async;
    int vtb_wait_async;
    int vtb_handle_resolution_change;
    int mediacodec_avc;
    int mediacodec_hevc;
    int mediacodec_mpeg2;
    int mediacodec_mpeg4;
    int mediacodec_all_videos;
    
    float       pf_playback_rate;
    int         pf_playback_rate_changed;
    float       pf_playback_volume;
    int         pf_playback_volume_changed;
	
	void* udp_player_opaque;
    UdpContext *udpCtx;
    UdpVideoParameters *udpVideoParams;
    UdpAudioParameters *udpAudioParams;

    bool codecIsNewAlloc;
    
    bool udp_prepared;
    
    bool muted;
    
    
#ifdef __APPLE__
    void                    *inject_opaque;
    AVApplicationContext    *app_ctx;
    bool                    show_debug_info;
#endif

    bool enable_report_capture;
    ReportInfo* report_info;
    
    int64_t created_tick;
    double connwait;
    
    void *capture_opaque;
    
    int networkType;

    int enable_accurate_seek;
    int accurate_seek_timeout;

    int mediacodec_auto_rotate;
    int rotate_degree;

} FFPlayer;

#define fftime_to_milliseconds(ts) (av_rescale(ts, 1000, AV_TIME_BASE))
#define milliseconds_to_fftime(ms) (av_rescale(ms, AV_TIME_BASE, 1000))

inline static void ffp_reset_internal(FFPlayer *ffp)
{
    /* ffp->is closed in stream_close() */
    av_opt_free(ffp);
    
    /* format/codec options */
    av_dict_free(&ffp->format_opts);
    av_dict_free(&ffp->codec_opts);
    av_dict_free(&ffp->sws_opts);
    av_dict_free(&ffp->player_opts);
    av_dict_free(&ffp->swr_opts);
    av_dict_free(&ffp->swr_preset_opts);

    /* ffplay options specified by the user */
    av_freep(&ffp->input_filename);
    ffp->audio_disable          = 0;
    ffp->video_disable          = 0;
    ffp->enable_subtitle        = 0;
    ffp->audioLanguage          = NULL;
    ffp->subtitleLanguage       = NULL;
    ffp->subtitle_display       = false;
    memset(ffp->wanted_stream_spec, 0, sizeof(ffp->wanted_stream_spec));
    ffp->seek_by_bytes          = -1;
    ffp->display_disable        = 0;
    ffp->show_status            = 0;
    ffp->av_sync_type           = AV_SYNC_AUDIO_MASTER;
    ffp->start_time             = AV_NOPTS_VALUE;
    ffp->duration               = AV_NOPTS_VALUE;
    ffp->fast                   = 1;
    ffp->genpts                 = 0;
    ffp->lowres                 = 0;
    ffp->decoder_reorder_pts    = -1;
    ffp->autoexit               = 0;
    ffp->loop                   = 1;
    ffp->framedrop              = 0;
    ffp->seek_at_start          = 0;
    ffp->infinite_buffer        = -1;
    ffp->show_mode              = SHOW_MODE_NONE;
    av_freep(&ffp->audio_codec_name);
    av_freep(&ffp->video_codec_name);
    av_freep(&ffp->subtitle_codec_name);
    ffp->rdftspeed              = 0.02;
#if CONFIG_AVFILTER
    ffp->vfilters_list          = NULL;
    ffp->nb_vfilters            = 0;
    ffp->afilters               = NULL;
#endif
    ffp->autorotate             = 1;

    // ffp->sws_flags              = SWS_BICUBIC;
    ffp->sws_flags              = SWS_FAST_BILINEAR;

    /* current context */
    ffp->audio_callback_time    = 0;

    /* extra fields */
    ffp->aout                   = NULL; /* reset outside */
    ffp->vout                   = NULL; /* reset outside */
    ffp->pipeline               = NULL;
    ffp->node_vdec              = NULL;
    ffp->sar_num                = 0;
    ffp->sar_den                = 0;

    av_freep(&ffp->video_codec_info);
    av_freep(&ffp->audio_codec_info);
    av_freep(&ffp->subtitle_codec_info);
    // ffp->overlay_format         = SDL_FCC_YV12;
    ffp->overlay_format         = SDL_FCC_RV32;
    // ffp->overlay_format         = SDL_FCC_RV16;
	ffp->overlay_format         = SDL_FCC_I420;

    ffp->last_error             = 0;
    ffp->prepared               = 0;
    ffp->auto_start             = 0;
    ffp->error                  = 0;
    ffp->error_count            = 0;

    ffp->start_on_prepared      = 1;
    ffp->first_video_frame_rendered = 0;
    ffp->first_audio_frame_rendered = 0;
    ffp->sync_av_start          = 1;
    ffp->enable_accurate_seek   = 1;
    ffp->accurate_seek_timeout  = MAX_ACCURATE_SEEK_TIMEOUT;

    ffp->max_buffer_size                = MAX_QUEUE_SIZE;

    ffp->playable_duration_ms           = 0;

    ffp->pictq_size                     = VIDEO_PICTURE_QUEUE_SIZE_MIN;
    ffp->sampq_size                     = SAMPLE_QUEUE_SIZE_MIN;
    ffp->max_fps                        = VIDEO_MAX_FPS_DEFAULT;

    ffp->format_control_opaque  = NULL;
    ffp->soundtouch_enable = 1; // option
    ijkmeta_reset(ffp->meta);
    ffp->meta = NULL;
    
    SDL_SpeedSamplerReset(&ffp->vfps_sampler);
    SDL_SpeedSamplerReset(&ffp->vdps_sampler);
    
    ffp->buffering_target_duration_ms = 100;
    ffp->buffering_target_duration_ms_first = 0;
    ffp->buffering_target_duration_ms_min = 800;
    ffp->buffering_target_duration_ms_max = 5000;
    ffp->videotoolbox = 0;
    ffp->vtb_handle_resolution_change   = 0;
    ffp->vtb_max_frame_width            = 0;

    ffp->pf_playback_rate               = 1.0f;
    ffp->pf_playback_rate_changed       = 0;
    ffp->pf_playback_volume             = 1.0f;
    ffp->pf_playback_volume_changed     = 0;

    ffp->codecIsNewAlloc = false;

    
    ffp->udp_prepared = false;
    
    ffp->muted = false;

    ReportInfo* reportInfo = ffp->report_info;
    if (reportInfo != NULL) {
        reportInfo->targetDuration = 15;
        reportInfo->scaled_width = 0;
        reportInfo->scaled_height = 0;
        reportInfo->section_start = 0;
        reportInfo->section_end = 0;
        reportInfo->isMuxing = 0;
    }

    ffp->created_tick = 0;
    ffp->connwait = 0.0f;
    ffp->radical_realtime_level = 0;
    
    msg_queue_flush(&ffp->msg_queue);
    
#ifdef __APPLE__
    ffp->show_debug_info = false;
    av_application_closep(&ffp->app_ctx);
    ffp->inject_opaque = NULL;
#endif
    ffp->capture_opaque = NULL;
    
    ffp->max_open_input_time = 5 * 1000 * 1000;
    
    ffp->networkType = 1;

    ffp->mediacodec_auto_rotate = 1;

}

inline static void ffp_notify_msg1(FFPlayer *ffp, int what) {
    msg_queue_put_simple3(&ffp->msg_queue, what, 0, 0);
}

inline static void ffp_notify_msg2(FFPlayer *ffp, int what, int arg1) {
    msg_queue_put_simple3(&ffp->msg_queue, what, arg1, 0);
}

inline static void ffp_notify_msg3(FFPlayer *ffp, int what, int arg1, int arg2) {
    msg_queue_put_simple3(&ffp->msg_queue, what, arg1, arg2);
}

inline static void ffp_notify_msg4(FFPlayer *ffp, int what, int arg1, int arg2, void *obj, int obj_len) {
    msg_queue_put_simple4(&ffp->msg_queue, what, arg1, arg2, obj, obj_len);
}

inline static void ffp_remove_msg(FFPlayer *ffp, int what) {
    msg_queue_remove(&ffp->msg_queue, what);
}

inline static const char *ffp_get_error_string(int error) {
    switch (error) {
        case AVERROR(ENOMEM):       return "AVERROR(ENOMEM)";       // 12
        case AVERROR(EINVAL):       return "AVERROR(EINVAL)";       // 22
        case AVERROR(EAGAIN):       return "AVERROR(EAGAIN)";       // 35
        case AVERROR(ETIMEDOUT):    return "AVERROR(ETIMEDOUT)";    // 60
        case AVERROR_EOF:           return "AVERROR_EOF";
        case AVERROR_EXIT:          return "AVERROR_EXIT";
    }
    return "unknown";
}

#define FFTRACE ALOGW

#define AVCODEC_MODULE_NAME    "avcodec"
#define MEDIACODEC_MODULE_NAME "MediaCodec"

#endif
