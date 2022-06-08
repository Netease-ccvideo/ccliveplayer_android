/*
 * ijkplayer_jni.c
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

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <jni.h>
#include "ijkutil/ijkutil.h"
#include "ijkutil/android/ijkutil_android.h"
#include "../ff_ffplay.h"
#include "../ijkplayer_internal.h"
#include "ffmpeg_api_jni.h"
#include "ijkplayer_android_def.h"
#include "ijkplayer_android.h"
#include "ijksdl/android/android_arraylist.h"
#include "ijksdl/android/android_bundle.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include "ijksdl/android/ijksdl_codec_android_mediadef.h"

#define JNI_MODULE_PACKAGE      "tv/danmaku/ijk/media/player"
#define JNI_CLASS_IJKPLAYER     "tv/danmaku/ijk/media/player/IjkMediaPlayer"
#define JNI_IJK_MEDIA_EXCEPTION "tv/danmaku/ijk/media/player/IjkMediaException"

#define IJK_CHECK_MPRET_GOTO(retval, env, label) \
    JNI_CHECK_GOTO((retval != EIJK_INVALID_STATE), env, "java/lang/IllegalStateException", NULL, label); \
    JNI_CHECK_GOTO((retval != EIJK_OUT_OF_MEMORY), env, "java/lang/OutOfMemoryError", NULL, label); \
    JNI_CHECK_GOTO((retval == 0), env, JNI_IJK_MEDIA_EXCEPTION, NULL, label);

static JavaVM* g_jvm;
static jobject* g_jdisplayFrame = NULL;

static player_fields_t g_clazz;

void ijkmp_async_release_android(ReleaseArgs* args);
static bool mediacodec_select_callback(void *opaque, ijkmp_mediacodecinfo_context *mcc);
static void video_decoder_selected_callback(void *opaque, const char *url, int rawdata);
static IjkMediaPlayer *jni_get_media_player(JNIEnv* env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);

    IjkMediaPlayer *mp = (IjkMediaPlayer *) (intptr_t) (*env)->GetLongField(env, thiz, g_clazz.mNativeMediaPlayer);
    if (mp) {
        ijkmp_inc_ref(mp);
    }

    pthread_mutex_unlock(&g_clazz.mutex);
    return mp;
}

static IjkMediaPlayer *jni_set_media_player(JNIEnv* env, jobject thiz, IjkMediaPlayer *mp)
{
    pthread_mutex_lock(&g_clazz.mutex);

    IjkMediaPlayer *old = (IjkMediaPlayer*) (intptr_t) (*env)->GetLongField(env, thiz, g_clazz.mNativeMediaPlayer);
    if (mp) {
        ijkmp_inc_ref(mp);
    }
    (*env)->SetLongField(env, thiz, g_clazz.mNativeMediaPlayer, (intptr_t) mp);

    pthread_mutex_unlock(&g_clazz.mutex);

    // NOTE: ijkmp_dec_ref may block thread
    if (old != NULL ) {
        ijkmp_dec_ref_p(&old);
    }

    return old;
}

static CCPlayerStat *jni_get_cc_player_stat(JNIEnv *env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);
    CCPlayerStat *cc_player_stat = (CCPlayerStat*) (intptr_t)(*env)->GetLongField(env, thiz, g_clazz.mNativeCCPlayerStat);
    pthread_mutex_unlock(&g_clazz.mutex);
    return cc_player_stat;
}

static void jni_set_cc_player_stat(JNIEnv *env, jobject thiz, CCPlayerStat *cc_player_stat)
{
    pthread_mutex_lock(&g_clazz.mutex);
    (*env)->SetLongField(env, thiz, g_clazz.mNativeCCPlayerStat, (intptr_t)cc_player_stat);
    pthread_mutex_unlock(&g_clazz.mutex);
}
/*
static PlayerConfig *jni_get_player_config(JNIEnv *env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);
    PlayerConfig *config = (PlayerConfig*) (intptr_t)(*env)->GetLongField(env, thiz, g_clazz.mNativePlayerConfig);
    pthread_mutex_unlock(&g_clazz.mutex);
    return config;
}

static void jni_set_player_config(JNIEnv *env, jobject thiz, PlayerConfig *config)
{
    pthread_mutex_lock(&g_clazz.mutex);
    (*env)->SetLongField(env, thiz, g_clazz.mNativePlayerConfig, (intptr_t)config);
    pthread_mutex_unlock(&g_clazz.mutex);
}
*/
static int message_loop(void *arg);

static void
IjkMediaPlayer_setDataSourceAndHeaders(
    JNIEnv *env, jobject thiz, jstring path,
    jobjectArray keys, jobjectArray values)
{
    MPTRACE("%s", __func__);
    int retval = 0;
    const char *c_path = NULL;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(path, env, "java/lang/IllegalArgumentException", "setDataSource: null path", LABEL_RETURN);
    JNI_CHECK_GOTO(mp, env, "java/lang/IllegalStateException", "setDataSource: null mp", LABEL_RETURN);

    c_path = (*env)->GetStringUTFChars(env, path, NULL );
    JNI_CHECK_GOTO(c_path, env, "java/lang/OutOfMemoryError", "setDataSource: path.string oom", LABEL_RETURN);

    retval = ijkmp_set_data_source(mp, c_path);
    (*env)->ReleaseStringUTFChars(env, path, c_path);

    IJK_CHECK_MPRET_GOTO(retval, env, LABEL_RETURN);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static int64_t jni_set_media_data_source(JNIEnv* env, jobject thiz, jobject media_data_source)
{
    int64_t nativeMediaDataSource = 0;

    pthread_mutex_lock(&g_clazz.mutex);

    jobject old = (jobject) (intptr_t) (*env)->GetLongField(env, thiz, g_clazz.field_mNativeMediaDataSource);;
    if (old) {
        //  J4AC_IMediaDataSource__close__catchAll(env, old);
        J4AC_tv_danmaku_ijk_media_player_misc_IMediaDataSource__close(env, old);
        J4A_ExceptionCheck__catchAll(env);
        J4A_DeleteGlobalRef__p(env, &old);
        (*env)->SetLongField(env, thiz, g_clazz.field_mNativeMediaDataSource, 0);
        J4A_ExceptionCheck__catchAll(env);
        ALOGI("[mds] release old mds");
    }

    if (media_data_source) {
        jobject global_media_data_source = (*env)->NewGlobalRef(env, media_data_source);
        if (J4A_ExceptionCheck__catchAll(env) || !global_media_data_source)
            goto fail;
        nativeMediaDataSource = (int64_t) (intptr_t) global_media_data_source;
        (*env)->SetLongField(env, thiz, g_clazz.field_mNativeMediaDataSource, nativeMediaDataSource);
        J4A_ExceptionCheck__catchAll(env);
        ALOGI("[mds] new mds");
    }

    fail:
    pthread_mutex_unlock(&g_clazz.mutex);
    return nativeMediaDataSource;
}

static void IjkMediaPlayer_setDataSourceCallback(JNIEnv *env, jobject thiz, jobject callback)
{
            MPTRACE("%s\n", __func__);
    int retval = 0;
    char uri[128];
    int64_t nativeMediaDataSource = 0;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(callback, env, "java/lang/IllegalArgumentException", "mpjni: setDataSourceCallback: null fd", LABEL_RETURN);
    JNI_CHECK_GOTO(mp, env, "java/lang/IllegalStateException", "mpjni: setDataSourceCallback: null mp", LABEL_RETURN);

    nativeMediaDataSource = jni_set_media_data_source(env, thiz, callback);
    JNI_CHECK_GOTO(nativeMediaDataSource, env, "java/lang/IllegalStateException", "mpjni: jni_set_media_data_source: NewGlobalRef", LABEL_RETURN);

    ALOGI("setDataSourceCallback: %"PRId64"\n", nativeMediaDataSource);
    snprintf(uri, sizeof(uri), "ijkmediadatasource:%"PRId64, nativeMediaDataSource);

    retval = ijkmp_set_data_source(mp, uri);

    IJK_CHECK_MPRET_GOTO(retval, env, LABEL_RETURN);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}


static void IjkMediaPlayer_setDomain(JNIEnv *env, jobject thiz)
{
}


static void
IjkMediaPlayer_setVideoSurface(JNIEnv *env, jobject thiz, jobject jsurface)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setVideoSurface: null mp", LABEL_RETURN);

    ijkmp_android_set_surface(env, mp, jsurface);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
    return;
}

static void
IjkMediaPlayer_prepareAsync(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    int retval = 0;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "prepareAsync: null mp", LABEL_RETURN);

    retval = ijkmp_prepare_async(mp);
    IJK_CHECK_MPRET_GOTO(retval, env, LABEL_RETURN);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_start(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "start: null mp", LABEL_RETURN);

    ijkmp_start(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_stop(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "stop: null mp", LABEL_RETURN);

    ijkmp_stop(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_resumedisplay(JNIEnv *env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "resumedisplay: null mp", LABEL_RETURN);

    ijkmp_resumedisplay(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_pausedisplay(JNIEnv *env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "pausedisplay: null mp", LABEL_RETURN);

    ijkmp_pausedisplay(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_pause(JNIEnv *env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "pause: null mp", LABEL_RETURN);

    ijkmp_pause(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_seekTo(JNIEnv *env, jobject thiz, int msec)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "seekTo: null mp", LABEL_RETURN);

    ijkmp_seek_to(mp, msec);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static jboolean
IjkMediaPlayer_isPlaying(JNIEnv *env, jobject thiz)
{
    jboolean retval = JNI_FALSE;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "isPlaying: null mp", LABEL_RETURN);

    retval = ijkmp_is_playing(mp) ? JNI_TRUE : JNI_FALSE;

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
    return retval;
}

static jlong
IjkMediaPlayer_getCurrentPosition(JNIEnv *env, jobject thiz)
{
    int retval = 0;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "getCurrentPosition: null mp", LABEL_RETURN);

    retval = ijkmp_get_current_position(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
    return retval;
}

static int IjkMediaPlayer_getTextureId(JNIEnv* env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if(!mp) return 0;
	if(!mp->ffplayer) return 0;
    SDL_Vout *vout = mp->ffplayer->vout;
    if(vout)
    {
       return vout->nYTexture;
    }
    return 0;
}

static void IjkMediaPlayer_setTextureId(JNIEnv* env, jobject thiz, jint textureId)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if(!mp) return ;
	if(!mp->ffplayer) return;
    SDL_Vout * vout = mp->ffplayer->vout;
    if(vout)
    {
        vout->nYTexture = textureId;
    }
}

static void  IjkMediaPlayer_setUseLibYuv(JNIEnv* env,jobject thiz,jint useLibYuv)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if(!mp) return ;
    if(!mp->ffplayer) return;
    SDL_Vout * vout = mp->ffplayer->vout;
    if(vout)
        vout->useLibYuv = useLibYuv;
}


static jlong
IjkMediaPlayer_getDuration(JNIEnv *env, jobject thiz)
{
    long retval = 0;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "getDuration: null mp", LABEL_RETURN);

    retval = ijkmp_get_duration(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
    return retval;
}

static jlong
IjkMediaPlayer_getPlayableDuration(JNIEnv *env, jobject thiz)
{
    long retval = 0;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "getPlayableDuration: null mp", LABEL_RETURN);

    retval = ijkmp_get_playable_duration(mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
    return retval;
}

static void
IjkMediaPlayer_release(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (!mp)
        return;

    CCPlayerStat *cc_player_stat = jni_get_cc_player_stat(env, thiz);
    pthread_mutex_lock(&g_clazz.mutex);
    (*env)->SetLongField(env, thiz, g_clazz.mNativeCCPlayerStat, (intptr_t)NULL);
    pthread_mutex_unlock(&g_clazz.mutex);

    ijkmp_android_set_surface(env, mp, NULL );

    ReleaseArgs* args = (ReleaseArgs*)malloc(sizeof(ReleaseArgs));
    args->mp = mp;
    args->cc_player_stat = cc_player_stat;
    args->playerThiz = (*env)->NewGlobalRef(env, thiz);

    ijkmp_async_release_android(args);
    SDL_DetachThread(mp->release_thread);

    MPTRACE("%s end", __func__);
}

static void IjkMediaPlayer_native_setup(JNIEnv *env, jobject thiz, jobject weak_this, jboolean crop,int nPanorama);
static void
IjkMediaPlayer_reset(JNIEnv *env, jobject thiz, int nPanorama)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (!mp)
        return;
    int crop = mp->ffplayer->crop;
    jobject weak_thiz = (jobject) ijkmp_set_weak_thiz(mp, NULL );

    IjkMediaPlayer_release(env, thiz);
    IjkMediaPlayer_native_setup(env, thiz, weak_thiz, crop, nPanorama);

    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setVolume(JNIEnv *env, jobject thiz, jfloat leftVolume, jfloat rightVolume)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setVolume: null mp", LABEL_RETURN);

    ijkmp_android_set_volume(env, mp, leftVolume, rightVolume);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setAvFormatOption(JNIEnv *env, jobject thiz, jobject name, jobject value)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    const char *c_name = NULL;
    const char *c_value = NULL;
    JNI_CHECK_GOTO(mp, env, NULL, "setAvFormatOption: null mp", LABEL_RETURN);

    c_name = (*env)->GetStringUTFChars(env, name, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "mpjni: setAvFormatOption: name.string oom", LABEL_RETURN);

    c_value = (*env)->GetStringUTFChars(env, value, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "mpjni: setAvFormatOption: name.string oom", LABEL_RETURN);

    ijkmp_set_format_option(mp, c_name, c_value);

    LABEL_RETURN:
    if (c_name)
        (*env)->ReleaseStringUTFChars(env, name, c_name);
    if (c_value)
        (*env)->ReleaseStringUTFChars(env, value, c_value);
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setAvCodecOption(JNIEnv *env, jobject thiz, jobject name, jobject value)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    const char *c_name = NULL;
    const char *c_value = NULL;
    JNI_CHECK_GOTO(mp, env, NULL, "setAvCodecOption: null mp", LABEL_RETURN);

    c_name = (*env)->GetStringUTFChars(env, name, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "setAvCodecOption: name.string oom", LABEL_RETURN);

    c_value = (*env)->GetStringUTFChars(env, value, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "setAvCodecOption: name.string oom", LABEL_RETURN);

    ijkmp_set_codec_option(mp, c_name, c_value);

    LABEL_RETURN:
    if (c_name)
        (*env)->ReleaseStringUTFChars(env, name, c_name);
    if (c_value)
        (*env)->ReleaseStringUTFChars(env, value, c_value);
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setSwScaleOption(JNIEnv *env, jobject thiz, jobject name, jobject value)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    const char *c_name = NULL;
    const char *c_value = NULL;
    JNI_CHECK_GOTO(mp, env, NULL, "setSwScaleOption: null mp", LABEL_RETURN);

    c_name = (*env)->GetStringUTFChars(env, name, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "mpjni: setSwScaleOption: name.string oom", LABEL_RETURN);

    c_value = (*env)->GetStringUTFChars(env, value, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "mpjni: setSwScaleOption: name.string oom", LABEL_RETURN);

    ijkmp_set_sws_option(mp, c_name, c_value);

    LABEL_RETURN:
    if (c_name)
        (*env)->ReleaseStringUTFChars(env, name, c_name);
    if (c_value)
        (*env)->ReleaseStringUTFChars(env, value, c_value);
    ijkmp_dec_ref_p(&mp);
}

static void IjkMediaPlayer_setOptionLong(JNIEnv *env, jobject thiz, jint category, jobject name, jlong value)
{
    MPTRACE("%s\n", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    const char *c_name = NULL;
    JNI_CHECK_GOTO(mp, env, "java/lang/IllegalStateException", "mpjni: setOptionLong: null mp", LABEL_RETURN);

    c_name = (*env)->GetStringUTFChars(env, name, NULL );
    JNI_CHECK_GOTO(c_name, env, "java/lang/OutOfMemoryError", "mpjni: setOptionLong: name.string oom", LABEL_RETURN);

    ijkmp_set_option_int(mp, category, c_name, value);

    LABEL_RETURN:
    if (c_name)
        (*env)->ReleaseStringUTFChars(env, name, c_name);
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setOverlayFormat(JNIEnv *env, jobject thiz, jint chromaFourCC)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setAvCodecOption: null mp", LABEL_RETURN);

    ijkmp_set_overlay_format(mp, chromaFourCC);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setFrameDrop(JNIEnv *env, jobject thiz, jint frameDrop)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setFrameDrop: null mp", LABEL_RETURN);

    ijkmp_set_framedrop(mp, frameDrop);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setMuted(JNIEnv *env, jobject thiz, jint muted)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setMuted: null mp", LABEL_RETURN);
    ijkmp_set_muted(mp,muted);
    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setStartSeekPos(JNIEnv *env, jobject thiz, jint seekPos)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setStartPos: null mp", LABEL_RETURN);
    if (seekPos > 0)
        ijmp_set_start_seek_pos(mp, seekPos);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setMediaCodecEnabled(JNIEnv *env, jobject thiz, jboolean enabled)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setMediaCodecEnabled: null mp", LABEL_RETURN);

    ijkmp_android_set_mediacodec_enabled(mp, enabled);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_setOpenSLESEnabled(JNIEnv *env, jobject thiz, jboolean enabled)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setOpenSLESEnabled: null mp", LABEL_RETURN);

    ijkmp_android_set_opensles_enabled(mp, enabled);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static jstring
IjkMediaPlayer_getColorFormatName(JNIEnv *env, jclass clazz, jint mediaCodecColorFormat)
{
    const char *codec_name = SDL_AMediaCodec_getColorFormatName(mediaCodecColorFormat);
    if (!codec_name)
        return NULL ;

    return (*env)->NewStringUTF(env, codec_name);
}

static jstring
IjkMediaPlayer_getVideoCodecInfo(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    jstring jcodec_info = NULL;
    int ret = 0;
    char *codec_info = NULL;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "getVideoCodecInfo: null mp", LABEL_RETURN);

    ret = ijkmp_get_video_codec_info(mp, &codec_info);
    if (ret < 0 || !codec_info)
        goto LABEL_RETURN;

    jcodec_info = (*env)->NewStringUTF(env, codec_info);
    LABEL_RETURN:
    if (codec_info)
        free(codec_info);

    ijkmp_dec_ref_p(&mp);
    return jcodec_info;
}

static jstring
IjkMediaPlayer_getAudioCodecInfo(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    jstring jcodec_info = NULL;
    int ret = 0;
    char *codec_info = NULL;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "getAudioCodecInfo: null mp", LABEL_RETURN);

    ret = ijkmp_get_audio_codec_info(mp, &codec_info);
    if (ret < 0 || !codec_info)
        goto LABEL_RETURN;

    jcodec_info = (*env)->NewStringUTF(env, codec_info);
    LABEL_RETURN:
    if (codec_info)
        free(codec_info);

    ijkmp_dec_ref_p(&mp);
    return jcodec_info;
}

inline static void fillMetaInternal(JNIEnv *env, jobject jbundle, IjkMediaMeta *meta, const char *key, const char *default_value)
{
    const char *value = ijkmeta_get_string_l(meta, key);
    if (value == NULL )
        value = default_value;

    ASDK_Bundle__putString_c(env, jbundle, key, value);
    SDL_JNI_CatchException(env);
}

static jobject
IjkMediaPlayer_getMediaMeta(JNIEnv *env, jobject thiz)
{
    MPTRACE("%s", __func__);
    bool is_locked = false;
    jobject jret_bundle = NULL;
    jobject jlocal_bundle = NULL;
    jobject jstream_bundle = NULL;
    jobject jarray_list = NULL;
    IjkMediaMeta *meta = NULL;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "getMediaMeta: null mp", LABEL_RETURN);

    meta = ijkmp_get_meta_l(mp);
    if (!meta)
        goto LABEL_RETURN;

    ijkmeta_lock(meta);
    is_locked = true;

    jlocal_bundle = ASDK_Bundle__init(env);
    if (SDL_JNI_RethrowException(env)) {
        goto LABEL_RETURN;
    }

    fillMetaInternal(env, jlocal_bundle, meta, IJKM_KEY_FORMAT, NULL );
    fillMetaInternal(env, jlocal_bundle, meta, IJKM_KEY_DURATION_US, NULL );
    fillMetaInternal(env, jlocal_bundle, meta, IJKM_KEY_START_US, NULL );
    fillMetaInternal(env, jlocal_bundle, meta, IJKM_KEY_START_US, NULL );

    fillMetaInternal(env, jlocal_bundle, meta, IJKM_KEY_VIDEO_STREAM, "-1");
    fillMetaInternal(env, jlocal_bundle, meta, IJKM_KEY_AUDIO_STREAM, "-1");

    jarray_list = ASDK_ArrayList__init(env);
    if (SDL_JNI_RethrowException(env)) {
        goto LABEL_RETURN;
    }

    size_t count = ijkmeta_get_children_count_l(meta);
    for (size_t i = 0; i < count; ++i) {
        IjkMediaMeta *streamRawMeta = ijkmeta_get_child_l(meta, i);
        if (streamRawMeta) {
            jstream_bundle = ASDK_Bundle__init(env);
            if (SDL_JNI_RethrowException(env)) {
                goto LABEL_RETURN;
            }

            fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_TYPE, IJKM_VAL_TYPE__UNKNOWN);
            const char *type = ijkmeta_get_string_l(streamRawMeta, IJKM_KEY_TYPE);
            if (type) {
                fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_CODEC_NAME, NULL );
                fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_CODEC_PROFILE, NULL );
                fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_CODEC_LONG_NAME, NULL );
                fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_BITRATE, NULL );

                if (0 == strcmp(type, IJKM_VAL_TYPE__VIDEO)) {
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_WIDTH, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_HEIGHT, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_FPS_NUM, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_FPS_DEN, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_TBR_NUM, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_TBR_DEN, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_SAR_NUM, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_SAR_DEN, NULL );
                } else if (0 == strcmp(type, IJKM_VAL_TYPE__AUDIO)) {
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_SAMPLE_RATE, NULL );
                    fillMetaInternal(env, jstream_bundle, streamRawMeta, IJKM_KEY_CHANNEL_LAYOUT, NULL );
                }
                ASDK_ArrayList__add(env, jarray_list, jstream_bundle);
                if (SDL_JNI_RethrowException(env)) {
                    goto LABEL_RETURN;
                }
            }

            SDL_JNI_DeleteLocalRefP(env, &jstream_bundle);
        }
    }

    ASDK_Bundle__putParcelableArrayList_c(env, jlocal_bundle, IJKM_KEY_STREAMS, jarray_list);
    jret_bundle = jlocal_bundle;
    jlocal_bundle = NULL;
    LABEL_RETURN:
    if (is_locked && meta)
        ijkmeta_unlock(meta);

    SDL_JNI_DeleteLocalRefP(env, &jstream_bundle);
    SDL_JNI_DeleteLocalRefP(env, &jlocal_bundle);
    SDL_JNI_DeleteLocalRefP(env, &jarray_list);

    ijkmp_dec_ref_p(&mp);
    return jret_bundle;
}

static void
IjkMediaPlayer_getStatInfo(JNIEnv *env, jobject thiz, jobject info)
{
}

static void copyjstring(JNIEnv *env, char *dst, jstring s, int length)
{
    if (s) {
        const char* content = (*env)->GetStringUTFChars(env, s, NULL);
        strncpy(dst, content, length);
        (*env)->ReleaseStringUTFChars(env, s, content);
    }
}

static void
IjkMediaPlayer_setPlayerConfig(JNIEnv *env, jobject thiz, jobject playerConfig)
{
    ALOGI("%s begin", __func__);
    ALOGI("%s end", __func__);
}

static void
IjkMediaPlayer_setPlayControlParameters(JNIEnv *env, jobject thiz, jboolean canfwd, jboolean fwdnew, int buffertime, int fwdexttime, int minjitter, int maxjitter)
{
//    MPTRACE("%s canfwd=%d, fwdnew=%d, buffertime=%d, fwdexttime=%d, minjitter=%d, maxjitter=%d", __func__, canfwd, fwdnew, buffertime, fwdexttime, minjitter, maxjitter);
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_set_play_control_parameters(mp, canfwd, fwdnew, buffertime, fwdexttime, minjitter, maxjitter);
        ijkmp_dec_ref_p(&mp);
    }
}

static void IjkMediaPlayer_setRealTimeFlag(JNIEnv *env, jobject thiz, jboolean realtime)
{
    IjkMediaPlayer * mp  = jni_get_media_player(env, thiz);
    if(mp)
    {
        ijkmp_set_realtime_flag(mp,realtime);
        ijkmp_dec_ref_p(&mp);
    }
}

static void IjkMediaPlayer_setCropMode(JNIEnv *env, jobject thiz, jboolean crop, jint surface_width, jint surface_height)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_set_crop_mode(mp, crop, surface_width, surface_height);
        ijkmp_dec_ref_p(&mp);
    }
}


static void IjkMediaPlayer_setLanguage(JNIEnv *env, jobject thiz, jstring audioLanguage, jstring subtitleLanguage)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    const char *c_audioLanguage = (*env)->GetStringUTFChars(env, audioLanguage, NULL);
    const char *c_subtitleLanguage = (*env)->GetStringUTFChars(env, subtitleLanguage, NULL);
    if (mp) {
        ijkmp_set_language(mp, c_audioLanguage, c_subtitleLanguage);
        ijkmp_dec_ref_p(&mp);
    }
    (*env)->ReleaseStringUTFChars(env, audioLanguage, c_audioLanguage);
    (*env)->ReleaseStringUTFChars(env, subtitleLanguage, c_subtitleLanguage);
}


static void IjkMediaPlayer_displaySubtitle(JNIEnv *env, jobject thiz, jboolean enable)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_set_sub_display(mp, enable);
        ijkmp_dec_ref_p(&mp);
    }
}

void IjkMediaPlayer_setSaveToLocal(JNIEnv *env, jobject thiz, jstring path)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    if (mp) {
        ijkmp_set_save_to_local(mp, c_path);
        ijkmp_dec_ref_p(&mp);
    }
    (*env)->ReleaseStringUTFChars(env, path, c_path);
}

//first buffering completed, send startplay statistics
static void IjkMediaPlayer_onCCPlayerFirstBufferingComplete(JNIEnv *env, jobject thiz)
{
}

static void IjkMediaPlayer_onGLSurfaceCreated(JNIEnv *env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_android_on_glsurface_created(mp);
        ijkmp_dec_ref_p(&mp);
    }
}

static void IjkMediaPlayer_onGLSurfaceChanged(JNIEnv *env, jobject thiz, int width, int height)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_android_on_glsurface_changed(mp, width, height);
        ijkmp_dec_ref_p(&mp);
    }
}

static void IjkMediaPlayer_redraw(JNIEnv *env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_android_redraw(mp);
        ijkmp_dec_ref_p(&mp);
    }
}

static void IjkMediaPlayer_redrawTexture(JNIEnv *env, jobject thiz, int width, int height)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_android_redraw_texture(mp, width, height);
        ijkmp_dec_ref_p(&mp);
    }
}

bool request_redraw(void *arg)
{
    IjkMediaPlayer *mp = (IjkMediaPlayer*)arg;
    if (!mp || !mp->weak_thiz) {
        return false;
    }
    JavaVM *vm = g_jvm;
    JNIEnv *env = NULL;
//    int ret = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_4);
//    if (ret < 0) {
//        (*vm)->AttachCurrentThread(vm, &env, NULL);
//    }
    if(JNI_OK!=SDL_JNI_SetupThreadEnv(&env)){
        ALOGE("request_redraw setup up thread fail ");
        return false;
    }

    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.jmid_requestRedraw, mp->weak_thiz);
    //(*vm)->DetachCurrentThread(vm);
//    if (ret < 0) {
//        (*vm)->DetachCurrentThread(vm);
//    }
    if(SDL_JNI_CatchException(env)){
        ALOGE("request_redraw setup up thread fail 2");
    }
    return true;
}

bool OnRawImageAvailable(void *arg, unsigned char* data[3], int stride[3], int width, int height, int format, int rotate)
{
    IjkMediaPlayer *mp = (IjkMediaPlayer*)arg;
    if (!mp || !mp->weak_thiz) {
        return false;
    }

    JNIEnv *env = NULL;
    if(JNI_OK!=SDL_JNI_SetupThreadEnv(&env)){
        ALOGE("request_redraw setup up thread fail ");
        return false;
    }

    int uScale = 1, vScale = 1;
    switch (format) {
        case SDL_FCC__AMC:
        case SDL_FCC_I420:
            uScale = vScale = 2;
            break;
        default:
            break;
    }

    jobject y = (*env)->NewDirectByteBuffer(env, data[0], stride[0] * height);
    jobject u = (*env)->NewDirectByteBuffer(env, data[1], stride[1] * height / uScale);
    jobject v = (*env)->NewDirectByteBuffer(env, data[2], stride[2] * height / vScale);

    //ALOGE("[RawVideo] sY(%d), sU(%d) sV(%d) width(%d) height(%d)", stride[0], stride[1], stride[2], width, height);

    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.jmid_onRawImageAvailable, mp->weak_thiz, y, u, v, stride[0], stride[1], stride[2], width, height, format, rotate);

    if (y != NULL) {
        (*env)->DeleteLocalRef(env, y);
    }

    if (u != NULL) {
        (*env)->DeleteLocalRef(env, u);
    }

    if (v != NULL) {
        (*env)->DeleteLocalRef(env, v);
    }

    if(SDL_JNI_CatchException(env)){
        ALOGE("request_redraw setup up thread fail 2");
    }
    return true;
}


static void on_display_frame(DisplayFrame *frame, void *obj) {

}





static void IjkMediaPlayer_captureFrame(JNIEnv *env, jobject thiz)
{
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_capture_frame(mp);
        ijkmp_dec_ref_p(&mp);
    }
}

static void IjkMediaPlayer_setDevMode(JNIEnv *env, jobject thiz, jboolean dev)
{
}

static void IjkMediaPlayer_setRadicalRealTimeFlag(JNIEnv *env, jobject thiz, jboolean radical) {
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    JNI_CHECK_GOTO(mp, env, NULL, "setRadicalRealTimeFlag: null mp", LABEL_RETURN);

    ijkmp_set_radical_real_time(mp, radical);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void IjkMediaPlayer_setLoopNumber(JNIEnv *env, jobject thiz,jint loop_number)
{
    IjkMediaPlayer *mp = jni_get_media_player(env,thiz);
    if(mp)
    {
        ijkmp_set_loop_number(mp,loop_number);
    }
}

static void IjkMediaPlayer_setVideoDisable(JNIEnv* env,jobject thiz,jint video_disable)
{
    IjkMediaPlayer *mp = jni_get_media_player(env,thiz);
    if(mp)
        ijkmp_set_video_disable(mp,video_disable);
}

static void
IjkMediaPlayer_native_init(JNIEnv *env, jclass clazz, jboolean logEnable)
{
    sLogEnable = logEnable;
    MPTRACE("%s, LOG ENABLE=%d", __func__, logEnable);
}

static void
IjkMediaPlayer_native_setup(JNIEnv *env, jobject thiz, jobject weak_this, jboolean crop, int nPanorama)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer *mp = ijkmp_android_create(message_loop, crop,nPanorama, request_redraw, OnRawImageAvailable);
    JNI_CHECK_GOTO(mp, env, "java/lang/OutOfMemoryError", "native_setup: ijkmp_create() failed", LABEL_RETURN);

    jni_set_media_player(env, thiz, mp);
    ijkmp_set_weak_thiz(mp, (*env)->NewGlobalRef(env, weak_this));
    ijkmp_set_format_callback(mp, (*env)->NewGlobalRef(env, weak_this));
    ijkmp_android_set_mediacodec_select_callback(mp, mediacodec_select_callback, (*env)->NewGlobalRef(env, weak_this));
    ijkmp_android_set_video_select_callback(mp,video_decoder_selected_callback,(*env)->NewGlobalRef(env, weak_this));

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
}

static void
IjkMediaPlayer_native_finalize(JNIEnv *env, jobject thiz, jobject name, jobject value)
{
    MPTRACE("%s", __func__);
    IjkMediaPlayer_release(env, thiz);
}

static void IjkMediaPlayer_setProperty(JNIEnv* env, jobject thiz, jint id, jfloat val) {
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ijkmp_set_property_float(mp, id, val);
        ijkmp_dec_ref_p(&mp);
    }
}

static jfloat IjkMediaPlayer_getProperty(JNIEnv* env, jobject thiz, int id, jfloat val) {
    float ret = val;
    IjkMediaPlayer *mp = jni_get_media_player(env, thiz);
    if (mp) {
        ret = ijkmp_get_property_float(mp, id, val);
        ijkmp_dec_ref_p(&mp);
    }
    return ret;
}


// player load video error
static void IjkMediaPlayer_onPlayerLoadError(JNIEnv *env, jobject weak_thiz, int error)
{
}

static void IjkMediaPlayer_setDecodeRawData(JNIEnv *env, jobject weak_thiz, int enable)
{
    jint sdk_int = SDL_Android_GetApiLevel();
    if(sdk_int <= IJK_API_20_KITKAT_WATCH) {
        MPTRACE("Not support raw data decode mode for android version <= API 20 ");
        return;
    }
    MPTRACE("Only support raw data decode mode for android version > API 20 ", sdk_int);
    IjkMediaPlayer *mp = jni_get_media_player(env, weak_thiz);
    if (mp) {
        ijkmp_andriod_set_decode_raw_data_enable(mp, enable);
    }
}

static bool mediacodec_select_callback(void *opaque, ijkmp_mediacodecinfo_context *mcc)
{
    JNIEnv *env = NULL;
    jobject jmime = NULL;
    jstring jcodec_name = NULL;
    jobject weak_this = (jobject) opaque;
    const char *codec_name = NULL;
    bool found_codec = false;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("%s: SetupThreadEnv failed\n", __func__);
        return -1;
    }

    jmime = (*env)->NewStringUTF(env, mcc->mime_type);
    if (SDL_JNI_CatchException(env) || !jmime) {
        goto fail;
    }

    jcodec_name = (*env)->CallStaticObjectMethod(env, g_clazz.clazz, g_clazz.jmid_onSelectCodec, weak_this, jmime, mcc->profile, mcc->level,mcc->mediaformat);
    if (SDL_JNI_CatchException(env) || !jcodec_name) {
        goto fail;
    }

    codec_name = (*env)->GetStringUTFChars(env, jcodec_name, NULL );
    if (!codec_name || !*codec_name) {
        goto fail;
    }

    strncpy(mcc->codec_name, codec_name, sizeof(mcc->codec_name) / sizeof(*mcc->codec_name));
    mcc->codec_name[sizeof(mcc->codec_name) / sizeof(*mcc->codec_name) - 1] = 0;
    found_codec = true;
    fail:
    if (codec_name) {
        (*env)->ReleaseStringUTFChars(env, jcodec_name, codec_name);
        codec_name = NULL;
    }

    SDL_JNI_DeleteLocalRefP(env, &jcodec_name);
    SDL_JNI_DeleteLocalRefP(env, &jmime);
    return found_codec;
}

inline static void post_event(JNIEnv *env, jobject weak_this, int what, int arg1, int arg2, jobject obj)
{
    // MPTRACE("post_event(%p, %p, %d, %d, %d)", (void*)env, (void*) weak_this, what, arg1, arg2);
    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.jmid_postEventFromNative, weak_this, what, arg1, arg2, obj);
    // MPTRACE("post_event()=void");
}

static void message_loop_n(JNIEnv *env, IjkMediaPlayer *mp)
{
    while (1) {
        AVMessage msg;

        int retval = ijkmp_get_msg(mp, &msg, 1);
        if (retval < 0)
            break;

        jobject weak_thiz = (jobject) ijkmp_get_weak_thiz(mp);//get every time
        JNI_CHECK_GOTO(weak_thiz, env, NULL, "message_loop_n: null weak_thiz", LABEL_RETURN);
        // block-get should never return 0
        assert(retval > 0);

        switch (msg.what) {
        case FFP_MSG_FLUSH:
            MPTRACE("FFP_MSG_FLUSH:");
            post_event(env, weak_thiz, MEDIA_NOP, 0, 0, NULL);
            break;
        case FFP_MSG_ERROR:
            MPTRACE("FFP_MSG_ERROR: %d", msg.arg1);
            post_event(env, weak_thiz, MEDIA_ERROR, msg.arg1, 0, NULL);
            break;
        case FFP_MSG_PREPARED:
            MPTRACE("FFP_MSG_PREPARED:");

            post_event(env, weak_thiz, MEDIA_PREPARED, 0, 0, NULL);
            break;
        case FFP_MSG_COMPLETED:
            MPTRACE("FFP_MSG_COMPLETED:");
            post_event(env, weak_thiz, MEDIA_PLAYBACK_COMPLETE, 0, 0, NULL);
            break;
        case FFP_MSG_VIDEO_SIZE_CHANGED:
            MPTRACE("FFP_MSG_VIDEO_SIZE_CHANGED: %d, %d", msg.arg1, msg.arg2);
            post_event(env, weak_thiz, MEDIA_SET_VIDEO_SIZE, msg.arg1, msg.arg2, NULL);
            break;
        case FFP_MSG_SAR_CHANGED:
            MPTRACE("FFP_MSG_SAR_CHANGED: %d, %d", msg.arg1, msg.arg2);
            post_event(env, weak_thiz, MEDIA_SET_VIDEO_SAR, msg.arg1, msg.arg2, NULL);
            break;
        case FFP_MSG_BUFFERING_START:
            MPTRACE("FFP_MSG_BUFFERING_START:");
            post_event(env, weak_thiz, MEDIA_INFO, MEDIA_INFO_BUFFERING_START, 0, NULL);
            break;
        case FFP_MSG_BUFFERING_END:
            MPTRACE("FFP_MSG_BUFFERING_END:");
            post_event(env, weak_thiz, MEDIA_INFO, MEDIA_INFO_BUFFERING_END, 0, NULL);
            break;
        case FFP_MSG_BUFFERING_UPDATE:
            // MPTRACE("FFP_MSG_BUFFERING_UPDATE: %d, %d", msg.arg1, msg.arg2);
            post_event(env, weak_thiz, MEDIA_BUFFERING_UPDATE, msg.arg1, msg.arg2, NULL);
            break;
        case FFP_MSG_BUFFERING_BYTES_UPDATE:
            break;
        case FFP_MSG_BUFFERING_TIME_UPDATE:
            break;
        case FFP_MSG_SEEK_COMPLETE:
            MPTRACE("FFP_MSG_SEEK_COMPLETE:");
            post_event(env, weak_thiz, MEDIA_SEEK_COMPLETE, 0, 0, NULL);
            break;
        case FFP_MSG_ACCURATE_SEEK_COMPLETE:
            MPTRACE("FFP_MSG_ACCURATE_SEEK_COMPLETE:\n");
            post_event(env, weak_thiz, MEDIA_INFO, MEDIA_INFO_MEDIA_ACCURATE_SEEK_COMPLETE, msg.arg1, NULL);
            break;
        case FFP_MSG_PLAYBACK_STATE_CHANGED:
            break;
        case FFP_MSG_RESTORE_VIDEO_PLAY:
            post_event(env, weak_thiz, MEDIA_INFO, MEDIA_INFO_RESTORE_VIDEO_PLAY, 0, NULL);
            break;
        case FFP_MSG_CAPTURE_COMPLETED:
        {
            int width = msg.arg1;
            int height = msg.arg2;
            int pixelCount = width * height;
            jintArray argbIntData = NULL;
            if (pixelCount > 0) {
                argbIntData = (*env)->NewIntArray(env, pixelCount);
                const jint *argbData = (jint*) ijkmp_get_capture_frame_data(mp);
                (*env)->SetIntArrayRegion(env, argbIntData, 0, pixelCount, argbData);
            }
            post_event(env, weak_thiz, MEDIA_CAPTURE_COMPLETE, width, height, argbIntData);
            if (argbIntData) {
                (*env)->DeleteLocalRef(env, argbIntData);
            }
            break;
        }
        case FFP_MSG_FIRST_BUFFERING_READY:
        {
            post_event(env, weak_thiz, MEDIA_FIRST_BUFFERING_COMPLETE, 0, 0, NULL);
            break;
        }
        case FFP_MSG_BUFFERING_COUNT_WARNING:     // buffer次数超限
        case FFP_MSG_BUFFERING_TAKE_WARNING:      // 单次buffer时间太长
        {
            post_event(env, weak_thiz, MEDIA_INFO, MEDIA_INFO_BUFFERING_COUNT_WARN, 0, NULL);
            MPTRACE("FFP_MSG_BUFFERING_COUNT_WARNING: %d", msg.what);
            break;
        }
        case FFP_MSG_TIMED_TEXT:
        {
            if (msg.obj) {
                jstring text = (*env)->NewStringUTF(env, (char *)msg.obj);
                post_event(env, weak_thiz, MEDIA_TIMED_TEXT, 0, 0, text);
                J4A_DeleteLocalRef__p(env, &text);
            } else {
                post_event(env, weak_thiz, MEDIA_TIMED_TEXT, 0, 0, NULL);
            }
            break;
        }
        default:
            ALOGE("unknown FFP_MSG_xxx(%d)", msg.what);
            break;
        }
        msg_free_res(&msg);
    }

    LABEL_RETURN:
    ;
}

static int message_loop(void *arg)
{
    MPTRACE("%s", __func__);

    JNIEnv *env = NULL;
    (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL );

    IjkMediaPlayer *mp = (IjkMediaPlayer*) arg;
    JNI_CHECK_GOTO(mp, env, NULL, "native_message_loop: null mp", LABEL_RETURN);

    message_loop_n(env, mp);

    LABEL_RETURN:
    ijkmp_dec_ref_p(&mp);
    (*g_jvm)->DetachCurrentThread(g_jvm);

    MPTRACE("message_loop exit");
    return 0;
}

// ----------------------------------------------------------------------------

static JNINativeMethod g_methods[] = {
    {
        "_setDataSource",
        "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",
        (void *) IjkMediaPlayer_setDataSourceAndHeaders
    },
    { "_setDataSource", "(Ltv/danmaku/ijk/media/player/misc/IMediaDataSource;)V", (void *)IjkMediaPlayer_setDataSourceCallback },
    { "_setVideoSurface", "(Landroid/view/Surface;)V", (void *) IjkMediaPlayer_setVideoSurface },
    { "_prepareAsync", "()V", (void *) IjkMediaPlayer_prepareAsync },
    { "_start", "()V", (void *) IjkMediaPlayer_start },
    { "_stop", "()V", (void *) IjkMediaPlayer_stop },
    { "seekTo", "(J)V", (void *) IjkMediaPlayer_seekTo },
    { "_pause", "()V", (void *) IjkMediaPlayer_pause },
    { "_resumedisplay", "()V", (void *) IjkMediaPlayer_resumedisplay},
    { "_pausedisplay", "()V", (void *) IjkMediaPlayer_pausedisplay},
    { "isPlaying", "()Z", (void *) IjkMediaPlayer_isPlaying },
    { "getCurrentPosition", "()J", (void *) IjkMediaPlayer_getCurrentPosition },
    { "getDuration", "()J", (void *) IjkMediaPlayer_getDuration },
    { "_getTextureId", "()I",(void*)IjkMediaPlayer_getTextureId},
    { "getPlayableDuration", "()J", (void *) IjkMediaPlayer_getPlayableDuration },
    { "_release", "()V", (void *) IjkMediaPlayer_release },
    { "_reset", "(I)V", (void *) IjkMediaPlayer_reset },
    { "setVolume", "(FF)V", (void *) IjkMediaPlayer_setVolume },
    { "native_init", "(Z)V", (void *) IjkMediaPlayer_native_init },
    { "native_setup", "(Ljava/lang/Object;ZI)V", (void *) IjkMediaPlayer_native_setup },
    { "native_finalize", "()V", (void *) IjkMediaPlayer_native_finalize },

    { "_setAvFormatOption", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) IjkMediaPlayer_setAvFormatOption },
    { "_setAvCodecOption", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) IjkMediaPlayer_setAvCodecOption },
    { "_setSwScaleOption", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) IjkMediaPlayer_setSwScaleOption },
    { "_setOption",             "(ILjava/lang/String;J)V",                  (void *) IjkMediaPlayer_setOptionLong },
    { "_setOverlayFormat", "(I)V", (void *) IjkMediaPlayer_setOverlayFormat },
    { "_setFrameDrop", "(I)V", (void *) IjkMediaPlayer_setFrameDrop },
    { "_setStartSeekPos", "(I)V", (void *) IjkMediaPlayer_setStartSeekPos},
    { "_setMediaCodecEnabled", "(Z)V", (void *) IjkMediaPlayer_setMediaCodecEnabled },
    { "_setOpenSLESEnabled", "(Z)V", (void *) IjkMediaPlayer_setOpenSLESEnabled },
    { "setTextureId","(I)V",(void*)IjkMediaPlayer_setTextureId},
    {"setUseLibYuv","(I)V",(void*)IjkMediaPlayer_setUseLibYuv},
    { "setLoopNumber","(I)V",(void*)IjkMediaPlayer_setLoopNumber},
    {"setVideoDisable","(I)V",(void*)IjkMediaPlayer_setVideoDisable},
    { "setDomain","()V",(void*)IjkMediaPlayer_setDomain},
    { "_getColorFormatName", "(I)Ljava/lang/String;", (void *) IjkMediaPlayer_getColorFormatName },
    { "_getVideoCodecInfo", "()Ljava/lang/String;", (void *) IjkMediaPlayer_getVideoCodecInfo },
    { "_getAudioCodecInfo", "()Ljava/lang/String;", (void *) IjkMediaPlayer_getAudioCodecInfo },
    { "_getMediaMeta", "()Landroid/os/Bundle;", (void *) IjkMediaPlayer_getMediaMeta },

    { "getStatInfo", "(Ltv/danmaku/ijk/media/player/StatInfo;)V", (void *) IjkMediaPlayer_getStatInfo },
    { "setPlayControlParameters", "(ZZIIII)V", (void *) IjkMediaPlayer_setPlayControlParameters},
    { "setRealTimeFlag","(Z)V",(void*)IjkMediaPlayer_setRealTimeFlag},
    { "setCropMode", "(ZII)V", (void*) IjkMediaPlayer_setCropMode},
    { "onCCPlayerFirstBufferingComplete", "()V", (void*) IjkMediaPlayer_onCCPlayerFirstBufferingComplete},
    { "_setPlayerConfig", "(Ltv/danmaku/ijk/media/player/PlayerConfig;)V", (void *)IjkMediaPlayer_setPlayerConfig},
    { "_captureFrame", "()V", (void *)IjkMediaPlayer_captureFrame},
	{ "onGLSurfaceCreated", "()V", (void*)IjkMediaPlayer_onGLSurfaceCreated },
    { "onGLSurfaceChanged", "(II)V", (void*)IjkMediaPlayer_onGLSurfaceChanged },
    { "redraw", "()V", (void*)IjkMediaPlayer_redraw },
    { "redrawTexture", "(II)V", (void*)IjkMediaPlayer_redrawTexture },
    { "setDevMode", "(Z)V", (void*)IjkMediaPlayer_setDevMode },
	{ "setRadicalRealTimeFlag", "(Z)V", (void*)IjkMediaPlayer_setRadicalRealTimeFlag },
	{"setMuted","(I)V", (void*)IjkMediaPlayer_setMuted},
    { "_setPropertyFloat", "(IF)V", (void*)IjkMediaPlayer_setProperty},
    { "_getPropertyFloat", "(IF)F", (void*)IjkMediaPlayer_getProperty},
	{ "onPlayerLoadError", "(I)V", (void*)IjkMediaPlayer_onPlayerLoadError},
    { "setDecodeRawData", "(I)V", (void*)IjkMediaPlayer_setDecodeRawData},
    {"_setLanguage", "(Ljava/lang/String;Ljava/lang/String;)V", (void*) IjkMediaPlayer_setLanguage},
    {"_displaySubtitle", "(Z)V", (void*) IjkMediaPlayer_displaySubtitle},
    {"_setSaveToLocal",            "(Ljava/lang/String;)V",      (void *) IjkMediaPlayer_setSaveToLocal},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = NULL;

    g_jvm = vm;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != NULL);

    pthread_mutex_init(&g_clazz.mutex, NULL );

    // FindClass returns GrobleReference
    IJK_FIND_JAVA_CLASS(env, g_clazz.clazz, JNI_CLASS_IJKPLAYER);
    (*env)->RegisterNatives(env, g_clazz.clazz, g_methods, NELEM(g_methods) );

    g_clazz.mNativeMediaPlayer = (*env)->GetFieldID(env, g_clazz.clazz, "mNativeMediaPlayer", "J");
    IJK_CHECK_RET(g_clazz.mNativeMediaPlayer, -1, "missing mNativeMediaPlayer");

    g_clazz.mNativeCCPlayerStat = (*env)->GetFieldID(env, g_clazz.clazz, "mNativePlayerStat", "J");
    IJK_CHECK_RET(g_clazz.mNativeCCPlayerStat, -1, "missing mNativePlayerStat");

    g_clazz.mNativePlayerConfig = (*env)->GetFieldID(env, g_clazz.clazz, "mNativePlayerConfig","J");
    IJK_CHECK_RET(g_clazz.mNativePlayerConfig, -1, "missing mNativePlayerConfig");

    g_clazz.field_mNativeMediaDataSource = (*env)->GetFieldID(env,g_clazz.clazz,"mNativeMediaDataSource","J");
    J4A_loadClass__J4AC_tv_danmaku_ijk_media_player_misc_IMediaDataSource(env);

//    IJK_FIND_JAVA_CLASS(env, g_clazz.mjdf_clzz, "com/netease/cc/facedetect/DisplayFrame");
//    g_clazz.mjdf_data = (*env)->GetFieldID(env, g_clazz.mjdf_clzz, "data", "J");
//    g_clazz.mjdf_format = (*env)->GetFieldID(env, g_clazz.mjdf_clzz, "format", "I");
//    g_clazz.mjdf_width = (*env)->GetFieldID(env, g_clazz.mjdf_clzz, "width", "I");
//    g_clazz.mjdf_height = (*env)->GetFieldID(env, g_clazz.mjdf_clzz, "height", "I");
//    g_clazz.mjdf_pitch = (*env)->GetFieldID(env, g_clazz.mjdf_clzz, "pitch", "I");
//    g_clazz.mjdf_bits = (*env)->GetFieldID(env, g_clazz.mjdf_clzz, "bits", "I");

    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_postEventFromNative, g_clazz.clazz,
        "postEventFromNative", "(Ljava/lang/Object;IIILjava/lang/Object;)V");

    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_onSelectCodec, g_clazz.clazz,
        "onSelectCodec", "(Ljava/lang/Object;Ljava/lang/String;IILjava/lang/Object;)Ljava/lang/String;");

    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_onControlResolveSegmentCount, g_clazz.clazz,
        "onControlResolveSegmentCount", "(Ljava/lang/Object;)I");

    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_onControlResolveSegmentDuration, g_clazz.clazz,
        "onControlResolveSegmentDuration", "(Ljava/lang/Object;I)I");

    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_onControlResolveSegmentUrl, g_clazz.clazz,
        "onControlResolveSegmentUrl", "(Ljava/lang/Object;I)Ljava/lang/String;");

    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_onControlResolveSegmentOfflineMrl, g_clazz.clazz,
        "onControlResolveSegmentOfflineMrl", "(Ljava/lang/Object;I)Ljava/lang/String;");
	IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_requestRedraw, g_clazz.clazz,"requestRedraw", "(Ljava/lang/Object;)V");
    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_onRawImageAvailable, g_clazz.clazz,"onRawImageAvailable", "(Ljava/lang/Object;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;IIIIIII)V");
    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_sendHttpStat, g_clazz.clazz, "sendHttpStat", "(Ljava/lang/Object;Ljava/lang/String;)V");
    IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_sendFFplayerState, g_clazz.clazz, "sendFFplayerState", "(Ljava/lang/Object;IILjava/lang/String;)V");
	IJK_FIND_JAVA_STATIC_METHOD(env, g_clazz.jmid_OnVideoDecoderSelectedFromNative, g_clazz.clazz, "OnVideoDecoderSelectedFromNative", "(Ljava/lang/Object;Ljava/lang/String;I)V");

    ijkmp_global_init();

    FFmpegApi_global_init(env);

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *jvm, void *reserved)
{
    ijkmp_global_uninit();

    pthread_mutex_destroy(&g_clazz.mutex);
}

static void video_decoder_selected_callback(void *opaque, const char *url, int rawdata){

    jobject weak_thiz = (jobject) opaque;

    JNIEnv *env = NULL;

    if(JNI_OK!=SDL_JNI_SetupThreadEnv(&env)){
         ALOGE("IjkMediaPlayer_OnVideoDecoderSelectedFromNative setup up env fail ");
         return ;
    }
    jstring jurl = (*env)->NewStringUTF(env, url);
    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.jmid_OnVideoDecoderSelectedFromNative, weak_thiz, jurl, rawdata);
    if(SDL_JNI_CatchException(env)){
        ALOGE("IjkMediaPlayer_OnVideoDecoderSelectedFromNative call fail");
    }

}

//post_event替代方案，传递ffplayer中需要返回的状态或结果等（解决问题：stop时消息无法传出）
void IjkMediaPlayer_sendFFplayerState(void* weak_thiz, int event, int state, const char *info)
{
    if(!weak_thiz){
        ALOGE("IjkMediaPlayer_sendFFplayerState weak this is releaseed");
        return ;
    }
    JNIEnv *env = NULL;
    if(JNI_OK!=SDL_JNI_SetupThreadEnv(&env)){
        ALOGE("IjkMediaPlayer_sendFFplayerState setup up env fail");
        return ;
    }
    jstring jinfo = (*env)->NewStringUTF(env, info);
    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.jmid_sendFFplayerState, (jobject)weak_thiz, event, state, jinfo);
    if(SDL_JNI_CatchException(env)){
        ALOGE("IjkMediaPlayer_sendFFplayerState setup up env fail 2");
    }
}

void IjkMediaPlayer_sendHttpStat(IjkMediaPlayer *mp, const char *url) {
    if (!url || strlen(url) <= 0) {
        return;
    }
    jobject weak_thiz = (jobject) ijkmp_get_weak_thiz(mp);
    if(!weak_thiz){
        ALOGE("IjkMediaPlayer_sendHttpStat weak this is releaseed ");
        return ;
    }
    JavaVM *vm = g_jvm;
    JNIEnv *env = NULL;

    //(*vm)->AttachCurrentThread(vm, &env, NULL);

    if(JNI_OK!=SDL_JNI_SetupThreadEnv(&env)){
        ALOGE("IjkMediaPlayer_sendHttpStat setup up env fail 1");
        return ;
    }
    jstring jurl = (*env)->NewStringUTF(env, url);
    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.jmid_sendHttpStat, weak_thiz, jurl);
    if(SDL_JNI_CatchException(env)){
        ALOGE("IjkMediaPlayer_sendHttpStat setup up env fail 2");
    }
    (*vm)->DetachCurrentThread(vm);

}

/**
 * 以下只有android用到
 * ijkmp_stop_cc_player_stat
 * async_release_android
 * ijkmp_async_release_android
 *
 */
void ijkmp_stop_cc_player_stat(CCPlayerStat* cc_player_stat ) {
    MPTRACE("%s", __func__);
}

void release_java_filed(IjkMediaPlayer* mp, jobject thiz) {

    if (mp == NULL || thiz == NULL)
        return;

    JNIEnv *env = NULL;
    if(JNI_OK != SDL_JNI_SetupThreadEnv(&env)){
        ALOGE("release_java_filed setup up env fail 1");
        return;
    }

    //only delete weak_thiz at release
    jobject weak_thiz = (jobject) ijkmp_set_weak_thiz(mp, NULL );
    (*env)->DeleteGlobalRef(env, weak_thiz);

    jni_set_media_data_source(env, thiz, 0);

    //set IjkMediaPlayer.mNativeMediaPlayer to null
    if (jni_get_media_player(env, thiz) == mp)
        (*env)->SetLongField(env, thiz, g_clazz.mNativeMediaPlayer, (intptr_t)NULL);
    __sync_fetch_and_and(&mp->ref_count, 0);

    jni_set_cc_player_stat(env, thiz, NULL);

    (*env)->DeleteGlobalRef(env, thiz);

    if(SDL_JNI_CatchException(env)){
        ALOGE("[stat] release_java_filed setup up env fail 2");
    }
}

static int async_release_android(void *arg) {

    ReleaseArgs* args = (ReleaseArgs*)arg;
    IjkMediaPlayer *mp = args->mp;
    CCPlayerStat* cc_player_stat = args->cc_player_stat;
    MPTRACE("%s %p", __func__, mp);

    pthread_mutex_lock(&(mp->mutex));
    ijkmp_stop_cc_player_stat(cc_player_stat);
    pthread_mutex_unlock(&(mp->mutex));

    ijkmp_shutdown(mp);

    release_java_filed(mp, args->playerThiz);

    ijkmp_destroy(mp);

    free(arg);

    MPTRACE("%s end %p", __func__, mp);
    return 0;
}

void ijkmp_async_release_android(ReleaseArgs* args) {

    IjkMediaPlayer *mp = args->mp;
    if (NULL != mp && (NULL == mp->release_thread)) {
        mp->release_thread = SDL_CreateThreadEx(&mp->_release_thread, async_release_android, args, "release_tid");
        ALOGE("create async thread(%d)", mp->release_thread);
    } else {
        ALOGE("can not create async thread");
    }
}
