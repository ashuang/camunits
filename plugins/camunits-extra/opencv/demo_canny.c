#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <camunits/plugin.h>
#include <camunits/gl_texture.h>
#include <opencv/cv.h>

#define err(args...) fprintf(stderr, args)

typedef struct {
    CamUnit parent;

    CamGLTexture * gl_texture;
    int gl_initialized;
    int texture_valid;

    CamUnitControl *thresh1_ctl;
    CamUnitControl *thresh2_ctl;
    CamUnitControl *apert_ctl;
    CamUnitControl *render_original_ctl;
} CamcvCanny;

typedef struct {
    CamUnitClass parent_class;
} CamcvCannyClass;

static CamcvCanny * camcv_canny_new(void);
static int _gl_draw_gl_init (CamUnit *super);
static int _gl_draw_gl (CamUnit *super);
static int _gl_draw_gl_shutdown (CamUnit *super);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);

GType camcv_canny_get_type (void);
CAM_PLUGIN_TYPE (CamcvCanny, camcv_canny, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camcv_canny_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("opencv.demo", "canny",
            "Canny Edge Detector", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)camcv_canny_new,
            module);
}

static void
camcv_canny_init (CamcvCanny *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);
    self->thresh1_ctl = cam_unit_add_control_float (super, "thresh1", 
            "Threshold 1", 0, FLT_MAX, 1.0, 400, 1);
    self->thresh2_ctl = cam_unit_add_control_float (super, "thresh2", 
            "Threshold 2", 0, FLT_MAX, 1.0, 600, 1);
    self->apert_ctl = cam_unit_add_control_int (super, "aperture",
            "Aperture", 3, 7, 2, 5, 1);
    self->render_original_ctl = cam_unit_add_control_boolean(super, "render-original",
            "Render Original Image", 0, 1);
    cam_unit_control_set_ui_hints(self->thresh1_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->thresh2_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
camcv_canny_class_init (CamcvCannyClass *klass)
{
    // override superclass methods
    klass->parent_class.draw_gl_init = _gl_draw_gl_init;
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.draw_gl_shutdown = _gl_draw_gl_shutdown;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static CamcvCanny * 
camcv_canny_new()
{
    return (CamcvCanny*)(g_object_new(camcv_canny_get_type(), NULL));
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (! infmt || infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY) return;

    cam_unit_add_output_format (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamcvCanny *self = (CamcvCanny*)(super);

    CvSize img_size = { infmt->width, infmt->height };
    IplImage *cvimg = cvCreateImage (img_size, IPL_DEPTH_8U, 1);

    for (int r=0; r<infmt->height; r++) {
        memcpy (cvimg->imageData + r * cvimg->widthStep, 
                inbuf->data + r * infmt->row_stride,
                infmt->width);
    }

    IplImage *cvout = cvCreateImage (img_size, IPL_DEPTH_8U, 1);

    cvCanny (cvimg, cvout, 
             cam_unit_control_get_float (self->thresh1_ctl),
             cam_unit_control_get_float (self->thresh2_ctl),
             cam_unit_control_get_int (self->apert_ctl));

    if (self->gl_texture) {
        if(cam_unit_control_get_boolean(self->render_original_ctl)) {
            cam_gl_texture_upload(self->gl_texture, CAM_PIXEL_FORMAT_GRAY, 
                    infmt->row_stride, inbuf->data);
        } else {
            cam_gl_texture_upload(self->gl_texture, CAM_PIXEL_FORMAT_GRAY, 
                    cvout->widthStep, cvout->imageData);
        }
        self->texture_valid = 1;
    }

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (infmt->height * infmt->row_stride);
    for (int r=0; r<infmt->height; r++) {
        memcpy(outbuf->data + r * infmt->row_stride,
               cvout->imageData + r * cvout->widthStep, 
               infmt->width);
    }
    cam_framebuffer_copy_metadata(outbuf, inbuf);
    outbuf->bytesused = infmt->height * infmt->row_stride;
    cam_unit_produce_frame(super, outbuf, infmt);
    g_object_unref(outbuf);

    cvReleaseImage(&cvout);

    cvReleaseImage(&cvimg);
}

static int 
_gl_draw_gl_init (CamUnit *super)
{
    CamcvCanny *self = (CamcvCanny*) (super);
    CamUnit * input = cam_unit_get_input(super);
    if (! input) 
        return -1;
    const CamUnitFormat *infmt = cam_unit_get_output_format(input);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (! outfmt) 
        return -1;

    if (self->gl_initialized) 
        return 0;

    if (! self->gl_texture) {
        self->gl_texture = cam_gl_texture_new (infmt->width, 
                infmt->height, infmt->height * infmt->row_stride);
    }
    if (!self->gl_texture) return -1;

    self->gl_initialized = 1;
    return 0;
}

static int 
_gl_draw_gl (CamUnit *super)
{
    CamcvCanny *self = (CamcvCanny*) (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (! outfmt) return -1;
    if (! self->gl_texture) return -1;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, outfmt->width, outfmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
    if (self->texture_valid) {
        cam_gl_texture_draw (self->gl_texture);
    };
    return 0;
}

static int 
_gl_draw_gl_shutdown (CamUnit *super)
{
    CamcvCanny *self = (CamcvCanny*) (super);
    if (self->gl_texture) {
        cam_gl_texture_free (self->gl_texture);
        self->gl_texture = NULL;
    }
    self->gl_initialized = 0;
    return 0;
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamcvCanny *self = (CamcvCanny*) (super);
    if (ctl == self->apert_ctl) {
        int requested = g_value_get_int (proposed);
        if (requested < 3 || requested > 7 || (requested % 2 == 0)) {
            return FALSE;
        }
        g_value_copy (proposed, actual);
        return TRUE;
    } else {
        g_value_copy (proposed, actual);
        return TRUE;
    }
    return FALSE;
}
