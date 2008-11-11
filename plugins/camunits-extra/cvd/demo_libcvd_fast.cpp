#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include <GL/gl.h>

#include <camunits/cam.h>
#include <camunits/plugin.h>

#include <cvd/fast_corner.h>
#include <cvd/config.h>

#define err(args...) fprintf(stderr, args)

extern "C" {

typedef struct _CamcvdFAST CamcvdFAST;
typedef struct _CamcvdFASTClass CamcvdFASTClass;

// boilerplate
#define CAMCVD_TYPE_FAST  camcvd_fast_get_type()
#define CAMCVD_FAST(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMCVD_TYPE_FAST, CamcvdFAST))
#define CAMCVD_FAST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMCVD_TYPE_FAST, CamcvdFASTClass ))
#define IS_CAMCVD_FAST(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMCVD_TYPE_FAST ))
#define IS_CAMCVD_FAST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMCVD_TYPE_FAST))
#define CAMCVD_FAST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMCVD_TYPE_FAST, CamcvdFASTClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);
struct _CamcvdFAST {
    CamUnit parent;

    CVD::Image< CVD::byte > *packed_img;
    std::vector< CVD::ImageRef > corners;

    CamUnitControl *points_ctl;
    CamUnitControl *thresh_ctl;
    CamUnitControl *suppress_nonmax_ctl;
};

struct _CamcvdFASTClass {
    CamUnitClass parent_class;
};

GType camcvd_fast_get_type (void);

static CamcvdFAST * camcvd_fast_new(void);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);
static int _gl_draw_gl (CamUnit *super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

CAM_PLUGIN_TYPE (CamcvdFAST, camcvd_fast, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    camcvd_fast_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("demo", 
            "cvd-fast",
            "FAST Features (Rosten, Drummond)", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)camcvd_fast_new,
            module);
}

static void
camcvd_fast_class_init (CamcvdFASTClass *klass)
{
    // class initializer.  setup the class vtable here.
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camcvd_fast_init (CamcvdFAST *self)
{
    // "private" constructor.  Initialize the unit with some reasonable
    // defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->points_ctl = cam_unit_add_control_int (super, "points",
            "Points", 7, 12, 1, 10, 1);
    self->thresh_ctl = cam_unit_add_control_int (super, "threshold",
            "Threshold", 1, 255, 1, 20, 1);
    self->suppress_nonmax_ctl = cam_unit_add_control_boolean (super, 
            "nonmax-suppress", "Non-max Suppression", 1, 1);

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamcvdFAST * 
camcvd_fast_new()
{
    // "public" constructor
    return CAMCVD_FAST(g_object_new(CAMCVD_TYPE_FAST, NULL));
}

static int
_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamcvdFAST * self = CAMCVD_FAST (super);

    CVD::ImageRef imsz(format->width, format->height);
    self->packed_img = new CVD::Image< CVD::byte >(imsz);
    //self->fl = KLTCreateFeatureList (100);

    return 0;
}

static int
_stream_shutdown (CamUnit * super)
{
    CamcvdFAST * self = CAMCVD_FAST (super);
    delete self->packed_img;
    self->packed_img = NULL;
    return 0;
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
    CamcvdFAST *self = CAMCVD_FAST(super);

    // make a copy of the image data, both to remove pad bytes between rows,
    // and to keep a copy around for future frame processing
    CVD::byte *imdata = self->packed_img->data();

    cam_pixel_copy_8u_generic (inbuf->data, infmt->row_stride,
            imdata, self->packed_img->row_stride(),
            0, 0, 0, 0, 
            infmt->width, infmt->height, 8);

    int threshold = cam_unit_control_get_int (self->thresh_ctl);
    self->corners.clear();

    std::vector< CVD::ImageRef > *work_corners = &self->corners;
    std::vector< CVD::ImageRef > tmp_corners;
    if (cam_unit_control_get_boolean (self->suppress_nonmax_ctl)) {
        work_corners = &tmp_corners;
    }

    switch (cam_unit_control_get_int (self->points_ctl)) {
        case 7:
#ifdef CVD_HAVE_FAST_7
            CVD::fast_corner_detect_7(*self->packed_img, *work_corners, 
                    threshold);
#endif
            break;
        case 8:
#ifdef CVD_HAVE_FAST_8
            CVD::fast_corner_detect_8(*self->packed_img, *work_corners, 
                    threshold);
#endif
            break;
        case 9:
            CVD::fast_corner_detect_9(*self->packed_img, *work_corners, 
                    threshold);
            break;
        case 10:
            CVD::fast_corner_detect_10(*self->packed_img, *work_corners, 
                    threshold);
            break;
#ifdef CVD_HAVE_FAST_11
        case 11:
            CVD::fast_corner_detect_11(*self->packed_img, *work_corners, 
                    threshold);
#endif
            break;
        case 12:
        default:
            CVD::fast_corner_detect_12(*self->packed_img, *work_corners, 
                    threshold);
            break;
    }

    if (cam_unit_control_get_boolean (self->suppress_nonmax_ctl)) {
        CVD::fast_nonmax (*self->packed_img, *work_corners, threshold,
                self->corners);
    }

    cam_unit_produce_frame (super, inbuf, infmt);
}

static int 
_gl_draw_gl (CamUnit *super)
{
    CamcvdFAST *self = CAMCVD_FAST (super);
    if (!super->fmt) return 0;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, super->fmt->width, super->fmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    glPointSize (4.0);
    glColor3f (0, 1, 0);

    glBegin (GL_POINTS);
    for (unsigned int i=0; i < self->corners.size(); i++) {
        glVertex2f (self->corners[i].x, self->corners[i].y);
    }
    glEnd ();
    return 0;
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamcvdFAST *self = CAMCVD_FAST (super);
    if (ctl == self->points_ctl) {
        int n = g_value_get_int (proposed);
        
        int ok = ( 
#ifdef CVD_HAVE_FAST_7
                    (n == 7) ||
#endif
#ifdef CVD_HAVE_FAST_8
                   (n == 8) ||
#endif
                   (n == 9) ||
                   (n == 10) ||
#ifdef CVD_HAVE_FAST_11
                   (n == 11) ||
#endif
                   (n == 12));
        if (!ok)
            return FALSE;
    }
    g_value_copy (proposed, actual);
    return TRUE;
}

}
