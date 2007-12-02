#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "filter_gl.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

CamUnitDriver *
cam_filter_gl_driver_new()
{
    return cam_unit_driver_new_stock ("output", "opengl",
            "OpenGL", CAM_UNIT_RENDERS_GL,
            (CamUnitConstructor)cam_filter_gl_new);
}

// ============== CamFilterGL ===============
static void cam_filter_gl_finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static int cam_filter_gl_draw_gl_init (CamUnit *super);
static int cam_filter_gl_draw_gl (CamUnit *super);
static int cam_filter_gl_draw_gl_shutdown (CamUnit *super);
static void on_status_changed(CamUnit *super, int old_status, gpointer nil);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

G_DEFINE_TYPE (CamFilterGL, cam_filter_gl, CAM_TYPE_UNIT);

static void
cam_filter_gl_class_init (CamFilterGLClass *klass)
{
    dbg(DBG_OUTPUT, "FilterGL class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_filter_gl_finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.draw_gl_init = cam_filter_gl_draw_gl_init;
    klass->parent_class.draw_gl = cam_filter_gl_draw_gl;
    klass->parent_class.draw_gl_shutdown = cam_filter_gl_draw_gl_shutdown;
}

static void
cam_filter_gl_init (CamFilterGL *self)
{
    dbg(DBG_OUTPUT, "FilterGL constructor\n");

    // constructor.  Initialize the unit with some reasonable defaults here.
    self->gl_initialized = 0;
    self->texture_valid = 0;

    g_signal_connect (G_OBJECT(self), "status-changed",
            G_CALLBACK(on_status_changed), NULL);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

static void
cam_filter_gl_finalize (GObject *obj)
{
    dbg(DBG_OUTPUT, "FilterGL finalize\n");
    // destructor.  release heap/freestore memory here
    G_OBJECT_CLASS (cam_filter_gl_parent_class)->finalize(obj);
}

CamFilterGL * 
cam_filter_gl_new()
{
    return CAM_FILTER_GL(g_object_new(CAM_TYPE_FILTER_GL, NULL));
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamFilterGL *self = CAM_FILTER_GL(super);
    dbg(DBG_OUTPUT, "[%s] iterate\n", cam_unit_get_name(super));

    if (! self->gl_initialized) {
        cam_filter_gl_draw_gl_init (super);
    }

    if (self->gl_texture) {
        cam_gl_texture_upload (self->gl_texture, infmt->pixelformat, 
                infmt->row_stride, inbuf->data);
        self->texture_valid = 1;
    }

    cam_unit_produce_frame (super, inbuf, infmt);
}

static 
int cam_filter_gl_draw_gl_init (CamUnit *super)
{
    CamFilterGL *self = CAM_FILTER_GL(super);
    if (! super->input_unit) {
        dbg(DBG_OUTPUT, "FilterGL cannot init drawing - no input unit\n");
        return -1;
    }
    const CamUnitFormat *infmt = 
        cam_unit_get_output_format(super->input_unit);

    if (! super->fmt) {
        dbg(DBG_OUTPUT, "FilterGL cannot init drawing.  No format!\n");
        return -1;
    }

    if (self->gl_initialized) {
        dbg(DBG_OUTPUT, "FilterGL already initialized.\n");
        return 0;
    }

    dbg(DBG_OUTPUT, "FilterGL draw_gl_init\n");
    if (! self->gl_texture) {
        self->gl_texture = cam_gl_texture_new (infmt->width, 
                infmt->height, infmt->height * infmt->row_stride);
    }
    if (!self->gl_texture) return -1;

    self->gl_initialized = 1;
    return 0;
}

static 
int cam_filter_gl_draw_gl (CamUnit *super)
{
//    dbg(DBG_OUTPUT, "FilterGL Draw GL\n");
    CamFilterGL *self = CAM_FILTER_GL(super);
    if (! super->fmt) return -1;
    if (! self->gl_texture) return -1;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, super->fmt->width, super->fmt->height, 0, -1, 1);

    if (self->texture_valid) {
        cam_gl_texture_draw (self->gl_texture);
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
int cam_filter_gl_draw_gl_shutdown (CamUnit *super)
{
    CamFilterGL *self = CAM_FILTER_GL(super);
    if (self->gl_texture) {
        cam_gl_texture_free (self->gl_texture);
        self->gl_texture = NULL;
    }
    self->gl_initialized = 0;
    return 0;
}

static void 
on_status_changed(CamUnit *super, int old_status, gpointer nil)
{
    CamFilterGL *self = CAM_FILTER_GL(super);
    if (self->gl_initialized) {
        cam_filter_gl_draw_gl_shutdown(super);
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamFilterGL *self = CAM_FILTER_GL(super);
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
           infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)) return;

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}

