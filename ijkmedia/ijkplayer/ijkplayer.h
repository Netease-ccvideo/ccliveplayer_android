/*
 * ijkplayer.h
 *
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

#ifndef IJKPLAYER_ANDROID__IJKPLAYER_H
#define IJKPLAYER_ANDROID__IJKPLAYER_H

#include <stdbool.h>
#include "ff_ffmsg_queue.h"

#include "ijkutil/ijkutil.h"
#include "ijkmeta.h"
#include "ff_ffplay_def.h"

#ifndef MPTRACE
#define MPTRACE ALOGW
#endif

typedef struct IjkMediaPlayer IjkMediaPlayer;
typedef struct FFPlayer FFPlayer;
typedef struct SDL_Vout SDL_Vout;

/*-
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_IDLE);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_INITIALIZED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_ASYNC_PREPARING);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_PREPARED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_STARTED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_PAUSED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_COMPLETED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_STOPPED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_ERROR);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_END);
 */

/*-
 * ijkmp_set_data_source()  -> MP_STATE_INITIALIZED
 *
 * ijkmp_reset              -> self
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_IDLE               0

/*-
 * ijkmp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_INITIALIZED        1

/*-
 *                   ...    -> MP_STATE_PREPARED
 *                   ...    -> MP_STATE_ERROR
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_ASYNC_PREPARING    2

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_PREPARED           3

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> self
 * ijkmp_pause()            -> MP_STATE_PAUSED
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *                   ...    -> MP_STATE_COMPLETED
 *                   ...    -> MP_STATE_ERROR
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_STARTED            4

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED
 * ijkmp_pause()            -> self
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_PAUSED             5

/*-
 * ijkmp_seek_to()          -> self
 * ijkmp_start()            -> MP_STATE_STARTED (from beginning)
 * ijkmp_pause()            -> self
 * ijkmp_stop()             -> MP_STATE_STOPPED
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_COMPLETED          6

/*-
 * ijkmp_stop()             -> self
 * ijkmp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_STOPPED            7

/*-
 * ijkmp_reset              -> MP_STATE_IDLE
 * ijkmp_release            -> MP_STATE_END
 */
#define MP_STATE_ERROR              8

/*-
 * ijkmp_release            -> self
 */
#define MP_STATE_END                9



#define IJKMP_IO_STAT_READ 1


#define IJKMP_OPT_CATEGORY_FORMAT FFP_OPT_CATEGORY_FORMAT
#define IJKMP_OPT_CATEGORY_CODEC  FFP_OPT_CATEGORY_CODEC
#define IJKMP_OPT_CATEGORY_SWS    FFP_OPT_CATEGORY_SWS
#define IJKMP_OPT_CATEGORY_PLAYER FFP_OPT_CATEGORY_PLAYER
#define IJKMP_OPT_CATEGORY_SWR    FFP_OPT_CATEGORY_SWR

void            ijkmp_global_init();
void            ijkmp_global_uninit();
void            ijkmp_global_set_log_report(int use_report);
void            ijkmp_global_set_log_level(int log_level);
void            ijkmp_io_stat_register(void (*cb)(const char *url, int type, int bytes));
void            ijkmp_io_stat_complete_register(void (*cb)(const char *url,
                                                           int64_t read_bytes, int64_t total_size,
                                                           int64_t elpased_time, int64_t total_duration));

// ref_count is 1 after open
IjkMediaPlayer *ijkmp_create(int (*msg_loop)(void*), int crop);
void            ijkmp_set_format_callback(IjkMediaPlayer *mp, void *opaque);
void            ijkmp_set_format_option(IjkMediaPlayer *mp, const char *name, const char *value);
void            ijkmp_set_codec_option(IjkMediaPlayer *mp, const char *name, const char *value);
void            ijkmp_set_sws_option(IjkMediaPlayer *mp, const char *name, const char *value);
void            ijkmp_set_overlay_format(IjkMediaPlayer *mp, int chroma_fourcc);
void            ijkmp_set_picture_queue_capicity(IjkMediaPlayer *mp, int frame_count);
void            ijkmp_set_max_fps(IjkMediaPlayer *mp, int max_fps);
void            ijkmp_set_framedrop(IjkMediaPlayer *mp, int framedrop);
void            ijkmp_set_videotoolbox(IjkMediaPlayer *mp, int videotoolbox);
void            ijkmp_set_loop_number(IjkMediaPlayer *mp, int loop_number);
void            ijkmp_set_video_disable(IjkMediaPlayer *mp,int video_disable);
void            ijmp_set_start_seek_pos(IjkMediaPlayer *mp, int startpos);


void            ijkmp_set_option(IjkMediaPlayer *mp, int opt_category, const char *name, const char *value);
void            ijkmp_set_option_int(IjkMediaPlayer *mp, int opt_category, const char *name, int64_t value);


int             ijkmp_get_video_codec_info(IjkMediaPlayer *mp, char **codec_info);
int             ijkmp_get_audio_codec_info(IjkMediaPlayer *mp, char **codec_info);

void            ijkmp_set_playback_rate(IjkMediaPlayer *mp, float rate);
void            ijkmp_set_playback_volume(IjkMediaPlayer *mp, float rate);

float           ijkmp_get_property_float(IjkMediaPlayer *mp, int id, float default_value);
void            ijkmp_set_property_float(IjkMediaPlayer *mp, int id, float value);

// must be freed with free();
IjkMediaMeta   *ijkmp_get_meta_l(IjkMediaPlayer *mp);

// preferred to be called explicity, can be called multiple times
// NOTE: ijkmp_shutdown may block thread
void            ijkmp_shutdown(IjkMediaPlayer *mp);
void            ijkmp_destroy(IjkMediaPlayer *mp);
void            ijkmp_async_release(IjkMediaPlayer *mp);
void            ijkmp_sync_release(IjkMediaPlayer *mp);

void            ijkmp_inc_ref(IjkMediaPlayer *mp);

// call close at last release, also free memory
// NOTE: ijkmp_dec_ref may block thread
void            ijkmp_dec_ref(IjkMediaPlayer *mp);
void            ijkmp_dec_ref_p(IjkMediaPlayer **pmp);

int             ijkmp_set_data_source(IjkMediaPlayer *mp, const char *url);
int             ijkmp_prepare_async(IjkMediaPlayer *mp);
int             ijkmp_start(IjkMediaPlayer *mp);
void            ijkmp_resumedisplay(IjkMediaPlayer *mp);
void            ijkmp_pausedisplay(IjkMediaPlayer *mp);
void            ijkmp_set_play_control_parameters(IjkMediaPlayer* mp, bool canfwd, bool fwdnew, int buffertime, int fwdexttime, int minjitter, int maxjitter);
void            ijkmp_set_radical_real_time(IjkMediaPlayer *mp, bool radical);
void            ijkmp_set_realtime_flag(IjkMediaPlayer *mp,bool realtime);
void            ijkmp_set_crop_mode(IjkMediaPlayer *mp, bool crop, int surface_width, int surface_height);
int             ijkmp_pause(IjkMediaPlayer *mp);
int             ijkmp_stop(IjkMediaPlayer *mp);
int             ijkmp_seek_to(IjkMediaPlayer *mp, long msec);
int             ijkmp_get_state(IjkMediaPlayer *mp);
bool            ijkmp_is_playing(IjkMediaPlayer *mp);
long            ijkmp_get_current_position(IjkMediaPlayer *mp);
long            ijkmp_get_duration(IjkMediaPlayer *mp);
long            ijkmp_get_playable_duration(IjkMediaPlayer *mp);

void           *ijkmp_get_weak_thiz(IjkMediaPlayer *mp);
void           *ijkmp_set_weak_thiz(IjkMediaPlayer *mp, void *weak_thiz);

StatInfo       *ijkmp_get_stat_info(IjkMediaPlayer *mp);
void            ijkmp_capture_frame(IjkMediaPlayer *mp);
const uint8_t    *ijkmp_get_capture_frame_data(IjkMediaPlayer *mp);
/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int             ijkmp_get_msg(IjkMediaPlayer *mp, AVMessage *msg, int block);

void            ijkmp_set_display_frame_cb(IjkMediaPlayer *mp, OnDisplayFrameCb handle, void *obj);
bool            ijkmp_is_realplay(IjkMediaPlayer *mp);
void            ijkmp_set_muted(IjkMediaPlayer* mp, int mute);

void            ijkmp_set_language(IjkMediaPlayer *mp, const char *audioLanguage, const char *subtitleLanguage);
void            ijkmp_set_sub_display(IjkMediaPlayer *mp, bool enable);

void            ijkmp_set_save_to_local(IjkMediaPlayer *mp,const char *path);

#endif
