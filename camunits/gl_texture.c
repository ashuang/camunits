#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/ipc.h>
//#include <sys/shm.h>

#include <glib.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#include "pixels.h"
#include "gl_texture.h"

#define err(args...) fprintf(stderr, args)

static int
cam_gl_texture_upload_now (CamGLTexture * t, CamPixelFormat pixelformat, int stride,
        const void * data);

static void
cam_gl_texture_upload_check(CamGLTexture *t);

struct _CamGLTexture {
    int width, height;
    GLenum target;
    GLint int_format;
    GLuint texname;
    GLint interp_mode;

    GLuint texc_width;
    GLuint texc_height;

    GLuint pbo;
    int use_pbo;
    int buf_size;

    CamPixelFormat buf_pixelformat;
    int buf_stride;
    void * upload_buffer;
    int buf_uploaded;
};
    
CamGLTexture *
cam_gl_texture_new (int width, int height, int buf_size)
{
    CamGLTexture * t = calloc(1, sizeof(CamGLTexture));

    int has_non_power_of_two = 0;
    int has_texture_rectangle = 0;
    int has_pbo = 0;

    t->upload_buffer = malloc(buf_size);
    t->buf_uploaded = 1;

    const char * extstr = (const char *) glGetString (GL_EXTENSIONS);
    if (extstr) {
        gchar ** exts = g_strsplit (extstr, " ", 0);
        int i;
        for (i = 0; exts[i]; i++) {
            gchar * ext = exts[i];
            if (!strcmp (ext, "GL_ARB_texture_non_power_of_two"))
                has_non_power_of_two = 1;
            if (!strcmp (ext, "GL_ARB_texture_rectangle"))
                has_texture_rectangle = 1;
            if (!strcmp (ext, "GL_ARB_pixel_buffer_object"))
                has_pbo = 1;
        }
        g_strfreev (exts);
    }

//    printf ("%s:%d - %d %d %d\n", __FILE__, __LINE__,
//            has_non_power_of_two, has_texture_rectangle,
//            has_pbo
//            );

    t->use_pbo = has_pbo;
    t->int_format = GL_RGBA8;
    t->interp_mode = GL_LINEAR;
    t->width = width;
    t->height = height;

    if (has_non_power_of_two) {
        t->target = GL_TEXTURE_2D;
        t->texc_width = 1;
        t->texc_height = 1;
    }
    else if (has_texture_rectangle) {
        t->target = GL_TEXTURE_RECTANGLE_ARB;
        t->texc_width = width;
        t->texc_height = height;
    }
    else {
        fprintf (stderr, "%s:%d -- GL supports neither non-power-of-two nor "
                "texture-rectangle\n", __FILE__, __LINE__);
        free (t);
        return NULL;
    }

    glGenTextures (1, &t->texname);
#if 0
    glBindTexture (t->target, t->texname);

#endif

    if (t->use_pbo) {
        glGenBuffersARB (1, &t->pbo);
        t->buf_size = buf_size;
        glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, t->pbo);
        glBufferDataARB (GL_PIXEL_UNPACK_BUFFER_ARB, 
                t->buf_size, NULL, 
                GL_STREAM_DRAW);
        glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    }

    return t;
}

void
cam_gl_texture_free (CamGLTexture * t)
{
    glDeleteTextures (1, &t->texname);
    if (t->pbo) {
        glDeleteBuffersARB (1, &t->pbo);
    }
    free(t->upload_buffer);
    free (t);
}

void
cam_gl_texture_draw_alpha (CamGLTexture * t, double alpha)
{
    cam_gl_texture_upload_check(t);
    glPushAttrib (GL_ENABLE_BIT);
    glEnable (t->target);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture (t->target, t->texname);
    glBegin (GL_QUADS);
    glColor4f (1.0, 1.0, 1.0, alpha);
    glTexCoord2i (0,0);
    glVertex2i (0,0);
    glTexCoord2i (0,t->texc_height);
    glVertex2i (0,t->height);
    glTexCoord2i (t->texc_width, t->texc_height);
    glVertex2i (t->width,t->height);
    glTexCoord2i (t->texc_width,0);
    glVertex2i (t->width,0);
    glEnd ();
    glBindTexture (t->target, 0);
    glPopAttrib ();
}

void
cam_gl_texture_draw (CamGLTexture * t)
{
    cam_gl_texture_upload_check(t);
    glEnable (t->target);
    glBindTexture (t->target, t->texname);
    glBegin (GL_QUADS);
    glColor3f (1.0, 1.0, 1.0);
    glTexCoord2i (0,0);
    glVertex2i (0,0);
    glTexCoord2i (0,t->texc_height);
    glVertex2i (0,t->height);
    glTexCoord2i (t->texc_width,t->texc_height);
    glVertex2i (t->width,t->height);
    glTexCoord2i (t->texc_width,0);
    glVertex2i (t->width,0);
    glEnd ();
    glBindTexture (t->target, 0);
    glDisable (t->target);
}

void
cam_gl_texture_draw_partial (CamGLTexture * t, double x, double y, double w, double h)
{
    cam_gl_texture_upload_check(t);
    glEnable (t->target);
    glBindTexture (t->target, t->texname);
    glBegin (GL_QUADS);
    glTexCoord2d (x * t->texc_width / t->width,
	    y * t->texc_height / t->height);
    glVertex2d (x, y);
    glTexCoord2d (x * t->texc_width / t->width,
	    (y+h) * t->texc_height / t->height);
    glVertex2d (x, y+h);
    glTexCoord2d ((x+w) * t->texc_width / t->width,
	    (y+h) * t->texc_height / t->height);
    glVertex2d (x+w, y+h);
    glTexCoord2d ((x+w) * t->texc_width / t->width,
	    y * t->texc_height / t->height);
    glVertex2d (x+w, y);
    glEnd ();
    glBindTexture (t->target, 0);
    glDisable (t->target);
}

int
cam_gl_texture_upload (CamGLTexture * t, CamPixelFormat pixelformat, int stride,
        const void * data)
{
    t->buf_pixelformat = pixelformat;
    t->buf_stride = stride;
    memcpy(t->upload_buffer, data, t->buf_size);
    t->buf_uploaded = 0;
    return 0;
}

static void
cam_gl_texture_upload_check(CamGLTexture *t)
{
    if (!t->buf_uploaded) {
        cam_gl_texture_upload_now(t, t->buf_pixelformat, t->buf_stride, t->upload_buffer);
        t->buf_uploaded = 1;
    }
}

static int
cam_gl_texture_upload_now (CamGLTexture * t, CamPixelFormat pixelformat, int stride,
        const void * data)
{
    if (t->use_pbo && (stride * t->height) > t->buf_size) {
        fprintf (stderr, "Error: gl_texture buffer (%d bytes) too small for "
                "texture (%d bytes)\n", t->buf_size, 
                stride * t->height);
        return -1;
    }
    if (!data && !t->use_pbo) {
        fprintf (stderr, "Error: gl_texture data is NULL\n");
        return -1;
    }
    int swap_bytes = 0;
    GLenum format;
    GLenum type = GL_UNSIGNED_BYTE;
    if (pixelformat == CAM_PIXEL_FORMAT_GRAY ||
            pixelformat == CAM_PIXEL_FORMAT_BAYER_BGGR ||
            pixelformat == CAM_PIXEL_FORMAT_BAYER_GBRG ||
            pixelformat == CAM_PIXEL_FORMAT_BAYER_GRBG ||
            pixelformat == CAM_PIXEL_FORMAT_BAYER_RGGB) {
        format = GL_LUMINANCE;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_RGB) {
        format = GL_RGB;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_BGR) {
        format = GL_BGR;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_RGBA) {
        format = GL_RGBA;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_BGRA) {
        format = GL_BGRA;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_BE_SIGNED_GRAY16) {
        format = GL_LUMINANCE;
        type = GL_SHORT;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16) {
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_SHORT;

        if(G_BYTE_ORDER != G_BIG_ENDIAN)
            swap_bytes = 1;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_LE_GRAY16) {
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_SHORT;

        if(G_BYTE_ORDER != G_LITTLE_ENDIAN)
            swap_bytes = 1;
    }
    else if (pixelformat == CAM_PIXEL_FORMAT_FLOAT_GRAY32) {
        format = GL_LUMINANCE;
        type = GL_FLOAT;
    }
    else {
        fprintf (stderr, "Error: gl_texture does not support pixel format %s\n",
                cam_pixel_format_nickname (pixelformat));
        return -1;
    }

    glBindTexture (t->target, t->texname);

    glTexParameterf (t->target, GL_TEXTURE_MAG_FILTER, t->interp_mode);
    glTexParameterf (t->target, GL_TEXTURE_MIN_FILTER, t->interp_mode);

    if (stride % 2) {
        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    } else if (stride % 4) {
        glPixelStorei (GL_UNPACK_ALIGNMENT, 2);
    } else {
        glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
    }
    if(swap_bytes) {
        glPixelStorei (GL_UNPACK_SWAP_BYTES, GL_TRUE);
    }

    glPixelStorei (GL_UNPACK_ROW_LENGTH, 
            stride * 8 / cam_pixel_format_bpp (pixelformat));
    if (t->use_pbo) {
        glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, t->pbo);

        /* By setting data to NULL, we skip the memcpy and just re-upload
         * from the buffer object.  This can be useful to re-upload with
         * different PixelTransfer settings. */
        if (data) {
            uint8_t *buffer_data = 
                (uint8_t*) glMapBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB,
                        GL_WRITE_ONLY);
            memcpy (buffer_data, data, stride * t->height);
            glUnmapBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB);
        }

        glTexImage2D (t->target, 0, t->int_format, t->width, t->height, 0,
                format, type, 0);

        glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    } else {
        glTexImage2D (t->target, 0, t->int_format, t->width, t->height, 0,
                format, type, data);
    }
    if(swap_bytes) {
        glPixelStorei (GL_UNPACK_SWAP_BYTES, GL_FALSE);
    }
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture (t->target, 0);
    return 0;
}

void
cam_gl_texture_set_interp (CamGLTexture * t, GLint nearest_or_linear)
{
    t->interp_mode = nearest_or_linear;
}
