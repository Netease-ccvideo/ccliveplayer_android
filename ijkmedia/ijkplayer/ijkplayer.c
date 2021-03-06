/*
 * ijkplayer.c
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

#include "ijkplayer.h"

#include "ijkplayer_internal.h"

#define MP_RET_IF_FAILED(ret) \
    do { \
        int retval = ret; \
        if (retval != 0) return (retval); \
    } while(0)

#define MPST_RET_IF_EQ_INT(real, expected, errcode) \
    do { \
        if ((real) == (expected)) return (errcode); \
    } while(0)

#define MPST_RET_IF_EQ(real, expected) \
    MPST_RET_IF_EQ_INT(real, expected, EIJK_INVALID_STATE)

void ijkmp_destroy(IjkMediaPlayer *mp)
{
    ALOGF("ijkmp_destroy \n");
    if (!mp)
        return;

    ffp_destroy_p(&mp->ffplayer);

    if (mp->msg_thread) {
        SDL_WaitThread(mp->msg_thread, NULL);
        mp->msg_thread = NULL;
    }
    // 修正get_message崩溃，msg_queue的mutex应在msg线程结束后才销毁
    msg_queue_destroy(&mp->ffplayer->msg_queue);
    av_free(mp->ffplayer);

    pthread_mutex_destroy(&mp->mutex);

    av_freep(&mp->data_source);
    memset(mp, 0, sizeof(IjkMediaPlayer));
    av_freep(&mp);
    ALOGF("ijkmp_destroy done\n");
}

inline static void ijkmp_destroy_p(IjkMediaPlayer **pmp)
{
    if (!pmp)
        return;

    ijkmp_destroy(*pmp);
    *pmp = NULL;
}

void ijkmp_global_init()
{
    ffp_global_init();
}

void ijkmp_global_uninit()
{
    ffp_global_uninit();
}

void ijkmp_global_set_log_report(int use_report)
{
    ffp_global_set_log_report(use_report);
}

void ijkmp_global_set_log_level(int log_level)
{
    ffp_global_set_log_level(log_level);
}

void ijkmp_io_stat_register(void (*cb)(const char *url, int type, int bytes))
{
    ffp_io_stat_register(cb);
}

void ijkmp_io_stat_complete_register(void (*cb)(const char *url,
                                                int64_t read_bytes, int64_t total_size,
                                                int64_t elpased_time, int64_t total_duration))
{
    ffp_io_stat_complete_register(cb);
}

void ijkmp_change_state_l(IjkMediaPlayer *mp, int new_state)
{
    mp->mp_state = new_state;
    ffp_notify_msg1(mp->ffplayer, FFP_MSG_PLAYBACK_STATE_CHANGED);
}

IjkMediaPlayer *ijkmp_create(int (*msg_loop)(void*), int crop)
{
    ALOGF("ijkmp_create \n");
    IjkMediaPlayer *mp = (IjkMediaPlayer *) av_mallocz(sizeof(IjkMediaPlayer));
    if (!mp)
        goto fail;

    mp->ffplayer = ffp_create(crop);
    if (!mp->ffplayer)
        goto fail;

    mp->msg_loop = msg_loop;
    mp->release_thread = NULL;

    ijkmp_inc_ref(mp);
    pthread_mutex_init(&mp->mutex, NULL);
    ALOGF("ijkmp_create done\n");
    return mp;

    fail:
    ijkmp_destroy_p(&mp);
    return NULL;
}

void ijkmp_set_overlay_format(IjkMediaPlayer *mp, int chroma_fourcc)
{
    if (!mp)
        return;

    MPTRACE("ijkmp_set_overlay_format(%.4s(0x%x))\n", (char*)&chroma_fourcc, chroma_fourcc);
    if (mp->ffplayer) {
        ffp_set_overlay_format(mp->ffplayer, chroma_fourcc);
    }
    MPTRACE("ijkmp_set_overlay_format()=void\n");
}

void ijkmp_set_format_callback(IjkMediaPlayer *mp, void *opaque)
{
    assert(mp);

    MPTRACE("ijkmp_set_format_callback(%p)\n", opaque);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_format_callback(mp->ffplayer, opaque);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_format_callback()=void\n");
}

void ijkmp_set_format_option(IjkMediaPlayer *mp, const char *name, const char *value)
{
    assert(mp);

    MPTRACE("ijkmp_set_format_option(%s, %s)\n", name, value);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_format_option(mp->ffplayer, name, value);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_format_option()=void\n");
}

void ijkmp_set_codec_option(IjkMediaPlayer *mp, const char *name, const char *value)
{
    assert(mp);

    MPTRACE("ijkmp_set_codec_option()\n");
    pthread_mutex_lock(&mp->mutex);
    ffp_set_codec_option(mp->ffplayer, name, value);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_codec_option()=void\n");
}

void ijkmp_set_sws_option(IjkMediaPlayer *mp, const char *name, const char *value)
{
    assert(mp);

    MPTRACE("ijkmp_set_sws_option()\n");
    pthread_mutex_lock(&mp->mutex);
    ffp_set_sws_option(mp->ffplayer, name, value);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_sws_option()=void\n");
}

void ijkmp_set_picture_queue_capicity(IjkMediaPlayer *mp, int frame_count)
{
    assert(mp);

    MPTRACE("ijkmp_set_picture_queue_capicity(%d)\n", frame_count);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_picture_queue_capicity(mp->ffplayer, frame_count);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_picture_queue_capicity()=void\n");
}

void ijkmp_set_max_fps(IjkMediaPlayer *mp, int max_fps)
{
    assert(mp);

    MPTRACE("ijkmp_set_max_fp(%d)\n", max_fps);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_max_fps(mp->ffplayer, max_fps);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_max_fp()=void\n");
}

void ijkmp_set_framedrop(IjkMediaPlayer *mp, int framedrop)
{
    assert(mp);

    MPTRACE("ijkmp_set_framedrop(%d)\n", framedrop);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_framedrop(mp->ffplayer, framedrop);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_framedrop()=void\n");
}

void ijmp_set_start_seek_pos(IjkMediaPlayer *mp, int startpos)
{
    assert(mp);
    MPTRACE("ijmp_set_start_seek_pos(%d)\n", startpos);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_start_seek_pos(mp->ffplayer, startpos);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijmp_set_start_seek_pos=void\n");
}

void ijkmp_set_videotoolbox(IjkMediaPlayer *mp, int videotoolbox)
{
    assert(mp);
    
    MPTRACE("ijkmp_set_videotoolbox(%d)\n", videotoolbox);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_videotoolbox(mp->ffplayer, videotoolbox);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_videotoolbox()=void\n");
}

void ijkmp_set_loop_number(IjkMediaPlayer *mp, int loop_number)
{
    assert(mp);
    
    MPTRACE("ijkmp_set_loop_number(%d)\n", loop_number);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_loop_number(mp->ffplayer, loop_number);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_loop_number()=void\n");
}

void ijkmp_set_video_disable(IjkMediaPlayer *mp,int video_disable)
{
    assert(mp);
    MPTRACE("ijkmp_set_video_disable(%d)\n",video_disable);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_disable_video(mp->ffplayer,video_disable);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_option(IjkMediaPlayer *mp, int opt_category, const char *name, const char *value)
{
    assert(mp);
    
    // MPTRACE("%s(%s, %s)\n", __func__, name, value);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_option(mp->ffplayer, opt_category, name, value);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s()=void\n", __func__);
}

void ijkmp_set_option_int(IjkMediaPlayer *mp, int opt_category, const char *name, int64_t value)
{
    assert(mp);
    
    // MPTRACE("%s(%s, %"PRId64")\n", __func__, name, value);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_option_int(mp->ffplayer, opt_category, name, value);
    pthread_mutex_unlock(&mp->mutex);
    // MPTRACE("%s()=void\n", __func__);
}

int ijkmp_get_video_codec_info(IjkMediaPlayer *mp, char **codec_info)
{
    assert(mp);

    MPTRACE("%s\n", __func__);
    pthread_mutex_lock(&mp->mutex);
    int ret = ffp_get_video_codec_info(mp->ffplayer, codec_info);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("%s()=void\n", __func__);
    return ret;
}

int ijkmp_get_audio_codec_info(IjkMediaPlayer *mp, char **codec_info)
{
    assert(mp);

    MPTRACE("%s\n", __func__);
    pthread_mutex_lock(&mp->mutex);
    int ret = ffp_get_audio_codec_info(mp->ffplayer, codec_info);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("%s()=void\n", __func__);
    return ret;
}

void ijkmp_set_playback_rate(IjkMediaPlayer *mp, float rate)
{
    assert(mp);
    
    MPTRACE("%s(%f)\n", __func__, rate);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_playback_rate(mp->ffplayer, rate);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("%s()=void\n", __func__);
}

void ijkmp_set_playback_volume(IjkMediaPlayer *mp, float volume)
{
    assert(mp);
    
    MPTRACE("%s(%f)\n", __func__, volume);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_playback_volume(mp->ffplayer, volume);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("%s()=void\n", __func__);
}

float ijkmp_get_property_float(IjkMediaPlayer *mp, int id, float default_value)
{
    assert(mp);
    
    pthread_mutex_lock(&mp->mutex);
    float ret = ffp_get_property_float(mp->ffplayer, id, default_value);
    pthread_mutex_unlock(&mp->mutex);
    return ret;
}

void ijkmp_set_property_float(IjkMediaPlayer *mp, int id, float value)
{
    assert(mp);
    
    pthread_mutex_lock(&mp->mutex);
    ffp_set_property_float(mp->ffplayer, id, value);
    pthread_mutex_unlock(&mp->mutex);
}

IjkMediaMeta *ijkmp_get_meta_l(IjkMediaPlayer *mp)
{
    assert(mp);

    MPTRACE("%s\n", __func__);
    IjkMediaMeta *ret = ffp_get_meta_l(mp->ffplayer);
    MPTRACE("%s()=void\n", __func__);
    return ret;
}

void ijkmp_shutdown_l(IjkMediaPlayer *mp)
{
    ALOGF("ijkmp_shutdown_l \n");
    assert(mp);

    MPTRACE("ijkmp_shutdown_l()\n");
    if (mp->ffplayer) {
        ffp_stop_l(mp->ffplayer);
        ffp_wait_stop_l(mp->ffplayer);
    }
    MPTRACE("ijkmp_shutdown_l()=void\n");
    ALOGF("ijkmp_shutdown_l done\n");
}

void ijkmp_shutdown(IjkMediaPlayer *mp)
{
    return ijkmp_shutdown_l(mp);
}

static int async_release(void *arg) {
            MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = arg;
    ijkmp_shutdown(mp);
    ijkmp_destroy(mp);
    return 0;
}

void ijkmp_async_release(IjkMediaPlayer *mp) {
    SDL_CreateThreadEx(&mp->_release_thread, async_release, mp, "release_tid");
}

void ijkmp_sync_release(IjkMediaPlayer *mp) {
    ijkmp_shutdown(mp);
    ijkmp_destroy(mp);
}

void ijkmp_inc_ref(IjkMediaPlayer *mp)
{
    assert(mp);
    __sync_fetch_and_add(&mp->ref_count, 1);
}

void ijkmp_dec_ref(IjkMediaPlayer *mp)
{
    if (!mp)
        return;

    int ref_count = __sync_sub_and_fetch(&mp->ref_count, 1);
    if (ref_count == 0) {
        MPTRACE("ijkmp_dec_ref(): ref=0\n");
        ijkmp_shutdown(mp);
        ijkmp_destroy_p(&mp);
    }
}

void ijkmp_dec_ref_p(IjkMediaPlayer **pmp)
{
    if (!pmp)
        return;

    ijkmp_dec_ref(*pmp);
    *pmp = NULL;
}

static int ijkmp_set_data_source_l(IjkMediaPlayer *mp, const char *url)
{
    assert(mp);
    assert(url);

    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    av_freep(&mp->data_source);
    mp->data_source = av_strdup(url);
    if (!mp->data_source)
        return EIJK_OUT_OF_MEMORY;

    ijkmp_change_state_l(mp, MP_STATE_INITIALIZED);
    return 0;
}

int ijkmp_set_data_source(IjkMediaPlayer *mp, const char *url)
{
    assert(mp);
    assert(url);
    MPTRACE("ijkmp_set_data_source(url=\"%s\") \n", url);
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_set_data_source_l(mp, url);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static int ijkmp_prepare_async_l(IjkMediaPlayer *mp)
{
    assert(mp);

    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    assert(mp->data_source);

    ijkmp_change_state_l(mp, MP_STATE_ASYNC_PREPARING);

    msg_queue_start(&mp->ffplayer->msg_queue);

    // released in msg_loop
    ijkmp_inc_ref(mp);
    mp->msg_thread = SDL_CreateThreadEx(&mp->_msg_thread, mp->msg_loop, mp, "ff_msg_loop");
    // TODO: 9 release weak_thiz if pthread_create() failed;

    int retval = ffp_prepare_async_l(mp->ffplayer, mp->data_source);
    if (retval < 0) {
        ijkmp_change_state_l(mp, MP_STATE_ERROR);
        return retval;
    }

    return 0;
}

int ijkmp_prepare_async(IjkMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ijkmp_prepare_async()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_prepare_async_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static int ikjmp_chkst_start_l(int mp_state)
{
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ijkmp_start_l(IjkMediaPlayer *mp)
{
    assert(mp);

    MP_RET_IF_FAILED(ikjmp_chkst_start_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_START);

    return 0;
}

int ijkmp_start(IjkMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ijkmp_start()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_start_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_start()=%d\n", retval);
    return retval;
}

static int ikjmp_chkst_pause_l(int mp_state)
{
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ijkmp_pause_l(IjkMediaPlayer *mp)
{
    assert(mp);

    MP_RET_IF_FAILED(ikjmp_chkst_pause_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_PAUSE);

    return 0;
}

int ijkmp_pause(IjkMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ijkmp_pause()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_pause_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_pause()=%d\n", retval);
    return retval;
}

void ijkmp_resumedisplay(IjkMediaPlayer *mp)
{
    MPTRACE("%s", __func__);
    pthread_mutex_lock(&mp->mutex);
    if (mp->ffplayer->display_disable) {
//        ffp_picture_queue_clear(mp->ffplayer);
        mp->ffplayer->display_disable = 0;
        mp->ffplayer->display_ready = 0;
    }
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_pausedisplay(IjkMediaPlayer *mp)
{
    MPTRACE("%s", __func__);
    pthread_mutex_lock(&mp->mutex);
    if (!mp->ffplayer->display_disable) {
        mp->ffplayer->display_disable = 1;
    }
    pthread_mutex_unlock(&mp->mutex);
}

static int ijkmp_stop_l(IjkMediaPlayer *mp)
{
    assert(mp);

    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    int retval = ffp_stop_l(mp->ffplayer);
    if (retval < 0) {
        return retval;
    }

    ijkmp_change_state_l(mp, MP_STATE_STOPPED);
    return 0;
}

int ijkmp_stop(IjkMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ijkmp_stop()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_stop_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_stop()=%d\n", retval);
    return retval;
}

bool ijkmp_is_playing(IjkMediaPlayer *mp)
{
    assert(mp);
    if (mp->mp_state == MP_STATE_PREPARED ||
        mp->mp_state == MP_STATE_STARTED) {
        return true;
    }

    return false;
}

static int ikjmp_chkst_seek_l(int mp_state)
{
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

int ijkmp_seek_to_l(IjkMediaPlayer *mp, long msec)
{
    assert(mp);

    MP_RET_IF_FAILED(ikjmp_chkst_seek_l(mp->mp_state));

    mp->seek_req = 1;
    mp->seek_msec = msec;
    ffp_remove_msg(mp->ffplayer, FFP_REQ_SEEK);
    ffp_notify_msg2(mp->ffplayer, FFP_REQ_SEEK, (int)msec);
    // TODO: 9 64-bit long?

    return 0;
}

int ijkmp_seek_to(IjkMediaPlayer *mp, long msec)
{
    assert(mp);
    MPTRACE("ijkmp_seek_to(%ld)\n", msec);
    pthread_mutex_lock(&mp->mutex);
    int retval = ijkmp_seek_to_l(mp, msec);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_seek_to(%ld)=%d\n", msec, retval);

    return retval;
}

int ijkmp_get_state(IjkMediaPlayer *mp)
{
    return mp->mp_state;
}

static long ijkmp_get_current_position_l(IjkMediaPlayer *mp)
{
    return ffp_get_current_position_l(mp->ffplayer);
}

long ijkmp_get_current_position(IjkMediaPlayer *mp)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    long retval;
    if (mp->seek_req)
        retval = mp->seek_msec;
    else
        retval = ijkmp_get_current_position_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static long ijkmp_get_duration_l(IjkMediaPlayer *mp)
{
    return ffp_get_duration_l(mp->ffplayer);
}

long ijkmp_get_duration(IjkMediaPlayer *mp)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    long retval = ijkmp_get_duration_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static long ijkmp_get_playable_duration_l(IjkMediaPlayer *mp)
{
    return ffp_get_playable_duration_l(mp->ffplayer);
}

long ijkmp_get_playable_duration(IjkMediaPlayer *mp)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    long retval = ijkmp_get_playable_duration_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

void *ijkmp_get_weak_thiz(IjkMediaPlayer *mp)
{
    return mp->weak_thiz;
}

void *ijkmp_set_weak_thiz(IjkMediaPlayer *mp, void *weak_thiz)
{
    void *prev_weak_thiz = mp->weak_thiz;

    mp->weak_thiz = weak_thiz;

    mp->ffplayer->weak_thiz = weak_thiz;

    return prev_weak_thiz;
}

int ijkmp_get_msg(IjkMediaPlayer *mp, AVMessage *msg, int block)
{
    assert(mp);
    while (1) {
        int continue_wait_next_msg = 0;
        int retval = msg_queue_get(&mp->ffplayer->msg_queue, msg, block);
        if (retval <= 0)
            return retval;

        switch (msg->what) {
        case FFP_MSG_PREPARED:
            MPTRACE("ijkmp_get_msg: FFP_MSG_PREPARED\n");
            pthread_mutex_lock(&mp->mutex);
            if (mp->mp_state == MP_STATE_ASYNC_PREPARING) {
                ijkmp_change_state_l(mp, MP_STATE_PREPARED);
            } else {
                // FIXME: 1: onError() ?
                ALOGE("FFP_MSG_PREPARED: expecting mp_state==MP_STATE_ASYNC_PREPARING\n");
            }
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_MSG_COMPLETED:
            MPTRACE("ijkmp_get_msg: FFP_MSG_COMPLETED\n");

            pthread_mutex_lock(&mp->mutex);
            mp->restart_from_beginning = 1;
            ijkmp_change_state_l(mp, MP_STATE_COMPLETED);
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_MSG_SEEK_COMPLETE:
            MPTRACE("ijkmp_get_msg: FFP_MSG_SEEK_COMPLETE\n");

            pthread_mutex_lock(&mp->mutex);
            mp->seek_req = 0;
            mp->seek_msec = 0;
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_REQ_START:
            MPTRACE("ijkmp_get_msg: FFP_REQ_START\n");
            continue_wait_next_msg = 1;
            pthread_mutex_lock(&mp->mutex);
            if (0 == ikjmp_chkst_start_l(mp->mp_state)) {
                // FIXME: 8 check seekable
                if (mp->mp_state == MP_STATE_COMPLETED) {
                    if (mp->restart_from_beginning) {
                        ALOGI("ijkmp_get_msg: FFP_REQ_START: restart from beginning\n");
                        retval = ffp_start_from_l(mp->ffplayer, 0);
                        if (retval == 0)
                            ijkmp_change_state_l(mp, MP_STATE_STARTED);
                    } else {
                        ALOGI("ijkmp_get_msg: FFP_REQ_START: restart from seek pos\n");
                        retval = ffp_start_l(mp->ffplayer);
                        if (retval == 0)
                            ijkmp_change_state_l(mp, MP_STATE_STARTED);
                    }
                    mp->restart_from_beginning = 0;
                } else {
                    ALOGI("ijkmp_get_msg: FFP_REQ_START: start on fly\n");
                    retval = ffp_start_l(mp->ffplayer);
                    if (retval == 0)
                        ijkmp_change_state_l(mp, MP_STATE_STARTED);
                }
            }
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_REQ_PAUSE:
            MPTRACE("ijkmp_get_msg: FFP_REQ_PAUSE\n");
            continue_wait_next_msg = 1;
            pthread_mutex_lock(&mp->mutex);
            if (0 == ikjmp_chkst_pause_l(mp->mp_state)) {
                int pause_ret = ffp_pause_l(mp->ffplayer);
                if (pause_ret == 0)
                    ijkmp_change_state_l(mp, MP_STATE_PAUSED);
            }
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_REQ_SEEK:
            MPTRACE("ijkmp_get_msg: FFP_REQ_SEEK\n");
            continue_wait_next_msg = 1;

            pthread_mutex_lock(&mp->mutex);
            if (0 == ikjmp_chkst_seek_l(mp->mp_state)) {
                if (0 == ffp_seek_to_l(mp->ffplayer, msg->arg1)) {
                    ALOGI("ijkmp_get_msg: FFP_REQ_SEEK: seek to %d\n", (int)msg->arg1);
                    mp->restart_from_beginning = 0;
                }
            }
            pthread_mutex_unlock(&mp->mutex);
            break;
        }

        if (continue_wait_next_msg){
            msg_free_res(msg);
            continue;
        }

        return retval;
    }

    return -1;
}

StatInfo *ijkmp_get_stat_info(IjkMediaPlayer *mp)
{
    return ffp_get_stat_info(mp->ffplayer);
}

void ijkmp_set_play_control_parameters(IjkMediaPlayer* mp, bool realtime, bool fwdnew, int buffertime, int fwdexttime, int minjitter, int maxjitter)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    bool cur_real_time = mp->ffplayer->realtime_play;
    int cur_min_jitter = mp->ffplayer->buffering_target_duration_ms_min;
    int cur_max_jitter = mp->ffplayer->buffering_target_duration_ms_max;
    if (cur_real_time != realtime
        || cur_min_jitter != minjitter
        || cur_max_jitter != maxjitter) {
        ALOGW("%s canfwd=%d, fwdnew=%d, buffertime=%d, fwdexttime=%d, minjitter=%d, maxjitter=%d", __func__, realtime, fwdnew, buffertime, fwdexttime, minjitter, maxjitter);
        if (realtime) {
            //reset realtime parameters
            mp->ffplayer->realtime_last_audio_packet_time = 0;
            mp->ffplayer->realtime_audio_packet_count = 0;
            mp->ffplayer->realtime_video_packet_count = 0;
            
            mp->ffplayer->buffering_target_duration_ms_min = minjitter;
            mp->ffplayer->buffering_target_duration_ms_max = maxjitter;
            mp->ffplayer->buffering_target_duration_ms = minjitter;
        } else {
            //reset realtime parameters
            mp->ffplayer->realtime_last_audio_packet_time = 0;
            mp->ffplayer->realtime_audio_packet_count = 0;
            mp->ffplayer->realtime_video_packet_count = 0;
        }

        mp->ffplayer->realtime_play = realtime;
    }
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_radical_real_time(IjkMediaPlayer *mp, bool radical) {
    if (mp && mp->ffplayer) {
        mp->ffplayer->realtime_play = true;
        if(radical)
            mp->ffplayer->radical_realtime_level = RadicalLevel_1;
        //mp->ffplayer->radical_realtime = radical;
    }
}

void  ijkmp_set_realtime_flag(IjkMediaPlayer *mp,bool realtime)
{
    if(mp && mp->ffplayer)
    {
        mp->ffplayer->realtime_play = realtime;
    }
}

void ijkmp_set_crop_mode(IjkMediaPlayer *mp, bool crop, int surface_width, int surface_height)
{
    ALOGI("%s crop=%d, surface_width=%d, surface_height=%d", __func__, crop, surface_width, surface_height);
    ffp_set_crop_mode(mp->ffplayer, crop, surface_width, surface_height);
}

void ijkmp_set_display_frame_cb(IjkMediaPlayer *mp, OnDisplayFrameCb handle, void *obj)
{
    ffp_set_display_frame_cb(mp->ffplayer, handle, obj);
}

void ijkmp_capture_frame(IjkMediaPlayer *mp)
{
    ALOGI("%s", __func__);
    ffp_capture_frame(mp->ffplayer);
}

const uint8_t *ijkmp_get_capture_frame_data(IjkMediaPlayer *mp)
{
    if (mp->ffplayer) {
        return mp->ffplayer->is->capture_frame_data;
    }
    return NULL;
}

bool ijkmp_is_realplay(IjkMediaPlayer *mp)
{
    return mp && mp->ffplayer && mp->ffplayer->realtime_play;
}

void  ijkmp_set_muted(IjkMediaPlayer* mp, int mute)
{
    assert(mp);
    MPTRACE("%s \n", __func__);
    pthread_mutex_lock(&mp->mutex);
    if (mp->ffplayer) {
        mp->ffplayer->muted = mute;
    }
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_set_language(IjkMediaPlayer *mp, const char *audioLanguage, const char *subtitleLanguage)
{
    assert(mp);
    ALOGI("%s audioLanguage = %s, subtitleLanguage = %s",__func__, audioLanguage, subtitleLanguage);
    ffp_set_language(mp->ffplayer, audioLanguage, subtitleLanguage);
}

void ijkmp_set_sub_display(IjkMediaPlayer *mp, bool enable)
{
    ALOGI("% display_subtitle = %d", __func__, enable);
    ffp_set_sub_display(mp->ffplayer, enable);
}

void ijkmp_set_save_to_local(IjkMediaPlayer *mp,const char *path)
{
    assert(mp);
    ALOGI("%s set_save_to_local = %s",__func__, path);
    ffp_set_save_to_local(mp->ffplayer, path);
}
