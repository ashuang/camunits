#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <camunits/plugin.h>
#include <opencv/cv.h>

typedef struct _CamcvUndistort CamcvUndistort;
struct _CamcvUndistort {
    CamUnit parent;

    CamUnitControl *enabled_ctl;
    CamUnitControl *cop_x_ctl;
    CamUnitControl *cop_y_ctl;
    CamUnitControl *flen_x_ctl;
    CamUnitControl *flen_y_ctl;
    CamUnitControl *k_1_ctl;
    CamUnitControl *k_2_ctl;
    CamUnitControl *k_3_ctl;
    CamUnitControl *p_1_ctl;
    CamUnitControl *p_2_ctl;

    CamFrameBuffer * outbuf;
    IplImage * dst_cv;
    CvMat * mapx;
    CvMat * mapy;
};

typedef struct _CamcvUndistortClass CamcvUndistortClass;
struct _CamcvUndistortClass {
    CamUnitClass parent_class;
};

static CamcvUndistort * camcv_undistort_new(void);
static void on_input_frame_ready(CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed(CamUnit *super, 
        const CamUnitFormat *infmt);
static int _stream_init(CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown(CamUnit * super);
static gboolean _try_set_control(CamUnit *super, const CamUnitControl *ctl,
        const GValue *proposed, GValue *actual);
static void _update_mapping(CamcvUndistort * self);
static void _finalize (GObject * obj);

GType camcv_undistort_get_type(void);
CAM_PLUGIN_TYPE(CamcvUndistort, camcv_undistort, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module);
void cam_plugin_initialize(GTypeModule * module)
{
    camcv_undistort_register_type(module);
}

CamUnitDriver * cam_plugin_create(GTypeModule * module);
CamUnitDriver * cam_plugin_create(GTypeModule * module)
{
    return cam_unit_driver_new_stock_full("opencv", "undistort",
            "Undistort", 0, 
            (CamUnitConstructor)camcv_undistort_new, module);
}

static void
camcv_undistort_class_init(CamcvUndistortClass *klass)
{
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
    klass->parent_class.try_set_control = _try_set_control;

    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = _finalize;
}

static void
_finalize (GObject * obj)
{
    CamcvUndistort * self = (CamcvUndistort*)(obj);
    if(self->dst_cv) {
        cvReleaseImage(&self->dst_cv);
       g_object_unref(self->outbuf);
    }
    G_OBJECT_CLASS (camcv_undistort_parent_class)->finalize(obj);
}

static void
camcv_undistort_init(CamcvUndistort *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT(self);

    self->enabled_ctl = cam_unit_add_control_boolean(super, "enabled", "Enabled", 1, 1);
    self->cop_x_ctl = cam_unit_add_control_float(super, "cop_x", "Center of Proj. (x)", FLT_MIN, FLT_MAX, 1, 300, 1);
    self->cop_y_ctl = cam_unit_add_control_float(super, "cop_y", "Center of Proj. (y)", FLT_MIN, FLT_MAX, 1, 300, 1);

    self->flen_x_ctl = cam_unit_add_control_float(super, "flen_x", "Focal Len. (x)", 0, FLT_MAX, 1, 300, 1);
    self->flen_y_ctl = cam_unit_add_control_float(super, "flen_y", "Focal Len. (y)", 0, FLT_MAX, 1, 300, 1);

    self->k_1_ctl = cam_unit_add_control_float(super, "k_1", "K 1", -FLT_MAX, FLT_MAX, 0.01, 0, 1);
    self->k_2_ctl = cam_unit_add_control_float(super, "k_2", "K 2", -FLT_MAX, FLT_MAX, 0.01, 0, 1);
    self->k_3_ctl = cam_unit_add_control_float(super, "k_3", "K 3", -FLT_MAX, FLT_MAX, 0.01, 0, 1);
    self->p_1_ctl = cam_unit_add_control_float(super, "p_1", "P 1", -FLT_MAX, FLT_MAX, 0.01, 0, 1);
    self->p_2_ctl = cam_unit_add_control_float(super, "p_2", "P 2", -FLT_MAX, FLT_MAX, 0.01, 0, 1);

    cam_unit_control_set_ui_hints(self->cop_x_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->cop_y_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->flen_x_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->flen_y_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->k_1_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->k_2_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->k_3_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->p_1_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->p_2_ctl, CAM_UNIT_CONTROL_SPINBUTTON);

    self->mapx = NULL;
    self->mapy = NULL;
    self->dst_cv = NULL;
    self->outbuf = NULL;

    g_signal_connect(G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), self);
}

static CamcvUndistort * 
camcv_undistort_new()
{
    return (CamcvUndistort*)(g_object_new(camcv_undistort_get_type(), NULL));
}

static int 
_stream_init(CamUnit * super, const CamUnitFormat * fmt)
{
    CamcvUndistort *self = (CamcvUndistort*)(super);

    self->mapx = cvCreateMat(fmt->height, fmt->width, CV_32F);
    self->mapy = cvCreateMat(fmt->height, fmt->width, CV_32F);

    _update_mapping(self);

    return 0;
}

static int 
_stream_shutdown(CamUnit * super)
{
    CamcvUndistort *self = (CamcvUndistort*)(super);

    cvReleaseMat(&self->mapx);
    cvReleaseMat(&self->mapy);

    self->mapx = NULL;
    self->mapy = NULL;

    return 0;
}

static void 
on_input_frame_ready(CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamcvUndistort *self = (CamcvUndistort*)(super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if(!cam_unit_control_get_boolean(self->enabled_ctl)) {
        cam_unit_produce_frame(super, inbuf, infmt);
        return;
    }

    // TODO don't malloc
    CvSize img_size = { infmt->width, infmt->height };
    IplImage *src_cv;
    if(infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        src_cv = cvCreateImage(img_size, IPL_DEPTH_8U, 1);
    
    if(infmt->pixelformat == CAM_PIXEL_FORMAT_RGB)
        src_cv = cvCreateImage(img_size, IPL_DEPTH_8U, 3);

    //this function might need to change
    for(int r=0; r<infmt->height; r++) {
        memcpy(src_cv->imageData + r * src_cv->widthStep, 
               inbuf->data + r * infmt->row_stride,
               src_cv->widthStep);
    }

    cvRemap(src_cv, self->dst_cv, self->mapx, self->mapy,
            CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
//    cvUndistort2(src_cv, self->dst_cv, &intrinsic_matrix, &distortion_matrix);

    self->outbuf->timestamp = inbuf->timestamp;
    self->outbuf->bytesused = outfmt->height * outfmt->row_stride;
    cam_unit_produce_frame(super, self->outbuf, outfmt);

    cvReleaseImage(&src_cv);
}

static void
on_input_format_changed(CamUnit *super, const CamUnitFormat *infmt)
{
    CamcvUndistort *self = (CamcvUndistort*)(super);
    cam_unit_remove_all_output_formats(CAM_UNIT(self));
    if(!infmt) return;
    if(infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY && infmt->pixelformat != CAM_PIXEL_FORMAT_RGB) return;

    if(self->dst_cv) {
        cvReleaseImage(&self->dst_cv);
        g_object_unref(self->outbuf);
    }

    CvSize img_size = { infmt->width, infmt->height };
    if(infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        self->dst_cv = cvCreateImage(img_size, IPL_DEPTH_8U, 1);
    
    if(infmt->pixelformat == CAM_PIXEL_FORMAT_RGB)
        self->dst_cv = cvCreateImage(img_size, IPL_DEPTH_8U, 3);

    uint8_t * dst_data = NULL;
    int out_stride;
    cvGetRawData(self->dst_cv, &dst_data, &out_stride, NULL);
    int buf_sz = infmt->height * out_stride;
    self->outbuf = cam_framebuffer_new(dst_data, buf_sz);
    self->outbuf->bytesused = buf_sz;

    cam_unit_add_output_format(CAM_UNIT(self), 
            infmt->pixelformat,
            NULL, infmt->width, infmt->height, out_stride);
}

static void
_update_mapping(CamcvUndistort * self)
{
    // create the camera intrinsic matrix
    float fx = cam_unit_control_get_float(self->flen_x_ctl);
    float fy = cam_unit_control_get_float(self->flen_y_ctl);
    float cx = cam_unit_control_get_float(self->cop_x_ctl);
    float cy = cam_unit_control_get_float(self->cop_y_ctl);
    float intrinsic_data[] = {
        fx, 0, cx,
        0, fy, cy,
        0, 0, 1
        };
    CvMat intrinsic_matrix = cvMat(3, 3, CV_32FC1, intrinsic_data);

    // assemble distortion coefficients into one array
    float p1 = cam_unit_control_get_float(self->p_1_ctl);
    float p2 = cam_unit_control_get_float(self->p_2_ctl);
    float k1 = cam_unit_control_get_float(self->k_1_ctl);
    float k2 = cam_unit_control_get_float(self->k_2_ctl);
    float k3 = cam_unit_control_get_float(self->k_3_ctl);
    float distortion_coeffs_data[] = { k1, k2, p1, p2, k3 };
    CvMat distortion_matrix = cvMat(5, 1, CV_32FC1, distortion_coeffs_data);

    cvInitUndistortMap(&intrinsic_matrix, &distortion_matrix,
            self->mapx, self->mapy);
}

static gboolean
_try_set_control(CamUnit *super, const CamUnitControl *ctl,
        const GValue *proposed, GValue *actual) {
    CamcvUndistort * self = (CamcvUndistort*)(super);

    g_value_copy(proposed, actual);
    if(ctl == self->enabled_ctl)
        return TRUE;

    _update_mapping(self);

    return TRUE;
}
