#ifndef __CAM_GL_TEXTURE_H__
#define __CAM_GL_TEXTURE_H__

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl.h>
#endif

#include "pixels.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CamGLTexture CamGLTexture;

CamGLTexture *
cam_gl_texture_new (int width, int height, int buf_size);

void
cam_gl_texture_free (CamGLTexture * t);

void
cam_gl_texture_draw (CamGLTexture * t);

void
cam_gl_texture_draw_alpha (CamGLTexture * t, double alpha);

void
cam_gl_texture_draw_partial (CamGLTexture * t, double x, double y, 
        double w, double h);

int
cam_gl_texture_upload (CamGLTexture * t, CamPixelFormat pixelformat, int stride,
        const void * data);

/**
 * cam_gl_texture_set_interp:
 * @nearest_or_linear: typically GL_LINEAR or GL_NEAREST.  default is GL_LINEAR
 *
 * sets the interpolation mode when the texture is not drawn at a 1:1 scale.
 */
void
cam_gl_texture_set_interp (CamGLTexture * t, GLint nearest_or_linear);

#ifdef __cplusplus
}
#endif

#endif
