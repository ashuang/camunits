#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <libcam/plugin.h>
#include <opencv/cv.h>

#include "demo_harris.h"

#define err(args...) fprintf(stderr, args)

struct _CamcvHarris {
    CamUnit parent;
};

struct _CamcvHarrisClass {
    CamUnitClass parent_class;
};

GType camcv_harris_get_type (void);

static CamcvHarris * camcv_harris_new(void);
static int _gl_draw_gl_init (CamUnit *super);
static int _gl_draw_gl (CamUnit *super);
static int _gl_draw_gl_shutdown (CamUnit *super);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

CAM_PLUGIN_TYPE (CamcvHarris, camcv_harris, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    camcv_harris_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("demo.opencv", "harris",
            "Harris Corners", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)camcv_harris_new,
            module);
}

static void
camcv_harris_init (CamcvHarris *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
camcv_harris_class_init (CamcvHarrisClass *klass)
{
//    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
//    gobject_class->finalize = camlcm_publish_finalize;
    klass->parent_class.draw_gl_init = _gl_draw_gl_init;
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.draw_gl_shutdown = _gl_draw_gl_shutdown;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

static CamcvHarris * 
camcv_harris_new()
{
    return CAMCV_HARRIS(g_object_new(CAMCV_TYPE_HARRIS, NULL));
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
//    CamcvHarris *self = CAMCV_HARRIS(super);

    CvSize img_size = { infmt->width, infmt->height };
    IplImage *cvimg = cvCreateImage (img_size, IPL_DEPTH_8U, 1);

    for (int r=0; r<infmt->height; r++) {
        memcpy (cvimg->imageData + r * cvimg->widthStep, 
                inbuf->data + r * infmt->row_stride,
                infmt->width);
    }

    IplImage *cvout = cvCreateImage (img_size, IPL_DEPTH_8U, 1);

//    cvCornerHarris (cvimg, cvout, 3, 3, 0.04);
//    cvCanny(cvimg,cvout,0.1,1.0,5); 

#if 0
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
    // TODO
    return 0;
}

static int 
_gl_draw_gl (CamUnit *super)
{
    // TODO
    return 0;
}

static int 
_gl_draw_gl_shutdown (CamUnit *super)
{
    // TODO
    return 0;
}
