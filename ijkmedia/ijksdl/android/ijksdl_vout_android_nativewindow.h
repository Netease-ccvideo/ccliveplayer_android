/*****************************************************************************
 * ijksdl_vout_android_nativewindow.h
 *****************************************************************************
 *
 * copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
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

#ifndef IJKSDL_ANDROID__IJKSDL_VOUT_ANDROID_NATIVEWINDOW_H
#define IJKSDL_ANDROID__IJKSDL_VOUT_ANDROID_NATIVEWINDOW_H

#include "../ijksdl_stdinc.h"
#include "../ijksdl_vout.h"

#define GL_FRAME_EVENT_ON_BIND_BUFFER 8000
#define GL_FRAME_EVENT_ON_PRE_RENDER_BUFFER 8001
#define GL_FRAME_EVENT_ON_POST_RENDER_BUFFER 8002
#define AMEDIACODEC__BUFFER_FLAG_FAKE_FRAME 0x1000
typedef struct ANativeWindow   ANativeWindow;
typedef struct SDL_AMediaCodec SDL_AMediaCodec;

typedef struct SDL_AMediaCodecBufferInfo SDL_AMediaCodecBufferInfo;
typedef struct SDL_AMediaCodecBufferProxy SDL_AMediaCodecBufferProxy;
SDL_Vout *SDL_VoutAndroid_CreateForANativeWindow(void *mp, bool(*post_redraw_msg)(void*), bool(*OnRawImageAvailable)(void *arg, unsigned char**, int*, int, int, int, int), int nPanorama);
void SDL_VoutAndroid_SetNativeWindow(SDL_Vout *vout, ANativeWindow *native_window);
void SDL_VoutAndroid_setAMediaCodec(SDL_Vout *vout, SDL_AMediaCodec *acodec);
void SDL_VoutAndroid_setAMediaCodecWithoutLock(SDL_Vout *vout, SDL_AMediaCodec *acodec);
void SDL_VoutAndroid_SetGLFrameCallback(SDL_Vout *vout, int (*gl_frame_callback)(void*, int));

void SDL_VoutAndroid_OnGLSurfaceViewCreated(SDL_Vout *vout);
void SDL_VoutAndroid_OnGLSurfaceViewChanged(SDL_Vout *vout, int width, int height);
void SDL_VoutAndroid_redraw(SDL_Vout *vout);
void SDL_VoutAndroid_redrawTexture(SDL_Vout *vout, int widht, int height);
SDL_AMediaCodecBufferProxy *SDL_VoutAndroid_obtainBufferProxy(SDL_Vout *vout, int buffer_index, SDL_AMediaCodecBufferInfo *buffer_info);
SDL_AMediaCodec *SDL_VoutAndroid_peekAMediaCodec(SDL_Vout *vout);
int SDL_VoutAndroid_releaseBufferProxy_Vout(SDL_AMediaCodecBufferProxy ** proxy, SDL_Vout *vout, bool render);
int SDL_VoutAndroid_releaseBufferProxy_Vout_Wihtoutlock(SDL_AMediaCodecBufferProxy ** proxy, SDL_Vout *vout, bool render);
#endif
