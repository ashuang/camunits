#include <GL/gl.h>
#include <GL/glext.h>

#include <camunits/plugin.h>
#include <camunits/dbg.h>
#include <camunits/gl_texture.h>

typedef struct _CamFilterGL {
    CamUnit parent;

    CamGLTexture * gl_texture;
    int gl_initialized;
    int texture_valid;
} CamFilterGL;

typedef struct _CamFilterGLClass {
    CamUnitClass parent_class;
} CamFilterGLClass;

static CamFilterGL * cam_filter_gl_new (void);

GType cam_filter_gl_get_type (void);
CAM_PLUGIN_TYPE(CamFilterGL, cam_filter_gl, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module);
void cam_plugin_initialize(GTypeModule * module)
{
    cam_filter_gl_register_type(module);
}

CamUnitDriver * cam_plugin_create(GTypeModule * module);
CamUnitDriver * cam_plugin_create(GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("output", "opengl",
            "OpenGL", CAM_UNIT_RENDERS_GL,
            (CamUnitConstructor)cam_filter_gl_new, module);
}

// ============== CamFilterGL ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static int cam_filter_gl_draw_gl_init (CamUnit *super);
static int cam_filter_gl_draw_gl (CamUnit *super);
static int cam_filter_gl_draw_gl_shutdown (CamUnit *super);
static void on_status_changed(CamUnit *super, int old_status, gpointer nil);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

static void
cam_filter_gl_class_init (CamFilterGLClass *klass)
{
    dbg(DBG_OUTPUT, "FilterGL class initializer\n");
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.draw_gl_init = cam_filter_gl_draw_gl_init;
    klass->parent_class.draw_gl = cam_filter_gl_draw_gl;
    klass->parent_class.draw_gl_shutdown = cam_filter_gl_draw_gl_shutdown;
}

static void
cam_filter_gl_init (CamFilterGL *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    dbg(DBG_OUTPUT, "FilterGL constructor\n");

    self->gl_initialized = 0;
    self->texture_valid = 0;

    g_signal_connect (G_OBJECT(self), "status-changed",
            G_CALLBACK(on_status_changed), NULL);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

CamFilterGL * 
cam_filter_gl_new()
{
    return (CamFilterGL*)(g_object_new(cam_filter_gl_get_type(), NULL));
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamFilterGL *self = (CamFilterGL*)super;
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
    CamFilterGL *self = (CamFilterGL*)super;
    CamUnit *input = cam_unit_get_input(super);
    if (! input) {
        dbg(DBG_OUTPUT, "FilterGL cannot init drawing - no input unit\n");
        return -1;
    }
    const CamUnitFormat *infmt = cam_unit_get_output_format(input);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (! outfmt) {
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
    CamFilterGL *self = (CamFilterGL*)super;
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (! outfmt) return -1;
    if (! self->gl_texture) return -1;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, outfmt->width, outfmt->height, 0, -1, 1);

    if (self->texture_valid) {
        cam_gl_texture_draw (self->gl_texture);
    } else {
        glColor3f (0, 0, 0);
        glBegin (GL_QUADS);
        glVertex2f (0, 0);
        glVertex2f (outfmt->width, 0);
        glVertex2f (outfmt->width, outfmt->height);
        glVertex2f (0, outfmt->height);
        glEnd ();
    }
    return 0;
}

static 
int cam_filter_gl_draw_gl_shutdown (CamUnit *super)
{
    CamFilterGL *self = (CamFilterGL*)super;
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
    CamFilterGL *self = (CamFilterGL*)super;
    if (self->gl_initialized) {
        cam_filter_gl_draw_gl_shutdown(super);
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamFilterGL *self = (CamFilterGL*)super;
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
           infmt->pixelformat == CAM_PIXEL_FORMAT_LE_GRAY16 ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_FLOAT_GRAY32
           )) return;

    cam_unit_add_output_format (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride);
}
