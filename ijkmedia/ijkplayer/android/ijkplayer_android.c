/*
 * ijkplayer_android.c
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

#include "ijkplayer_android.h"

#include <assert.h>
#include "ijksdl/android/ijksdl_android.h"
#include "../ff_fferror.h"
#include "../ff_ffplay.h"
#include "../ijkplayer_internal.h"
#include "../pipeline/ffpipeline_ffplay.h"
#include "pipeline/ffpipeline_android.h"
extern void IjkMediaPlayer_sendHttpStat(IjkMediaPlayer *mp, const char *url);

IjkMediaPlayer *ijkmp_android_create(int(*msg_loop)(void*), int crop, int nPanorama,bool(*post_redraw_msg)(void*), bool(*OnRawImageAvailable)(void *arg, unsigned char**, int*, int, int, int, int))
{
    IjkMediaPlayer *mp = ijkmp_create(msg_loop, crop);
    if (!mp)
        goto fail;

    mp->ffplayer->vout = SDL_VoutAndroid_CreateForAndroidSurface(mp, post_redraw_msg, OnRawImageAvailable, nPanorama);
    if (!mp->ffplayer->vout)
        goto fail;

    //mp->ffplayer->aout = SDL_AoutAndroid_CreateForOpenSLES();
    mp->ffplayer->aout = SDL_AoutAndroid_CreateForAudioTrack();
    if (!mp->ffplayer->aout)
        goto fail;

    mp->ffplayer->pipeline = ffpipeline_create_from_android(mp->ffplayer);
    if (!mp->ffplayer->pipeline)
        goto fail;

    ffpipeline_set_vout(mp->ffplayer->pipeline, mp->ffplayer->vout);

    return mp;

    fail:
    ijkmp_dec_ref_p(&mp);
    return NULL;
}

void ijkmp_android_set_surface_l(JNIEnv *env, IjkMediaPlayer *mp, jobject android_surface)
{
    if (!mp || !mp->ffplayer || !mp->ffplayer->vout)
        return;

    SDL_VoutAndroid_SetAndroidSurface(env, mp->ffplayer->vout, android_surface);
    ffpipeline_set_surface(env, mp->ffplayer->pipeline, android_surface);
}

void ijkmp_android_on_glsurface_created(IjkMediaPlayer *mp)
{
    pthread_mutex_lock(&mp->mutex);
    if (!mp || !mp->ffplayer || !mp->ffplayer->vout) {
        return;
    }
    SDL_VoutAndroid_OnGLSurfaceViewCreated(mp->ffplayer->vout);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_android_on_glsurface_changed(IjkMediaPlayer *mp, int width, int height)
{
    pthread_mutex_lock(&mp->mutex);
    if (!mp || !mp->ffplayer || !mp->ffplayer->vout) {
        return;
    }
    MPTRACE("on_glsurface_changed width=%d height=%d", width, height);
    SDL_VoutAndroid_OnGLSurfaceViewChanged(mp->ffplayer->vout, width, height);
    pthread_mutex_unlock(&mp->mutex);
}

void ijkmp_android_redraw(IjkMediaPlayer *mp)
{
    SDL_VoutAndroid_redraw(mp->ffplayer->vout);
}

void ijkmp_android_redraw_texture(IjkMediaPlayer *mp, int width, int height)
{
    if (!mp || !mp->ffplayer || !mp->ffplayer->vout) {
        return;
    }
    SDL_VoutAndroid_redrawTexture(mp->ffplayer->vout, width, height);
}

void ijkmp_android_set_surface(JNIEnv *env, IjkMediaPlayer *mp, jobject android_surface)
{
    if (!mp)
        return;

    MPTRACE("ijkmp_set_android_surface(surface=%p)", (void*)android_surface);
    pthread_mutex_lock(&mp->mutex);
    ijkmp_android_set_surface_l(env, mp, android_surface);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_set_android_surface(surface=%p)=void", (void*)android_surface);
}

void ijkmp_android_set_volume(JNIEnv *env, IjkMediaPlayer *mp, float left, float right)
{
    if (!mp)
        return;

    MPTRACE("ijkmp_android_set_volume(%f, %f)", left, right);
    pthread_mutex_lock(&mp->mutex);

    if (mp && mp->ffplayer && mp->ffplayer->aout) {
        SDL_AoutSetStereoVolume(mp->ffplayer->aout, left, right);
    }

    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_android_set_volume(%f, %f)=void", left, right);
}

void ijkmp_android_set_mediacodec_select_callback(IjkMediaPlayer *mp, bool (*callback)(void *opaque, ijkmp_mediacodecinfo_context *mcc), void *opaque)
{
    if (!mp)
        return;

    MPTRACE("ijkmp_android_set_mediacodec_select_callback()");
    pthread_mutex_lock(&mp->mutex);

    if (mp && mp->ffplayer && mp->ffplayer->pipeline) {
        ffpipeline_set_mediacodec_select_callback(mp->ffplayer->pipeline, callback, opaque);
    }

    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_android_set_mediacodec_select_callback()=void");
}

void ijkmp_android_set_video_select_callback(IjkMediaPlayer *mp, bool (*callback)(void *opaque, const char *type, int rawdata), void *opaque)
{
    if (!mp)
        return;

    MPTRACE("ijkmp_android_set_video_select_callback()");
    pthread_mutex_lock(&mp->mutex);

    if (mp && mp->ffplayer && mp->ffplayer->pipeline) {
        ffpipeline_set_video_select_callback(mp->ffplayer->pipeline, callback, opaque);
    }

    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_android_set_video_select_callback()=void");
}


void ijkmp_android_set_mediacodec_enabled(IjkMediaPlayer *mp, bool enabled)
{
    if (!mp)
         return;

    MPTRACE("ijkmp_android_set_mediacodec_enabled(%d)", enabled ? 1 : 0);
    pthread_mutex_lock(&mp->mutex);

    if (mp && mp->ffplayer && mp->ffplayer->pipeline) {
        ffpipeline_set_mediacodec_enabled(mp->ffplayer->pipeline, enabled);
    }

    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_android_set_mediacodec_enabled()=void");
}

void ijkmp_android_set_opensles_enabled(IjkMediaPlayer *mp, bool enabled)
{
    if (!mp)
         return;

    MPTRACE("ijkmp_android_set_opensles_enabled(%d)", enabled ? 1 : 0);
    pthread_mutex_lock(&mp->mutex);

    if (mp) {
        if (enabled) {
            if (!SDL_AoutAndroid_IsObjectOfOpenSLES(mp->ffplayer->aout)) {
                ALOGI("recreate aout for OpenSL ES\n");
                SDL_AoutFreeP(&mp->ffplayer->aout);
                mp->ffplayer->aout = SDL_AoutAndroid_CreateForOpenSLES();
            }
        } else {
            if (!SDL_AoutAndroid_IsObjectOfAudioTrack(mp->ffplayer->aout)) {
                ALOGI("recreate aout for AudioTrack\n");
                SDL_AoutFreeP(&mp->ffplayer->aout);
                mp->ffplayer->aout = SDL_AoutAndroid_CreateForAudioTrack();
            }
        }
    }

    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ijkmp_android_set_opensles_enabled()=void");
}

void ijkmp_android_sendHttpStat(IjkMediaPlayer *mp, const char *url)
{
    IjkMediaPlayer_sendHttpStat(mp, url);
}

void ijkmp_andriod_set_decode_raw_data_enable(IjkMediaPlayer *mp, int enable) {
    if(mp && mp->ffplayer && mp->ffplayer->vout) {
        mp->ffplayer->vout->decode_out_raw_data = enable;
        SDL_VoutAndroid_DecoderRawData(mp->ffplayer->vout, enable);
        ALOGI("[raw] set decode raw data %d", enable);
    }
}