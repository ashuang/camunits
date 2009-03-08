#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <camunits/plugin.h>
#include <ipp.h>
#include <ippi.h>

enum {
    FILTER_SIZE_3X3,
    FILTER_SIZE_5x5
};

typedef struct {
    CamUnit parent;

    CamUnitControl *size_ctl;

    CamFrameBuffer *outbuf;
} CamippFilterFixedGauss;

typedef struct {
    CamUnitClass parent_class;
} CamippFilterFixedGaussClass;

static CamippFilterFixedGauss * camipp_filter_fixed_gauss_new(void);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);

GType camipp_filter_fixed_gauss_get_type (void);
CAM_PLUGIN_TYPE (CamippFilterFixedGauss, camipp_filter_fixed_gauss, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camipp_filter_fixed_gauss_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("ipp", "filter-fixed-gauss",
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

    CamUnitControlEnumValue size_options[] = { 
        { FILTER_SIZE_3X3, "3x3 (sigma = 0.85)", 1 },
        { FILTER_SIZE_5x5, "5x5 (sigma = 1.0)", 1 },
        { 0, NULL, 0 },
    };

    self->size_ctl = cam_unit_add_control_enum (super,
            "size", "Size", FILTER_SIZE_3X3, 1, size_options);
    self->outbuf = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamippFilterFixedGauss * 
camipp_filter_fixed_gauss_new()
{
    return (CamippFilterFixedGauss*)(g_object_new(camipp_filter_fixed_gauss_get_type(), NULL));
}

static int 
_stream_init (CamUnit * super, const CamUnitFormat * fmt)
{
    CamippFilterFixedGauss *self = (CamippFilterFixedGauss*) (super);
    assert (!self->outbuf);
    self->outbuf = cam_framebuffer_new_alloc (fmt->height * fmt->row_stride);
    return 0;
}

static int 
_stream_shutdown (CamUnit * super)
{
    CamippFilterFixedGauss *self = (CamippFilterFixedGauss*) (super);
    g_object_unref (self->outbuf);
    self->outbuf = NULL;
    return 0;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamippFilterFixedGauss *self = (CamippFilterFixedGauss*) (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

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
                    self->outbuf->data, outfmt->row_stride, sz, mask_size);
            break;
        case CAM_PIXEL_FORMAT_RGB:
        case CAM_PIXEL_FORMAT_BGR:
            ippiFilterGauss_8u_C3R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, outfmt->row_stride, sz, mask_size);
            break;
        case CAM_PIXEL_FORMAT_BGRA:
        case CAM_PIXEL_FORMAT_RGBA:
            ippiFilterGauss_8u_C4R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, outfmt->row_stride, sz, mask_size);
            break;
        default:
            // TODO
            break;
    }

    cam_framebuffer_copy_metadata(self->outbuf, inbuf);
    self->outbuf->bytesused = outfmt->row_stride * outfmt->height;

    cam_unit_produce_frame (super, self->outbuf, outfmt);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamippFilterFixedGauss *self = (CamippFilterFixedGauss*) (super);
    cam_unit_remove_all_output_formats (CAM_UNIT (self));
    if (!infmt) return;
    if (infmt->pixelformat != CAM_PIXEL_FORMAT_RGB &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_BGR &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_BGRA) return;

    cam_unit_add_output_format (CAM_UNIT (self), 
            infmt->pixelformat,
            NULL, infmt->width, infmt->height, infmt->row_stride);
}
