#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <libcam/plugin.h>
#include <libcam/gl_texture.h>
#include <opencv/cv.h>

#define err(args...) fprintf(stderr, args)

typedef struct _CamcvCanny CamcvCanny;
typedef struct _CamcvCannyClass CamcvCannyClass;

// boilerplate
#define CAMCV_TYPE_CANNY  camcv_canny_get_type()
#define CAMCV_CANNY(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMCV_TYPE_CANNY, CamcvCanny))
#define CAMCV_CANNY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMCV_TYPE_CANNY, CamcvCannyClass ))
#define IS_CAMCV_CANNY(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMCV_TYPE_CANNY ))
#define IS_CAMCV_CANNY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMCV_TYPE_CANNY))
#define CAMCV_CANNY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMCV_TYPE_CANNY, CamcvCannyClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

struct _CamcvCanny {
    CamUnit parent;

    CamGLTexture * gl_texture;
    int gl_initialized;
    int texture_valid;

    CamUnitControl *thresh1_ctl;
    CamUnitControl *thresh2_ctl;
    CamUnitControl *apert_ctl;
};

struct _CamcvCannyClass {
    CamUnitClass parent_class;
};

GType camcv_canny_get_type (void);

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

CAM_PLUGIN_TYPE (CamcvCanny, camcv_canny, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    camcv_canny_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("demo.opencv", "canny",
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
            "Threshold 1", 0, 10, 0.1, 0.1, 1);
    self->thresh2_ctl = cam_unit_add_control_float (super, "thresh2", 
            "Threshold 2", 0, 10, 0.1, 1.0, 1);
    self->apert_ctl = cam_unit_add_control_int (super, "aperture",
            "Aperture", 3, 7, 2, 5, 1);
    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
camcv_canny_class_init (CamcvCannyClass *klass)
{
//    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
//    gobject_class->finalize = camlcm_publish_finalize;
    klass->parent_class.draw_gl_init = _gl_draw_gl_init;
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.draw_gl_shutdown = _gl_draw_gl_shutdown;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static CamcvCanny * 
camcv_canny_new()
{
    return CAMCV_CANNY(g_object_new(CAMCV_TYPE_CANNY, NULL));
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (! infmt || infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY) return;

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamcvCanny *self = CAMCV_CANNY(super);

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
        cam_gl_texture_upload (self->gl_texture, CAM_PIXEL_FORMAT_GRAY, 
                cvout->widthStep, cvout->imageData);
        self->texture_valid = 1;
    }

#if 1
    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (infmt->height * infmt->row_stride);
    for (int r=0; r<infmt->height; r++) {
        memcpy (outbuf->data + r * infmt->row_stride,
                cvout->imageData + r * cvout->widthStep, 
                infmt->width);
    }
    cam_unit_produce_frame (super, outbuf, infmt);
    g_object_unref (outbuf);

#else
    cam_unit_produce_frame (super, inbuf, infmt);
#endif

    cvReleaseImage (&cvout);

    cvReleaseImage (&cvimg);

    return;
}

static int 
_gl_draw_gl_init (CamUnit *super)
{
    CamcvCanny *self = CAMCV_CANNY (super);
    if (! super->input_unit) 
        return -1;
    const CamUnitFormat *infmt = cam_unit_get_output_format(super->input_unit);
    if (! super->fmt) 
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
    CamcvCanny *self = CAMCV_CANNY (super);
    if (! super->fmt) return -1;
    if (! self->gl_texture) return -1;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, super->fmt->width, super->fmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
    if (self->texture_valid) {
        cam_gl_texture_draw (self->gl_texture);
    };
    return 0;
}

static int 
_gl_draw_gl_shutdown (CamUnit *super)
{
    CamcvCanny *self = CAMCV_CANNY (super);
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
    CamcvCanny *self = CAMCV_CANNY (super);
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
