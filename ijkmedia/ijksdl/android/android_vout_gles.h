/*
 * android_vout_gles.h
 *
 *  Created on: 2015年9月21日
 *      Author: g7536
 */

#ifndef IJKMEDIA_IJKSDL_ANDROID_ANDROID_VOUT_GLES_H_
#define IJKMEDIA_IJKSDL_ANDROID_ANDROID_VOUT_GLES_H_

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "ijksdl_vout.h"
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#define SHADER_STRING(text) #text

typedef struct SDL_VoutOverlay SDL_VoutOverlay;

typedef struct SDL_GLFrameCallback {
    int (*gl_frame_callback)(int);
} SDL_GLFrameCallback;

typedef enum {
    ScalingAspectFit = 0,
    ScalingAspectFill = 1,
} Scale_Mode;

typedef struct SDL_GLSurfaceViewRender {
    GLint backing_width;
    GLint backing_height;
    GLuint program;
    GLint uniform_matrix;

    GLfloat vertices[8];
    GLfloat texCoords[8];

    GLint uniform_samplers[3];
    GLuint textures[3];

    int frame_width;
    int frame_height;
    int frmae_chroma;
    int reset_shader;
    Scale_Mode scale_mode;
    SDL_VoutOverlay *target_overlay;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;
    EGLDisplay display;

} SDL_GLSurfaceViewRender;

int sdl_android_display_I420_l(SDL_Vout *vout, SDL_GLSurfaceViewRender *render);
int sdl_android_display_I420_l_hw(SDL_Vout *vout, SDL_GLSurfaceViewRender *render, int width, int height);
void sdl_android_on_glsurfaceview_created(SDL_GLSurfaceViewRender *render);
void sdl_android_on_glsurfaceview_changed(SDL_GLSurfaceViewRender *render, int width, int height);

#endif /* IJKMEDIA_IJKSDL_ANDROID_ANDROID_VOUT_GLES_H_ */
