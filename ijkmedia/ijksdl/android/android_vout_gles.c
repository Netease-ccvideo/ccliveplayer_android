/*
 * android_vout_gles.c
 *
 *  Created on: 2015年9月21日
 *      Author: g7536
 */

#include "android_vout_gles.h"
#include "ijksdl_fourcc.h"
#include "ijksdl_vout.h"
#include <assert.h>
#include "../../ijkutil/android/loghelp.h"
#include "../yuv2rgb/yuv2rgb.h"
#include "libyuv.h"

static const char *const g_vertexShaderString = SHADER_STRING
(
    attribute vec4 position;
    attribute vec2 texcoord;
    uniform mat4 modelViewProjectionMatrix;
    varying vec2 v_texcoord;

    void main()
    {
        gl_Position = modelViewProjectionMatrix * position;
        v_texcoord = texcoord.xy;
    }
);

static const char *const g_yuvFragmentShaderString = SHADER_STRING
(
    varying highp vec2 v_texcoord;
    uniform sampler2D s_texture_y;
    uniform sampler2D s_texture_u;
    uniform sampler2D s_texture_v;

    void main()
    {
        highp float y = texture2D(s_texture_y, v_texcoord).r;
        highp float u = texture2D(s_texture_u, v_texcoord).r - 0.5;
        highp float v = texture2D(s_texture_v, v_texcoord).r - 0.5;

        highp float r = y +               1.40200 * v;
        highp float g = y - 0.34414 * u - 0.71414 * v;
        highp float b = y + 1.77200 * u;

        gl_FragColor = vec4(r,g,b,1.0);
    }
);

enum {
    ATTRIBUTE_VERTEX,
    ATTRIBUTE_TEXCOORD,
};

void sdl_android_on_glsurfaceview_created(SDL_GLSurfaceViewRender *render)
{
    ALOGI("%s", __func__);
    render->reset_shader = 1;
}

static void updateVertexCoordinate(SDL_GLSurfaceViewRender *render) {
    if (render == NULL) return;
    if (render->backing_width > 0 && render->backing_height > 0
        && render->frame_width > 0 && render->frame_height > 0) {
        //ALOGI("%s", __func__);
        if (render->backing_width * render->frame_height != render->backing_height * render->frame_width) {
            float backingAspect = render->backing_width / (float) render->backing_height;
            float frameAspect = render->frame_width / (float) render->frame_height;
            if (render->scale_mode == ScalingAspectFit) {
                if (backingAspect > frameAspect) {//horizontal padding
                    float nw = frameAspect / backingAspect;
                    render->vertices[0] = -nw;
                    render->vertices[1] = -1.0f;
                    render->vertices[2] = nw;
                    render->vertices[3] = -1.0f;
                    render->vertices[4] = -nw;
                    render->vertices[5] = 1.0f;
                    render->vertices[6] = nw;
                    render->vertices[7] = 1.0f;
                } else {//vertical padding
                    float nh = backingAspect / frameAspect;
                    render->vertices[0] = -1.0f;
                    render->vertices[1] = -nh;
                    render->vertices[2] = 1.0f;
                    render->vertices[3] = -nh;
                    render->vertices[4] = -1.0f;
                    render->vertices[5] = nh;
                    render->vertices[6] = 1.0f;
                    render->vertices[7] = nh;
                }
            } else if (render->scale_mode == ScalingAspectFill) {
                if (backingAspect > frameAspect) {
                    float nh = backingAspect / frameAspect;
                    render->vertices[0] = -1.0f;
                    render->vertices[1] = -nh;
                    render->vertices[2] = 1.0f;
                    render->vertices[3] = -nh;
                    render->vertices[4] = -1.0f;
                    render->vertices[5] = nh;
                    render->vertices[6] = 1.0f;
                    render->vertices[7] = nh;
                } else {
                    float nw = frameAspect / backingAspect;
                    render->vertices[0] = -nw;
                    render->vertices[1] = -1.0f;
                    render->vertices[2] = nw;
                    render->vertices[3] = -1.0f;
                    render->vertices[4] = -nw;
                    render->vertices[5] = 1.0f;
                    render->vertices[6] = nw;
                    render->vertices[7] = 1.0f;
                }
            }
        }
    }
}

static void updateTextureCoordinate(SDL_GLSurfaceViewRender *render)
{
//    ALOGI("%s surface: %d, %d video: %d, %d", __func__, render->backing_width, render->backing_height, render->frame_width, render->frame_height);
    if (render && render->target_overlay) {
        int bytePerPixel = 1;
        float paddingRight = 0.0f;
        int pitchWidth = render->target_overlay->pitches[0] / bytePerPixel;
        if (pitchWidth > render->frame_width) {
            paddingRight = (pitchWidth - render->frame_width) / (float)render->frame_width;
        }
        if (!render->target_overlay->crop) {
            render->texCoords[0] = 0.0f;
            render->texCoords[1] = 1.0f;
            render->texCoords[2] = 1.0f - paddingRight;
            render->texCoords[3] = 1.0f;
            render->texCoords[4] = 0.0f;
            render->texCoords[5] = 0.0f;
            render->texCoords[6] = 1.0f - paddingRight;
            render->texCoords[7] = 0.0f;
        } else {
            if (render->backing_width > 0
                && render->backing_height > 0
                && render->frame_width > 0
                && render->frame_height > 0
                && (render->backing_width * render->frame_height != render->backing_height * render->frame_width)) {
                float backingAspect = render->backing_width / (float) render->backing_height;
                float frameAspect = (render->frame_width) / (float) render->frame_height;
                if (backingAspect > frameAspect) {
                    float verticalCrop = 0.5f - 0.5f * frameAspect / backingAspect;
                    render->texCoords[0] = 0.0f;
                    render->texCoords[1] = 1.0f - verticalCrop;
                    render->texCoords[2] = 1.0f - paddingRight;
                    render->texCoords[3] = 1.0f - verticalCrop;
                    render->texCoords[4] = 0.0f;
                    render->texCoords[5] = verticalCrop;
                    render->texCoords[6] = 1.0f - paddingRight;
                    render->texCoords[7] = verticalCrop;
                } else {
                    float horizontalCrop = (render->frame_width - backingAspect * render->frame_height) / 2.0f / (float)pitchWidth;
                    render->texCoords[0] = horizontalCrop;
                    render->texCoords[1] = 1.0f;
                    render->texCoords[2] = 1.0f - paddingRight - horizontalCrop;
                    render->texCoords[3] = 1.0f;
                    render->texCoords[4] = horizontalCrop;
                    render->texCoords[5] = 0.0f;
                    render->texCoords[6] = 1.0f - paddingRight - horizontalCrop;
                    render->texCoords[7] = 0.0f;
                }
            } else {
                render->texCoords[0] = 0.0f;
                render->texCoords[1] = 1.0f;
                render->texCoords[2] = 1.0f - paddingRight;
                render->texCoords[3] = 1.0f;
                render->texCoords[4] = 0.0f;
                render->texCoords[5] = 0.0f;
                render->texCoords[6] = 1.0f - paddingRight;
                render->texCoords[7] = 0.0f;
            }
        }
    }
}

static GLuint compileShader(GLenum type, const char *shaderString) {
    GLint status;
    GLuint shader = glCreateShader(type);
    if (shader == 0 || shader == GL_INVALID_ENUM) {
        ALOGE("failed to create shader %d, %s", type, shaderString);
        return 0;
    }
    glShaderSource(shader, 1, &shaderString, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        glDeleteShader(shader);
        ALOGE("failed to compile shader %d, %s", type, shaderString);
        return 0;
    }
    return shader;
}

static bool loadShaders(SDL_GLSurfaceViewRender *render) {
    ALOGD("%s", __func__);
    if (render->program) {
        glDeleteProgram(render->program);
        render->program = 0;
    }
//    updateVertexCoordinate(render);
    bool result = false;
    GLuint vertShader = 0, fragShader = 0;
    render->program = glCreateProgram();
    ALOGD("glCreateProgram %d", render->program);
    vertShader = compileShader(GL_VERTEX_SHADER, g_vertexShaderString);
    if (!vertShader) {
        goto exit;
    }
    ALOGD("compile vertex shader %d", vertShader);
    fragShader = compileShader(GL_FRAGMENT_SHADER, g_yuvFragmentShaderString);
    if (!fragShader) {
        goto exit;
    }
    ALOGD("compile frag shader %d", fragShader);
    glAttachShader(render->program, vertShader);
    glAttachShader(render->program, fragShader);
    glBindAttribLocation(render->program, ATTRIBUTE_VERTEX, "position");
    glBindAttribLocation(render->program, ATTRIBUTE_TEXCOORD, "texcoord");
    glLinkProgram(render->program);
    GLint linkStatus;
    glGetProgramiv(render->program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
        ALOGE("failed to link program");
        goto exit;
    }
    GLint validateStatus;
    glValidateProgram(render->program);
    glGetProgramiv(render->program, GL_VALIDATE_STATUS, &validateStatus);
    if (validateStatus == GL_FALSE) {
        ALOGE("failed to validate program");
        result = false;
        goto exit;
    }
    result = true;
    render->uniform_matrix = glGetUniformLocation(render->program, "modelViewProjectionMatrix");
    render->uniform_samplers[0] = glGetUniformLocation(render->program, "s_texture_y");
    render->uniform_samplers[1] = glGetUniformLocation(render->program, "s_texture_u");
    render->uniform_samplers[2] = glGetUniformLocation(render->program, "s_texture_v");

    exit:
    if (vertShader) {
        glDeleteShader(vertShader);
    }
    if (fragShader) {
        glDeleteShader(fragShader);
    }
    if (result) {
        ALOGI("ok setup GL program");
        render->reset_shader = 0;
    } else {
        glDeleteProgram(render->program);
        render->program = 0;
    }
    return result;
}

void sdl_android_on_glsurfaceview_changed(SDL_GLSurfaceViewRender *render, int width, int height)
{
    ALOGI("%s width=%d height=%d", __func__, width, height);
//    glViewport(0, 0, width, height);
    if (render->backing_width != width || render->backing_height != height) {
        render->backing_width = width;
        render->backing_height = height;
//        updateTextureCoordinate(render);
    }
//    updateVertexCoordinate(render);
}

static void mat4f_LoadOrtho(float left, float right, float bottom, float top, float near, float far, float* mout)
{
    float r_l = right - left;
    float t_b = top - bottom;
    float f_n = far - near;
    float tx = - (right + left) / (right - left);
    float ty = - (top + bottom) / (top - bottom);
    float tz = - (far + near) / (far - near);

    mout[0] = 2.0f / r_l;
    mout[1] = 0.0f;
    mout[2] = 0.0f;
    mout[3] = 0.0f;

    mout[4] = 0.0f;
    mout[5] = 2.0f / t_b;
    mout[6] = 0.0f;
    mout[7] = 0.0f;

    mout[8] = 0.0f;
    mout[9] = 0.0f;
    mout[10] = -2.0f / f_n;
    mout[11] = 0.0f;

    mout[12] = tx;
    mout[13] = ty;
    mout[14] = tz;
    mout[15] = 1.0f;
}

int sdl_android_display_I420_l_hw(SDL_Vout *vout, SDL_GLSurfaceViewRender *render, int w, int h)
{
    if (render == NULL) return 0;
    render->backing_width = w;
    render->backing_height = h;
    if(render->backing_width <= 0 || render->backing_height <= 0) {
        ALOGI("[render] error backing_width:%d backing_height:%d", render->backing_width, render->backing_height);
        return 0;
    }
    SDL_VoutOverlay *overlay = render->target_overlay;
    render->frmae_chroma = SDL_FCC_I420;
    if (overlay == NULL) return 0;

    if (render->frame_width != overlay->w || render->frame_height != overlay->h) {
        render->frame_width = overlay->w;
        render->frame_height = overlay->h;
    }

    if (!render->program || render->reset_shader) {
        loadShaders(render);
    }

    updateTextureCoordinate(render);
    glViewport(0, 0, render->backing_width, render->backing_height);

    glClearColor(1.0f, 1.0f, 0.0f, /*render->transparent_background ? 0.0f : */1.0f);

    glClear(GL_COLOR_BUFFER_BIT);

    if (overlay->pitches[0] <= 0 || overlay->pitches[1] <= 0 || overlay->pitches[2] <= 0)
        return 0;

    glUseProgram(render->program);

    assert(overlay->planes);
    assert(overlay->format == SDL_FCC_I420);
    assert(overlay->planes == 3);
    const int frameHeight = overlay->h;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (0 == render->textures[0]) {
        glGenTextures(3, render->textures);
    }

    const Uint8 *pixels[3] = { overlay->pixels[0], overlay->pixels[1], overlay->pixels[2] };
    const int width[3] = { overlay->pitches[0], overlay->pitches[1], overlay->pitches[2] };
    const int height[3] = { frameHeight, frameHeight / 2, frameHeight / 2 };

    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, render->textures[i]);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_LUMINANCE,
                     width[i],
                     height[i],
                     0,
                     GL_LUMINANCE,
                     GL_UNSIGNED_BYTE,
                     pixels[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    for (int i = 0; i < 3; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, render->textures[i]);
        glUniform1i(render->uniform_samplers[i], i);
    }
    GLfloat modelviewProj[16];
    mat4f_LoadOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, modelviewProj);
    glUniformMatrix4fv(render->uniform_matrix, 1, GL_FALSE, modelviewProj);
    glVertexAttribPointer(ATTRIBUTE_VERTEX, 2, GL_FLOAT, 0, 0, render->vertices);
    glEnableVertexAttribArray(ATTRIBUTE_VERTEX);
    glVertexAttribPointer(ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, 0, 0, render->texCoords);
    glEnableVertexAttribArray(ATTRIBUTE_TEXCOORD);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    return 0;
}

int sdl_android_display_I420_l(SDL_Vout *vout, SDL_GLSurfaceViewRender *render)
{
    SDL_VoutOverlay *overlay = render->target_overlay;
    render->frmae_chroma = SDL_FCC_I420;
    SDL_Vout_Opaque *opaque = vout->opaque;

    if (render->frame_width != overlay->w || render->frame_height != overlay->h) {
        render->frame_width = overlay->w;
        render->frame_height = overlay->h;
    }
    assert(overlay->planes);
    assert(overlay->format == SDL_FCC_I420);
    assert(overlay->planes == 3);
    const int frameHeight = overlay->h;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    render->textures[0] = vout->nYTexture;

    const Uint8 *pixels[3] = { overlay->pixels[0], overlay->pixels[1], overlay->pixels[2] };
    const int width[3] = { overlay->pitches[0], overlay->pitches[1], overlay->pitches[2] };
    const int height[3] = { frameHeight, frameHeight / 2, frameHeight / 2 };
    vout->nWidth = overlay->pitches[0];
    vout->nHeight = frameHeight;
    int bufsize = overlay->w * overlay->h * 3;

    if(vout->rgb == 0 || vout->rgb_size != bufsize)
    {
        if(vout->rgb) free(vout->rgb);
        vout->rgb_size = bufsize;
        vout->rgb = (unsigned char*)malloc(vout->rgb_size);
    }
    if(vout->useLibYuv == 1)
    {
        I420ToRGB24(overlay->pixels[0],overlay->pitches[0],overlay->pixels[2],
                    overlay->pitches[2],overlay->pixels[1],overlay->pitches[1],vout->rgb,overlay->w*3,overlay->w,overlay->h);
    }
    else
        yuv420_2_rgb888(vout->rgb,
                        overlay->pixels[0],
                        overlay->pixels[2],
                        overlay->pixels[1],
                        overlay->w,
                        overlay->h,
                        overlay->pitches[0],
                        overlay->pitches[1],
                        overlay->w * 3,
                        yuv2rgb565_table,
                        0);

    for (int i = 0; i < 1; i++) {
        GLint whichID;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &whichID);
        glBindTexture(GL_TEXTURE_2D, render->textures[i]);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGB,
                     overlay->w,
                     overlay->h,
                     0,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     vout->rgb);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, whichID);
    }

    /* GraphicBuffer * pGraphicBuffer = new GraphicBuffer(ImageWidth, ImageHeight, PIXEL_FORMAT_RGB_565, GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE);

     // Lock the buffer to get a pointer
     unsigned char * pBitmap = NULL;
     pGraphicBuffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,(void **)&pBitmap);

     // Write 2D image to pBitmap

     // Unlock to allow OpenGL ES to use it
     pGraphicBuffer->unlock();

     EGLClientBuffer ClientBufferAddress = pGraphicBuffer->getNativeBuffer();
     if (buffer == NULL) {
         buffer = (ANativeWindow_Buffer*)alloca(sizeof(ANativeWindow_Buffer));
         buffer->width = overlay->w;
         buffer->stride = overlay->w;
         buffer->height = overlay->h;
         buffer->format = WINDOW_FORMAT_RGB_565;
         buffer->bits = vout->rgb;
     }

     if(img == NULL)
         img = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY),
                                         EGL_NO_CONTEXT,
                                         EGL_NATIVE_BUFFER_ANDROID,
                                         buffer, //(EGLClientBuffer)buffer->getNativeBuffer(),
                                         eglImgAttrs);

     glBindTexture(GL_TEXTURE_EXTERNAL_OES, render->textures[0]);
     glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, img);*/

    return 0;
}
