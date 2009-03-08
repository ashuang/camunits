#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <GL/gl.h>

#include <camunits/plugin.h>
#include <opencv/cv.h>

#define err(args...) fprintf(stderr, args)

typedef struct {
    CamUnit parent;
    int max_ncorners;
    CvPoint2D32f *corners;
    int ncorners;

    CamUnitControl *quality_ctl;
    CamUnitControl *min_dist_ctl;
    CamUnitControl *block_size_ctl;
    CamUnitControl *use_harris_ctl;
    CamUnitControl *harris_k_ctl;
} CamcvGFTT;

typedef struct {
    CamUnitClass parent_class;
} CamcvGFTTClass;

static CamcvGFTT * camcv_gftt_new(void);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);
static int _gl_draw_gl (CamUnit *super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

GType camcv_gftt_get_type (void);
CAM_PLUGIN_TYPE (CamcvGFTT, camcv_gftt, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camcv_gftt_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("opencv.demo", 
            "good-features-to-track",
            "Good Features to Track", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)camcv_gftt_new,
            module);
}

static void
camcv_gftt_class_init (CamcvGFTTClass *klass)
{
    // class initializer.  setup the class vtable here.
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camcv_gftt_init (CamcvGFTT *self)
{
    // "private" constructor.  Initialize the unit with some reasonable
    // defaults here.
    CamUnit *super = CAM_UNIT (self);
    self->quality_ctl = cam_unit_add_control_float (super, "quality",
            "Quality", 0.0001, 1, 0.1, 0.3, 1);
    self->min_dist_ctl = cam_unit_add_control_float (super, "min-dist",
            "Min. Distance", 1, 100, 1, 5, 1);
    self->block_size_ctl = cam_unit_add_control_int (super, "block-size",
            "Block Size", 3, 15, 2, 3, 1);
    self->use_harris_ctl = cam_unit_add_control_boolean (super, "use-harris",
            "Use Harris", 1, 1);
    self->harris_k_ctl = cam_unit_add_control_float (super, "harris-k",
            "Harris K", 0, 0.5, 0.05, 0.04, 1);

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamcvGFTT * 
camcv_gftt_new()
{
    // "public" constructor
    return (CamcvGFTT*)(g_object_new(camcv_gftt_get_type(), NULL));
}

static int
_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamcvGFTT * self = (CamcvGFTT*) (super);
    self->max_ncorners = format->width * format->height / 4;
    self->corners = malloc (sizeof (CvPoint2D32f) * self->max_ncorners);
    self->ncorners = 0;
    return 0;
}

static int
_stream_shutdown (CamUnit * super)
{
    CamcvGFTT * self = (CamcvGFTT*) (super);
    free (self->corners);
    return 0;
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
    CamcvGFTT *self = (CamcvGFTT*)(super);

    CvSize img_size = { infmt->width, infmt->height };
    IplImage *cvimg = cvCreateImage (img_size, IPL_DEPTH_8U, 1);

    for (int r=0; r<infmt->height; r++) {
        memcpy (cvimg->imageData + r * cvimg->widthStep, 
                inbuf->data + r * infmt->row_stride,
                infmt->width);
    }

    IplImage *temp1 = cvCreateImage (img_size, IPL_DEPTH_32F, 1);
    IplImage *temp2 = cvCreateImage (img_size, IPL_DEPTH_32F, 1);

    double quality_level = cam_unit_control_get_float (self->quality_ctl);
    double min_distance = cam_unit_control_get_float (self->min_dist_ctl);
    int block_size = cam_unit_control_get_int (self->block_size_ctl);
    int use_harris = cam_unit_control_get_boolean (self->use_harris_ctl);
    double harris_k = cam_unit_control_get_float (self->harris_k_ctl);

    self->ncorners = self->max_ncorners;
    cvGoodFeaturesToTrack (cvimg, temp1, temp2, 
            self->corners, &self->ncorners,
            quality_level,
            min_distance,
            NULL, block_size,
            use_harris, harris_k);

    cam_unit_produce_frame (super, inbuf, infmt);

    cvReleaseImage (&temp1);
    cvReleaseImage (&temp2);

    cvReleaseImage (&cvimg);

    return;
}

static int 
_gl_draw_gl (CamUnit *super)
{
    CamcvGFTT *self = (CamcvGFTT*) (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (!outfmt) return 0;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, outfmt->width, outfmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    glPointSize (4.0);
    glColor3f (0, 1, 0);
    glBegin (GL_POINTS);
    for (int i=0; i<self->ncorners; i++) {
        glVertex2f (self->corners[i].x, self->corners[i].y);
    }
    glEnd ();
    return 0;
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamcvGFTT *self = (CamcvGFTT*) (super);
    if (ctl == self->block_size_ctl) {
        int requested = g_value_get_int (proposed);
        if (requested < 3 || (requested % 2 == 0)) {
            return FALSE;
        }
        g_value_copy (proposed, actual);
        return TRUE;
    }

    g_value_copy (proposed, actual);
    return TRUE;
}
