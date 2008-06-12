#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <libcam/plugin.h>
#include <ipp.h>
#include <ippi.h>

enum {
    FILTER_SIZE_3X3,
    FILTER_SIZE_5x5
};

typedef struct _CamippFilterFixedGauss CamippFilterFixedGauss;
typedef struct _CamippFilterFixedGaussClass CamippFilterFixedGaussClass;

// boilerplate
#define CAMIPP_TYPE_FIXED_FILTER_GAUSS  camipp_filter_fixed_gauss_get_type()
#define CAMIPP_FILTER_FIXED_GAUSS(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMIPP_TYPE_FIXED_FILTER_GAUSS, CamippFilterFixedGauss))
#define CAMIPP_FILTER_FIXED_GAUSS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMIPP_TYPE_FIXED_FILTER_GAUSS, CamippFilterFixedGaussClass ))
#define IS_CAMIPP_FILTER_FIXED_GAUSS(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMIPP_TYPE_FIXED_FILTER_GAUSS ))
#define IS_CAMIPP_FILTER_FIXED_GAUSS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMIPP_TYPE_FIXED_FILTER_GAUSS))
#define CAMIPP_FILTER_FIXED_GAUSS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMIPP_TYPE_FIXED_FILTER_GAUSS, CamippFilterFixedGaussClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

struct _CamippFilterFixedGauss {
    CamUnit parent;

    CamUnitControl *size_ctl;

    CamFrameBuffer *outbuf;
};

struct _CamippFilterFixedGaussClass {
    CamUnitClass parent_class;
};

GType camipp_filter_fixed_gauss_get_type (void);

static CamippFilterFixedGauss * camipp_filter_fixed_gauss_new(void);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);

CAM_PLUGIN_TYPE (CamippFilterFixedGauss, camipp_filter_fixed_gauss, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    camipp_filter_fixed_gauss_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("ipp.filter", "fixed-gauss",
            "Fixed Gaussian Blur", 0, 
            (CamUnitConstructor)camipp_filter_fixed_gauss_new, module);
}

static void
camipp_filter_fixed_gauss_class_init (CamippFilterFixedGaussClass *klass)
{
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
}

static void
camipp_filter_fixed_gauss_init (CamippFilterFixedGauss *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    const char *size_options[] = { 
        "3x3 (sigma = 0.85)", 
        "5x5 (sigma = 1.0)", 
        NULL 
    };
    int size_options_enabled[] = { 1, 1, 0 };

    self->size_ctl = cam_unit_add_control_enum (super,
            "size", "Size", 0, 1, size_options, size_options_enabled);
    self->outbuf = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamippFilterFixedGauss * 
camipp_filter_fixed_gauss_new()
{
    return CAMIPP_FILTER_FIXED_GAUSS(g_object_new(CAMIPP_TYPE_FIXED_FILTER_GAUSS, NULL));
}

static int 
_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamippFilterFixedGauss *self = CAMIPP_FILTER_FIXED_GAUSS (super);
    assert (!self->outbuf);
    self->outbuf = cam_framebuffer_new_alloc (format->max_data_size);
    return 0;
}

static int 
_stream_shutdown (CamUnit * super)
{
    CamippFilterFixedGauss *self = CAMIPP_FILTER_FIXED_GAUSS (super);
    g_object_unref (self->outbuf);
    self->outbuf = NULL;
    return 0;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamippFilterFixedGauss *self = CAMIPP_FILTER_FIXED_GAUSS (super);

    int filter_size = cam_unit_control_get_enum (self->size_ctl);

    IppiSize sz = {
        infmt->width, infmt->height
    };

    IppiMaskSize mask_size;
    if (filter_size == FILTER_SIZE_5x5)
        mask_size = ippMskSize5x5;
    else 
        mask_size = ippMskSize3x3;
    
    switch (infmt->pixelformat) {
        case CAM_PIXEL_FORMAT_GRAY:
            ippiFilterGauss_8u_C1R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, super->fmt->row_stride, sz, mask_size);
            break;
        case CAM_PIXEL_FORMAT_RGB:
        case CAM_PIXEL_FORMAT_BGR:
            ippiFilterGauss_8u_C3R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, super->fmt->row_stride, sz, mask_size);
            break;
        case CAM_PIXEL_FORMAT_BGRA:
        case CAM_PIXEL_FORMAT_RGBA:
            ippiFilterGauss_8u_C4R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, super->fmt->row_stride, sz, mask_size);
            break;
        default:
            // TODO
            break;
    }

    cam_unit_produce_frame (super, self->outbuf, super->fmt);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamippFilterFixedGauss *self = CAMIPP_FILTER_FIXED_GAUSS (super);
    cam_unit_remove_all_output_formats (CAM_UNIT (self));
    if (!infmt) return;
    if (infmt->pixelformat != CAM_PIXEL_FORMAT_RGB &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_BGR &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_BGRA) return;

    cam_unit_add_output_format_full (CAM_UNIT (self), 
            infmt->pixelformat,
            NULL, infmt->width, infmt->height, infmt->row_stride, 
            infmt->max_data_size);
}
