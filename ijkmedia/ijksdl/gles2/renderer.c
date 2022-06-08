/*
 * copyright (c) 2016 Zhang Rui <bbcallen@gmail.com>
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

#include "internal.h"

static void IJK_GLES2_printProgramInfo(GLuint program)
{
    if (!program)
        return;

    GLint info_len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
    if (!info_len) {
        ALOGE("[GLES2][Program] empty info\n");
        return;
    }

    char    buf_stack[32];
    char   *buf_heap = NULL;
    char   *buf      = buf_stack;
    GLsizei buf_len  = sizeof(buf_stack) - 1;
    if (info_len > sizeof(buf_stack)) {
        buf_heap = (char*) malloc(info_len + 1);
        if (buf_heap) {
            buf     = buf_heap;
            buf_len = info_len;
        }
    }

    glGetProgramInfoLog(program, buf_len, NULL, buf);
    ALOGE("[GLES2][Program] error %s\n", buf);

    if (buf_heap)
        free(buf_heap);
}

void IJK_GLES2_Renderer_reset(IJK_GLES2_Renderer *renderer)
{
    if (!renderer)
        return;

    if (renderer->vertex_shader)
        glDeleteShader(renderer->vertex_shader);
    if (renderer->fragment_shader)
        glDeleteShader(renderer->fragment_shader);
    if (renderer->program)
        glDeleteProgram(renderer->program);

    renderer->vertex_shader   = 0;
    renderer->fragment_shader = 0;
    renderer->program         = 0;

    for (int i = 0; i < IJK_GLES2_MAX_PLANE; ++i) {
        if (renderer->plane_textures[i]) {
            glDeleteTextures(1, &renderer->plane_textures[i]);
            renderer->plane_textures[i] = 0;
        }
    }
}

void IJK_GLES2_Renderer_free(IJK_GLES2_Renderer *renderer)
{
    if (!renderer)
        return;

    if (renderer->func_destroy)
        renderer->func_destroy(renderer);

#if 0
    if (renderer->vertex_shader)    ALOGW("[GLES2] renderer: vertex_shader not deleted.\n");
    if (renderer->fragment_shader)  ALOGW("[GLES2] renderer: fragment_shader not deleted.\n");
    if (renderer->program)          ALOGW("[GLES2] renderer: program not deleted.\n");

    for (int i = 0; i < IJK_GLES2_MAX_PLANE; ++i) {
        if (renderer->plane_textures[i])
            ALOGW("[GLES2] renderer: plane texture[%d] not deleted.\n", i);
    }
#endif
    
#ifdef __APPLE__
    free(renderer->vVertices);
    free(renderer->vTextCoord);
    free(renderer->indices);
#endif
    free(renderer);
}

void IJK_GLES2_Renderer_freeP(IJK_GLES2_Renderer **renderer)
{
    if (!renderer || !*renderer)
        return;

    IJK_GLES2_Renderer_free(*renderer);
    *renderer = NULL;
}

IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_base(const char *fragment_shader_source)
{
    assert(fragment_shader_source);

    IJK_GLES2_Renderer *renderer = (IJK_GLES2_Renderer *)calloc(1, sizeof(IJK_GLES2_Renderer));
    if (!renderer)
        goto fail;

    renderer->vertex_shader = IJK_GLES2_loadShader(GL_VERTEX_SHADER, IJK_GLES2_getVertexShader_default());
    if (!renderer->vertex_shader)
        goto fail;

    renderer->fragment_shader = IJK_GLES2_loadShader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (!renderer->fragment_shader)
        goto fail;

    renderer->program = glCreateProgram();                          IJK_GLES2_checkError("glCreateProgram");
    if (!renderer->program)
        goto fail;

    glAttachShader(renderer->program, renderer->vertex_shader);     IJK_GLES2_checkError("glAttachShader(vertex)");
    glAttachShader(renderer->program, renderer->fragment_shader);   IJK_GLES2_checkError("glAttachShader(fragment)");
    glLinkProgram(renderer->program);                               IJK_GLES2_checkError("glLinkProgram");
    GLint link_status = GL_FALSE;
    glGetProgramiv(renderer->program, GL_LINK_STATUS, &link_status);
    if (!link_status)
        goto fail;


    renderer->av4_position = glGetAttribLocation(renderer->program, "av4_Position");                IJK_GLES2_checkError_TRACE("glGetAttribLocation(av4_Position)");
    renderer->av2_texcoord = glGetAttribLocation(renderer->program, "av2_Texcoord");                IJK_GLES2_checkError_TRACE("glGetAttribLocation(av2_Texcoord)");
    renderer->um4_mvp      = glGetUniformLocation(renderer->program, "um4_ModelViewProjection");    IJK_GLES2_checkError_TRACE("glGetUniformLocation(um4_ModelViewProjection)");

    return renderer;

fail:

    if (renderer->program)
        IJK_GLES2_printProgramInfo(renderer->program);

    IJK_GLES2_Renderer_free(renderer);
    return NULL;
}

IJK_GLES2_Renderer *IJK_GLES2_Renderer_create(SDL_VoutOverlay *overlay)
{
    if (!overlay)
        return NULL;
    
    IJK_GLES2_printString("Version", GL_VERSION);
    IJK_GLES2_printString("Vendor", GL_VENDOR);
    IJK_GLES2_printString("Renderer", GL_RENDERER);
    IJK_GLES2_printString("Extensions", GL_EXTENSIONS);
    
    IJK_GLES2_Renderer *renderer = NULL;
    switch (overlay->format) {
        case SDL_FCC_RV16:      renderer = IJK_GLES2_Renderer_create_rgb565(); break;
        case SDL_FCC_RV24:      renderer = IJK_GLES2_Renderer_create_rgb888(); break;
        case SDL_FCC_RV32:      renderer = IJK_GLES2_Renderer_create_rgbx8888(); break;
#ifdef __APPLE__
        case SDL_FCC_NV12:      renderer = IJK_GLES2_Renderer_create_yuv420sp(); break;
        case SDL_FCC__VTB:      renderer = IJK_GLES2_Renderer_create_yuv420sp_vtb(overlay); break;
#endif
        case SDL_FCC_YV12:      renderer = IJK_GLES2_Renderer_create_yuv420p(); break;
        case SDL_FCC_I420:      renderer = IJK_GLES2_Renderer_create_yuv420p(); break;
        case SDL_FCC_I444P10LE: renderer = IJK_GLES2_Renderer_create_yuv444p10le(); break;
        default:
            ALOGE("[GLES2] unknown format %4s(%d)\n", (char *)&overlay->format, overlay->format);
            return NULL;
    }
    
    renderer->format = overlay->format;
    return renderer;
}
#ifdef __APPLE__
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create2(SDL_VoutOverlay *overlay, bool panorama)
{
    if (!overlay)
        return NULL;

    IJK_GLES2_printString("Version", GL_VERSION);
    IJK_GLES2_printString("Vendor", GL_VENDOR);
    IJK_GLES2_printString("Renderer", GL_RENDERER);
    IJK_GLES2_printString("Extensions", GL_EXTENSIONS);

    IJK_GLES2_Renderer *renderer = NULL;
    switch (overlay->format) {
        case SDL_FCC_RV16:      renderer = IJK_GLES2_Renderer_create_rgb565(); break;
        case SDL_FCC_RV24:      renderer = IJK_GLES2_Renderer_create_rgb888(); break;
        case SDL_FCC_RV32:      renderer = IJK_GLES2_Renderer_create_rgbx8888(); break;
#ifdef __APPLE__
        case SDL_FCC_NV12:      renderer = IJK_GLES2_Renderer_create_yuv420sp(); break;
        case SDL_FCC__VTB:      renderer = IJK_GLES2_Renderer_create_yuv420sp_vtb(overlay); break;
#endif
        case SDL_FCC_YV12:      renderer = IJK_GLES2_Renderer_create_yuv420p(); break;
        case SDL_FCC_I420:      renderer = IJK_GLES2_Renderer_create_yuv420p(); break;
        case SDL_FCC_I444P10LE: renderer = IJK_GLES2_Renderer_create_yuv444p10le(); break;
        default:
            ALOGE("[GLES2] unknown format %4s(%d)\n", (char *)&overlay->format, overlay->format);
            return NULL;
    }
    renderer->bytesPerPixel = 1;
    if(overlay->format == SDL_FCC_RV24 || overlay->format == SDL_FCC_RV32)
        renderer->bytesPerPixel = 3;
    renderer->format = overlay->format;
    renderer->panorama = panorama;
    if(panorama) {
        IJK_GLES2_Renderer_Panorama_GenSphere(renderer);
    }
    return renderer;
}

GLboolean IJK_GLES2_Renderer_Panorama_updateMVP(IJK_GLES2_Renderer *renderer,const GLfloat* value)
{
    if(!renderer) return GL_FALSE;
    glUniformMatrix4fv(renderer->um4_mvp, 1, GL_FALSE, value);                    IJK_GLES2_checkError_TRACE("glUniformMatrix4fv(um4_mvp)");
    return GL_TRUE;
}

GLboolean IJK_GLES2_Renderer_Panorama_GlassMode(IJK_GLES2_Renderer *renderer,bool glass)
{
    if(!renderer) return GL_FALSE;
    renderer->glass = glass;
    return GL_TRUE;
}

GLboolean IJK_GLES2_Renderer_Panorama_Refresh2Normal(IJK_GLES2_Renderer *renderer)
{
    if(!renderer) return GL_FALSE;
    glViewport( 0, 0, renderer->layer_width,renderer->layer_height);
    glVertexAttribPointer(renderer->av4_position, 3, GL_FLOAT, 0, 0, renderer->vVertices);
    glEnableVertexAttribArray(renderer->av4_position);
    glVertexAttribPointer(renderer->av2_texcoord, 2, GL_FLOAT, 0, 0, renderer->vTextCoord);
    glEnableVertexAttribArray(renderer->av2_texcoord);
    glDrawElements(GL_TRIANGLE_STRIP, renderer->numIndices, GL_UNSIGNED_SHORT, renderer->indices);
    return GL_TRUE;
}

static void IJK_GLES2_Renderer_TexCoords_crop(IJK_GLES2_Renderer *renderer)
{
    //av_log(NULL,AV_LOG_ERROR,"[zhao] (frameWidth/frameHeight = %d : %d) | _backingWidth/_backHeight = %d : %d \n",renderer->frame_width, renderer->frame_height,renderer->layer_width, renderer->layer_height);
    float _rightPaddingPixels = 0.0f;
    float _rightPadding = 0.0f;
    float _pitchWidth = renderer->buffer_width;
    //av_log(NULL,AV_LOG_ERROR,"[zhao] pitchWidth =  %f \n",_pitchWidth);
    if(renderer->bytesPerPixel == 0)
        renderer->bytesPerPixel = 1;
    if (_pitchWidth / renderer->bytesPerPixel > renderer->frame_width) {
        _rightPaddingPixels = _pitchWidth / renderer->bytesPerPixel - renderer->frame_width;
        //ALOGE("[bytesPerPixel = %d rightPaddingPixels = %f \n]",renderer->bytesPerPixel,_rightPaddingPixels);
    }
    if (renderer->frame_width > 0)
        _rightPadding = ((GLfloat)_rightPaddingPixels) / renderer->frame_width;
        //ALOGE("[zhao] get rightPadding = %f\n",_rightPadding);
    
    if (renderer->videoCrop) {
        float _frameWidth = renderer->frame_width;
        float _frameHeight = renderer->frame_height;
        float _layerWidth = renderer->layer_width;
        float _layerHeight = renderer->layer_height;
        if (_layerWidth > 0
            && _layerHeight > 0
            && _frameWidth > 0
            && _frameHeight > 0
            && (_layerWidth * _frameHeight != _layerHeight * _frameWidth)) {
            float backingAspect = _layerWidth / (float) _layerHeight;
            float frameAspect = _frameWidth / (float) _frameHeight;
            //av_log(NULL, AV_LOG_ERROR, "[zhao] go to verticalCrop = %f rightPadding = %f\n",backingAspect,frameAspect);
            if (backingAspect > frameAspect) {
                float verticalCrop = 0.5f - 0.5f * frameAspect / backingAspect;
               // av_log(NULL, AV_LOG_ERROR, "[zhao] go to verticalCrop = %f rightPadding = %f\n",verticalCrop,_rightPadding);
                renderer->texcoords[0] = 0.0f;
                renderer->texcoords[1] = 1.0f - verticalCrop;
                renderer->texcoords[2] = 1.0f - _rightPadding;
                renderer->texcoords[3] = 1.0f - verticalCrop;
                renderer->texcoords[4] = 0.0f;
                renderer->texcoords[5] = verticalCrop;
                renderer->texcoords[6] = 1.0f - _rightPadding;
                renderer->texcoords[7] = verticalCrop;
            } else {
                float horizontalCrop = (_frameWidth - backingAspect * _frameHeight) / 2.0f / (float)_pitchWidth;
               // av_log(NULL, AV_LOG_ERROR, "[zhao] go to horizontalCrop = %f rightPadding = %f\n",horizontalCrop,_rightPadding);
                renderer->texcoords[0] = horizontalCrop;
                renderer->texcoords[1] = 1.0f;
                renderer->texcoords[2] = 1.0f - _rightPadding - horizontalCrop;
                renderer->texcoords[3] = 1.0f;
                renderer->texcoords[4] = horizontalCrop;
                renderer->texcoords[5] = 0.0f;
                renderer->texcoords[6] = 1.0f - _rightPadding - horizontalCrop;
                renderer->texcoords[7] = 0.0f;
            }
        } else {
          //  av_log(NULL, AV_LOG_ERROR, "[zhao] go to normal1 rightPadding = %f\n",_rightPadding);
            renderer->texcoords[0] = 0.0f;
            renderer->texcoords[1] = 1.0f;
            renderer->texcoords[2] = 1.0f - _rightPadding;
            renderer->texcoords[3] = 1.0f;
            renderer->texcoords[4] = 0.0f;
            renderer->texcoords[5] = 0.0f;
            renderer->texcoords[6] = 1.0f - _rightPadding;
            renderer->texcoords[7] = 0.0f;
        }
    } else {
      //  av_log(NULL, AV_LOG_ERROR, "[zhao] go to normal2 rightPadding = %f\n",_rightPadding);
        renderer->texcoords[0] = 0.0f;
        renderer->texcoords[1] = 1.0f;
        renderer->texcoords[2] = 1.0f - _rightPadding;
        renderer->texcoords[3] = 1.0f;
        renderer->texcoords[4] = 0.0f;
        renderer->texcoords[5] = 0.0f;
        renderer->texcoords[6] = 1.0f - _rightPadding;
        renderer->texcoords[7] = 0.0f;
    }
}

#endif

GLboolean IJK_GLES2_Renderer_isValid(IJK_GLES2_Renderer *renderer)
{
    return renderer && renderer->program ? GL_TRUE : GL_FALSE;
}

GLboolean IJK_GLES2_Renderer_isFormat(IJK_GLES2_Renderer *renderer, int format)
{
    if (!IJK_GLES2_Renderer_isValid(renderer))
        return GL_FALSE;

    return renderer->format == format ? GL_TRUE : GL_FALSE;
}

/*
 * Per-Context routine
 */
GLboolean IJK_GLES2_Renderer_setupGLES()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);       IJK_GLES2_checkError_TRACE("glClearColor");
    glEnable(GL_CULL_FACE);                     IJK_GLES2_checkError_TRACE("glEnable(GL_CULL_FACE)");
    glCullFace(GL_BACK);                        IJK_GLES2_checkError_TRACE("glCullFace");
    glDisable(GL_DEPTH_TEST);

    return GL_TRUE;
}

static void IJK_GLES2_Renderer_Vertices_reset(IJK_GLES2_Renderer *renderer)
{
    renderer->vertices[0] = -1.0f;
    renderer->vertices[1] = -1.0f;
    renderer->vertices[2] =  1.0f;
    renderer->vertices[3] = -1.0f;
    renderer->vertices[4] = -1.0f;
    renderer->vertices[5] =  1.0f;
    renderer->vertices[6] =  1.0f;
    renderer->vertices[7] =  1.0f;
}

static void IJK_GLES2_Renderer_Vertices_apply(IJK_GLES2_Renderer *renderer)
{
    switch (renderer->gravity) {
        case IJK_GLES2_GRAVITY_RESIZE_ASPECT:
            break;
        case IJK_GLES2_GRAVITY_RESIZE_ASPECT_FILL:
            break;
        case IJK_GLES2_GRAVITY_RESIZE:
            IJK_GLES2_Renderer_Vertices_reset(renderer);
            return;
        default:
            ALOGE("[GLES2] unknown gravity %d\n", renderer->gravity);
            IJK_GLES2_Renderer_Vertices_reset(renderer);
            return;
    }

    if (renderer->layer_width <= 0 ||
        renderer->layer_height <= 0 ||
        renderer->frame_width <= 0 ||
        renderer->frame_height <= 0)
    {
        ALOGE("[GLES2] invalid width/height for gravity aspect\n");
        IJK_GLES2_Renderer_Vertices_reset(renderer);
        return;
    }

    float width     = renderer->frame_width;
    float height    = renderer->frame_height;

    if (renderer->frame_sar_num > 0 && renderer->frame_sar_den > 0) {
        width = width * renderer->frame_sar_num / renderer->frame_sar_den;
    }
#ifdef __APPLE__
    if(renderer->videoCrop) {
        double dH = renderer->layer_height * 1.0f / renderer->frame_height;
        double ratio = renderer->layer_width / (dH * renderer->frame_width);
        width = width * ratio;
       // av_log(NULL, AV_LOG_ERROR, "[zhao] width ratio %f\n",width);
    }
#endif
    const float dW  = (float)renderer->layer_width	/ width;
    const float dH  = (float)renderer->layer_height / height;
    float dd        = 1.0f;
    float nW        = 1.0f;
    float nH        = 1.0f;

    switch (renderer->gravity) {
        case IJK_GLES2_GRAVITY_RESIZE_ASPECT_FILL:  dd = FFMAX(dW, dH); break;
        case IJK_GLES2_GRAVITY_RESIZE_ASPECT:       dd = FFMIN(dW, dH); break;
    }

    nW = (width  * dd / (float)renderer->layer_width);
    nH = (height * dd / (float)renderer->layer_height);
//    av_log(NULL, AV_LOG_ERROR, "[zhao] nW nH = %f:%f\n",nW,nH);
    renderer->vertices[0] = - nW;
    renderer->vertices[1] = - nH;
    renderer->vertices[2] =   nW;
    renderer->vertices[3] = - nH;
    renderer->vertices[4] = - nW;
    renderer->vertices[5] =   nH;
    renderer->vertices[6] =   nW;
    renderer->vertices[7] =   nH;
}

static void IJK_GLES2_Renderer_Vertices_reloadVertex(IJK_GLES2_Renderer *renderer)
{
    glVertexAttribPointer(renderer->av4_position, 2, GL_FLOAT, GL_FALSE, 0, renderer->vertices);    IJK_GLES2_checkError_TRACE("glVertexAttribPointer(av2_texcoord)");
    glEnableVertexAttribArray(renderer->av4_position);                                      IJK_GLES2_checkError_TRACE("glEnableVertexAttribArray(av2_texcoord)");
}

#define IJK_GLES2_GRAVITY_MIN                   (0)
#define IJK_GLES2_GRAVITY_RESIZE                (0) // Stretch to fill layer bounds.
#define IJK_GLES2_GRAVITY_RESIZE_ASPECT         (1) // Preserve aspect ratio; fit within layer bounds.
#define IJK_GLES2_GRAVITY_RESIZE_ASPECT_FILL    (2) // Preserve aspect ratio; fill layer bounds.
#define IJK_GLES2_GRAVITY_MAX                   (2)

GLboolean IJK_GLES2_Renderer_setGravity(IJK_GLES2_Renderer *renderer, int gravity, GLsizei layer_width, GLsizei layer_height)
{
    if (renderer->gravity != gravity && gravity >= IJK_GLES2_GRAVITY_MIN && gravity <= IJK_GLES2_GRAVITY_MAX)
        renderer->vertices_changed = 1;
    else if (renderer->layer_width != layer_width)
        renderer->vertices_changed = 1;
    else if (renderer->layer_height != layer_height)
        renderer->vertices_changed = 1;
    else
        return GL_TRUE;

    renderer->gravity      = gravity;
    renderer->layer_width  = layer_width;
    renderer->layer_height = layer_height;
    return GL_TRUE;
}

static void IJK_GLES2_Renderer_TexCoords_reset(IJK_GLES2_Renderer *renderer)
{
    renderer->texcoords[0] = 0.0f;
    renderer->texcoords[1] = 1.0f;
    renderer->texcoords[2] = 1.0f;
    renderer->texcoords[3] = 1.0f;
    renderer->texcoords[4] = 0.0f;
    renderer->texcoords[5] = 0.0f;
    renderer->texcoords[6] = 1.0f;
    renderer->texcoords[7] = 0.0f;
}

static void IJK_GLES2_Renderer_TexCoords_cropRight(IJK_GLES2_Renderer *renderer, GLfloat cropRight)
{
    ALOGE("IJK_GLES2_Renderer_TexCoords_cropRight\n");
    renderer->texcoords[0] = 0.0f;
    renderer->texcoords[1] = 1.0f;
    renderer->texcoords[2] = 1.0f - cropRight;
    renderer->texcoords[3] = 1.0f;
    renderer->texcoords[4] = 0.0f;
    renderer->texcoords[5] = 0.0f;
    renderer->texcoords[6] = 1.0f - cropRight;
    renderer->texcoords[7] = 0.0f;
}

static void IJK_GLES2_Renderer_TexCoords_reloadVertex(IJK_GLES2_Renderer *renderer)
{
    glVertexAttribPointer(renderer->av2_texcoord, 2, GL_FLOAT, GL_FALSE, 0, renderer->texcoords);   IJK_GLES2_checkError_TRACE("glVertexAttribPointer(av2_texcoord)");
    glEnableVertexAttribArray(renderer->av2_texcoord);                                              IJK_GLES2_checkError_TRACE("glEnableVertexAttribArray(av2_texcoord)");
}

/*
 * Per-Renderer routine
 */
GLboolean IJK_GLES2_Renderer_use(IJK_GLES2_Renderer *renderer)
{
    if (!renderer)
        return GL_FALSE;

    assert(renderer->func_use);
    if (!renderer->func_use(renderer))
        return GL_FALSE;

    IJK_GLES_Matrix modelViewProj;
    IJK_GLES2_loadOrtho(&modelViewProj, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(renderer->um4_mvp, 1, GL_FALSE, modelViewProj.m);                    IJK_GLES2_checkError_TRACE("glUniformMatrix4fv(um4_mvp)");

    IJK_GLES2_Renderer_TexCoords_reset(renderer);
    IJK_GLES2_Renderer_TexCoords_reloadVertex(renderer);

    IJK_GLES2_Renderer_Vertices_reset(renderer);
    IJK_GLES2_Renderer_Vertices_reloadVertex(renderer);

    return GL_TRUE;
}

/*
 * Per-Frame routine
 */
GLboolean IJK_GLES2_Renderer_renderOverlay(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay)
{
    if (!renderer || !renderer->func_uploadTexture)
        return GL_FALSE;

    glClear(GL_COLOR_BUFFER_BIT);               IJK_GLES2_checkError_TRACE("glClear");

#ifdef __APPLE__
    if(renderer->panorama) {
        if (!renderer->func_uploadTexture(renderer, overlay))
            return GL_FALSE;
        int visibleSize = renderer->glass ? 2 : 1;
        float itemWidth = renderer->layer_width / visibleSize;
        float itemHeight = renderer->layer_height;
        for (int i = 0; i < visibleSize; i++) {
            glViewport(itemWidth * i, 0, itemWidth,itemHeight);
            glVertexAttribPointer(renderer->av4_position, 3, GL_FLOAT, 0, 0, renderer->vVertices);
            glEnableVertexAttribArray(renderer->av4_position);
            glVertexAttribPointer(renderer->av2_texcoord, 2, GL_FLOAT, 0, 0, renderer->vTextCoord);
            glEnableVertexAttribArray(renderer->av2_texcoord);
            glDrawElements(GL_TRIANGLE_STRIP, renderer->numIndices, GL_UNSIGNED_SHORT, renderer->indices);
        }
        return GL_TRUE;
    }
#endif
    GLsizei visible_width  = renderer->frame_width;
    GLsizei visible_height = renderer->frame_height;
    if (overlay) {
        visible_width  = overlay->w;
        visible_height = overlay->h;
        if (renderer->frame_width   != visible_width    ||
            renderer->frame_height  != visible_height   ||
            renderer->frame_sar_num != overlay->sar_num ||
            renderer->frame_sar_den != overlay->sar_den) {

            renderer->frame_width   = visible_width;
            renderer->frame_height  = visible_height;
            renderer->frame_sar_num = overlay->sar_num;
            renderer->frame_sar_den = overlay->sar_den;

            renderer->vertices_changed = 1;
        }
#ifdef __APPLE__
        renderer->videoCrop = overlay->crop;
#endif
        renderer->last_buffer_width = renderer->func_getBufferWidth(renderer, overlay);

        if (!renderer->func_uploadTexture(renderer, overlay))
            return GL_FALSE;
    } else {
        // NULL overlay means force reload vertice
        renderer->vertices_changed = 1;
    }
   // av_log(NULL, AV_LOG_ERROR, "[zhao] V [%f] [%f] [%f] [%f] [%f] [%f] [%f] [%f] \n",renderer->vertices[0],renderer->vertices[1],renderer->vertices[2],renderer->vertices[3],
   //        renderer->vertices[4],renderer->vertices[5],renderer->vertices[6],renderer->vertices[7]);
   // av_log(NULL, AV_LOG_ERROR, "[zhao] T [%f] [%f] [%f] [%f] [%f] [%f] [%f] [%f] \n",renderer->texcoords[0],renderer->texcoords[1],renderer->texcoords[2],renderer->texcoords[3],
    //       renderer->texcoords[4],renderer->texcoords[5],renderer->texcoords[6],renderer->texcoords[7]);
    GLsizei buffer_width = renderer->last_buffer_width;
    if (renderer->vertices_changed ||
        (buffer_width > 0 &&
         buffer_width > visible_width &&
         buffer_width != renderer->buffer_width &&
         visible_width != renderer->visible_width)){

        renderer->vertices_changed = 0;

        IJK_GLES2_Renderer_Vertices_apply(renderer);
        IJK_GLES2_Renderer_Vertices_reloadVertex(renderer);

        renderer->buffer_width  = buffer_width;
        renderer->visible_width = visible_width;
            
        IJK_GLES2_Renderer_TexCoords_reset(renderer);
#ifdef __APPLE__
            IJK_GLES2_Renderer_TexCoords_crop(renderer);
#else
            GLsizei padding_pixels     = buffer_width - visible_width;
            GLfloat padding_normalized = ((GLfloat)padding_pixels) / buffer_width;
            IJK_GLES2_Renderer_TexCoords_cropRight(renderer, padding_normalized);
#endif
        IJK_GLES2_Renderer_TexCoords_reloadVertex(renderer);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);      IJK_GLES2_checkError_TRACE("glDrawArrays");

    return GL_TRUE;
}
