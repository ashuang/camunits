#ifndef GL_TEXTURE_H
#define GL_TEXTURE_H

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

typedef struct _GLTexture GLTexture;

GLTexture *
gl_texture_new (int width, int height, int max_data_size);

void
gl_texture_free (GLTexture * t);

void
gl_texture_draw (GLTexture * t);

void
gl_texture_draw_alpha (GLTexture * t, double alpha);

void
gl_texture_draw_partial (GLTexture * t, double x, double y, double w, double h);

int
gl_texture_upload (GLTexture * t, CamPixelFormat pixelformat, int stride,
        void * data);

/**
 * gl_texture_set_interp:
 * @nearest_or_linear: typically GL_LINEAR or GL_NEAREST.  default is GL_LINEAR
 *
 * sets the interpolation mode when the texture is not drawn at a 1:1 scale.
 */
void
gl_texture_set_interp (GLTexture * t, GLint nearest_or_linear);

#ifdef __cplusplus
}
#endif

#endif
