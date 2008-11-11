#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include <glib.h>
#include <glib-object.h>

#include <camunits/cam.h>
#include <camunits/gl_texture.h>
#include <camunits/plugin.h>

typedef struct _CampgrBumblebee2Gl CampgrBumblebee2Gl;
typedef struct _CampgrBumblebee2GlClass CampgrBumblebee2GlClass;

// boilerplate
#define CAMPGR_TYPE_BUMBLEBEE2_GL  campgr_bumblebee2_gl_get_type()
#define CAMPGR_BUMBLEBEE2_GL(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMPGR_TYPE_BUMBLEBEE2_GL, CampgrBumblebee2Gl))
#define CAMPGR_BUMBLEBEE2_GL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMPGR_TYPE_BUMBLEBEE2_GL, CampgrBumblebee2GlClass ))
#define IS_CAMPGR_BUMBLEBEE2_GL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMPGR_TYPE_BUMBLEBEE2_GL ))
#define IS_CAMPGR_BUMBLEBEE2_GL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMPGR_TYPE_BUMBLEBEE2_GL))
#define CAMPGR_BUMBLEBEE2_GL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMPGR_TYPE_BUMBLEBEE2_GL, CampgrBumblebee2GlClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);
struct _CampgrBumblebee2Gl {
    CamUnit parent;

    uint8_t * buf_1;
    uint8_t * buf_2;

    CamGLTexture * gl_texture_2;
    CamGLTexture * gl_texture;
    int gl_initialized;
    int texture_valid;
};

struct _CampgrBumblebee2GlClass {
    CamUnitClass parent_class;
};

GType campgr_bumblebee2_gl_get_type (void);

static CampgrBumblebee2Gl * campgr_bumblebee2_gl_new(void);
static int _draw_gl (CamUnit *super);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

CAM_PLUGIN_TYPE (CampgrBumblebee2Gl, campgr_bumblebee2_gl, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    campgr_bumblebee2_gl_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("output", 
            "bumblebee2-gl",
            "Bumblee2 GL", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)campgr_bumblebee2_gl_new,
            module);
}

// ============== CampgrBumblebee2Gl ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static int _draw_gl_init (CamUnit *super);
static int _draw_gl (CamUnit *super);
static int _draw_gl_shutdown (CamUnit *super);
static void on_status_changed(CamUnit *super, int old_status, gpointer nil);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

static void
campgr_bumblebee2_gl_class_init (CampgrBumblebee2GlClass *klass)
{
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.draw_gl_init = _draw_gl_init;
    klass->parent_class.draw_gl = _draw_gl;
    klass->parent_class.draw_gl_shutdown = _draw_gl_shutdown;
}

static void
campgr_bumblebee2_gl_init (CampgrBumblebee2Gl *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.

    self->gl_initialized = 0;
    self->texture_valid = 0;
    self->buf_1 = NULL;
    self->buf_2 = NULL;
    self->gl_texture = NULL;
    self->gl_texture_2 = NULL;

    g_signal_connect (G_OBJECT(self), "status-changed",
            G_CALLBACK(on_status_changed), NULL);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

CampgrBumblebee2Gl * 
campgr_bumblebee2_gl_new()
{
    return CAMPGR_BUMBLEBEE2_GL(g_object_new(CAMPGR_TYPE_BUMBLEBEE2_GL, NULL));
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CampgrBumblebee2Gl *self = CAMPGR_BUMBLEBEE2_GL(super);

    if (! self->gl_initialized) {
        _draw_gl_init (super);
    }

    if(infmt->pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16 && 
       !strstr(infmt->name, "(F7-0)")) {
        if (self->gl_texture && self->gl_texture_2) {
            for (int i=0; i<infmt->height; i++) {
                uint8_t * left_row = self->buf_1 + i * infmt->width;
                uint8_t * right_row = self->buf_2 + i * infmt->width;
                uint8_t * src_row = inbuf->data + i * infmt->row_stride;

                for (int j=0; j<infmt->width; j++) {
                    left_row[j] = src_row[j * 2];
                    right_row[j] = src_row[j * 2 + 1];
                }
            }
            cam_gl_texture_upload (self->gl_texture, CAM_PIXEL_FORMAT_GRAY, 
                    infmt->width, self->buf_1);
            cam_gl_texture_upload (self->gl_texture_2, CAM_PIXEL_FORMAT_GRAY, 
                    infmt->width, self->buf_2);
            self->texture_valid = 1;
        }
    } else if(infmt->pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16) {
        for (int i=0; i<infmt->height; i++) {
            uint16_t * dst_row = (uint16_t*)(self->buf_1 + i*infmt->row_stride);
            uint16_t * src_row = (uint16_t*)(inbuf->data + i*infmt->row_stride);
            for (int j=0; j<infmt->width; j++) {
                dst_row[j] = ntohs(src_row[j]);
            }
        }
        cam_gl_texture_upload (self->gl_texture, infmt->pixelformat, 
                infmt->row_stride, self->buf_1);
        self->texture_valid = 1;
    } else {
        if (self->gl_texture) {
            cam_gl_texture_upload (self->gl_texture, infmt->pixelformat, 
                    infmt->row_stride, inbuf->data);
            self->texture_valid = 1;
        }
    }

    cam_unit_produce_frame (super, inbuf, infmt);
}

static 
int _draw_gl_init (CamUnit *super)
{
    CampgrBumblebee2Gl *self = CAMPGR_BUMBLEBEE2_GL(super);
    if (! super->input_unit) {
        return -1;
    }
    const CamUnitFormat *infmt = 
        cam_unit_get_output_format(super->input_unit);

    if (! super->fmt) {
        return -1;
    }

    if (self->gl_initialized) {
        return 0;
    }

    if (! self->gl_texture) {
        if(infmt->pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16 &&
                !strstr(infmt->name, "(F7-0)")) {
            int buf_size = infmt->height * infmt->width;
            self->gl_texture = cam_gl_texture_new (infmt->width, 
                    infmt->height, buf_size);
            self->gl_texture_2 = cam_gl_texture_new(infmt->width,
                    infmt->height, buf_size);
            self->buf_1 = (uint8_t*) malloc(buf_size);
            self->buf_2 = (uint8_t*) malloc(buf_size);
        } else if(infmt->pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16) {
            int buf_size = infmt->max_data_size;
            self->buf_1 = (uint8_t*) malloc(buf_size);
            self->gl_texture = cam_gl_texture_new (infmt->width, 
                    infmt->height, infmt->height * infmt->row_stride);
        } else {
            self->gl_texture = cam_gl_texture_new (infmt->width, 
                    infmt->height, infmt->height * infmt->row_stride);
        }
    }
    if (!self->gl_texture) return -1;

    self->gl_initialized = 1;
    return 0;
}

static 
int _draw_gl (CamUnit *super)
{
    CampgrBumblebee2Gl *self = CAMPGR_BUMBLEBEE2_GL(super);
    if (! super->fmt) return -1;
    if (! self->gl_texture) return -1;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, super->fmt->width, super->fmt->height, 0, -1, 1);

    if (self->texture_valid) {
        if(super->fmt->pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16 && 
           !strstr(super->fmt->name, "(F7-0)")) {
            glPushMatrix();
            glTranslatef(0, super->fmt->height * 0.25, 0);
            glScalef(0.5, 0.5, 1);
            cam_gl_texture_draw (self->gl_texture);
            glTranslatef(super->fmt->width, 0, 0);
            cam_gl_texture_draw (self->gl_texture_2);
            glPopMatrix();
        } else {
            cam_gl_texture_draw (self->gl_texture);
        }
    } else {
        glColor3f (0, 0, 0);
        glBegin (GL_QUADS);
        glVertex2f (0, 0);
        glVertex2f (super->fmt->width, 0);
        glVertex2f (super->fmt->width, super->fmt->height);
        glVertex2f (0, super->fmt->height);
        glEnd ();
    }
    return 0;
}

static 
int _draw_gl_shutdown (CamUnit *super)
{
    CampgrBumblebee2Gl *self = CAMPGR_BUMBLEBEE2_GL(super);
    if (self->gl_texture) {
        cam_gl_texture_free (self->gl_texture);
        self->gl_texture = NULL;
    }
    if (self->gl_texture_2) {
        cam_gl_texture_free (self->gl_texture_2);
        self->gl_texture_2 = NULL;
    }
    free(self->buf_1);
    free(self->buf_2);
    self->buf_1 = NULL;
    self->buf_2 = NULL;
    self->gl_initialized = 0;
    return 0;
}

static void 
on_status_changed(CamUnit *super, int old_status, gpointer nil)
{
    CampgrBumblebee2Gl *self = CAMPGR_BUMBLEBEE2_GL(super);
    if (self->gl_initialized) {
        _draw_gl_shutdown(super);
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CampgrBumblebee2Gl *self = CAMPGR_BUMBLEBEE2_GL(super);
    cam_unit_remove_all_output_formats (super);
    self->texture_valid = 0;

    if (!infmt) return;

    if (! (infmt->pixelformat == CAM_PIXEL_FORMAT_BGR ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_RGB ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_RGBA ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_RGGB ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_BGGR ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_GBRG ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_GRBG ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BE_GRAY16 ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_FLOAT_GRAY32
           )) return;

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}
