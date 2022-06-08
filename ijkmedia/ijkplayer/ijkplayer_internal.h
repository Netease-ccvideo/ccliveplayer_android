/*
 * ijkplayer_internal.h
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

#ifndef IJKPLAYER_ANDROID__IJKPLAYER_INTERNAL_H
#define IJKPLAYER_ANDROID__IJKPLAYER_INTERNAL_H

#include <assert.h>
#include "ijksdl/ijksdl.h"
#include "ff_fferror.h"
#include "ff_ffplay.h"
#include "ijkplayer.h"

typedef struct IjkMediaPlayer {
    volatile int ref_count;
    pthread_mutex_t mutex;
    FFPlayer *ffplayer;

    int (*msg_loop)(void*);
    SDL_Thread *msg_thread;
    SDL_Thread _msg_thread;

    SDL_Thread *release_thread;
    SDL_Thread _release_thread;

    int mp_state;
    char *data_source;
    void *weak_thiz;

    int restart_from_beginning;
    int seek_req;
    long seek_msec;
} IjkMediaPlayer;

#ifdef __ANDROID__
typedef struct CCPlayerStat CCPlayerStat;
typedef struct player_fields_t {
    pthread_mutex_t mutex;
    jclass clazz;

    jfieldID mNativeMediaPlayer;
    jfieldID field_mNativeMediaDataSource;
    jfieldID mNativeCCPlayerStat;
    jfieldID mNativePlayerConfig;

    //for display frame callback
    jclass   mjdf_clzz;
    jfieldID mjdf_data;
    jfieldID mjdf_format;
    jfieldID mjdf_width;
    jfieldID mjdf_height;
    jfieldID mjdf_pitch;
    jfieldID mjdf_bits;

    jfieldID surface_texture;

    jmethodID jmid_postEventFromNative;
    jmethodID jmid_onSelectCodec;
    jmethodID jmid_onControlResolveSegmentCount;
    jmethodID jmid_onControlResolveSegmentUrl;
    jmethodID jmid_onControlResolveSegmentOfflineMrl;
    jmethodID jmid_onControlResolveSegmentDuration;
    jmethodID jmid_requestRedraw;
    jmethodID jmid_onRawImageAvailable;
    jmethodID jmid_sendHttpStat;
    jmethodID jmid_sendFFplayerState;
    jmethodID jmid_OnVideoDecoderSelectedFromNative;
} player_fields_t;

typedef struct ReleaseArgs{
    CCPlayerStat *cc_player_stat;
    IjkMediaPlayer *mp;
    jobject playerThiz;
} ReleaseArgs;
#endif  // __ANDROID__

#endif
