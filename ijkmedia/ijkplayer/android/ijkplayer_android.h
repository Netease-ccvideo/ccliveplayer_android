/*
 * ijkplayer_android.h
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

#ifndef IJKPLAYER_ANDROID__IJKPLAYER_ANDROID_H
#define IJKPLAYER_ANDROID__IJKPLAYER_ANDROID_H

#include <jni.h>
#include "ijkplayer_android_def.h"
#include "../ijkplayer.h"

typedef struct ijkmp_android_media_format_context {
    const char *mime_type;
    int         profile;
    int         level;
} ijkmp_android_media_format_context;

// ref_count is 1 after open
IjkMediaPlayer *ijkmp_android_create(int(*msg_loop)(void*), int crop, int nPanorama,bool(*post_redraw_msg)(void*), bool(*OnRawImageAvailable)(void *arg, unsigned char**, int*, int, int, int, int));

void ijkmp_android_set_surface(JNIEnv *env, IjkMediaPlayer *mp, jobject android_surface);
void ijkmp_android_set_volume(JNIEnv *env, IjkMediaPlayer *mp, float left, float right);
void ijkmp_android_set_mediacodec_select_callback(IjkMediaPlayer *mp, bool (*callback)(void *opaque, ijkmp_mediacodecinfo_context *mcc), void *opaque);
void ijkmp_android_set_video_select_callback(IjkMediaPlayer *mp, bool (*callback)(void *opaque, const char *type, int rawdata), void *opaque);
void ijkmp_android_set_mediacodec_enabled(IjkMediaPlayer *mp, bool enabled);
void ijkmp_android_set_opensles_enabled(IjkMediaPlayer *mp, bool enabled);

void ijkmp_android_on_glsurface_created(IjkMediaPlayer *mp);
void ijkmp_android_on_glsurface_changed(IjkMediaPlayer *mp, int width, int height);
void ijkmp_android_redraw(IjkMediaPlayer *mp);
void ijkmp_android_redraw_texture(IjkMediaPlayer *mp, int w, int h);
void ijkmp_android_sendHttpStat(IjkMediaPlayer *mp, const char *url);
void ijkmp_andriod_set_decode_raw_data_enable(IjkMediaPlayer *mp, int enable);

#endif
