#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dc1394/conversions.h>

#include "filter_bayer.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

CamUnitDriver *
cam_bayer_filter_driver_new()
{
    return cam_unit_driver_new_stock( "convert", "debayer",
            "Bayer Demosaic", 0,
            (CamUnitConstructor)cam_bayer_filter_new );
}

// ============== CamBayerFilter ===============
static int cam_bayer_filter_stream_init (CamUnit * super,
        const CamUnitFormat * fmt);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

G_DEFINE_TYPE (CamBayerFilter, cam_bayer_filter, CAM_TYPE_UNIT);

static void
cam_bayer_filter_init( CamBayerFilter *self )
{
    dbg(DBG_FILTER, "bayer filter constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT( self );

    // WARNING.  do not change these without also changing the enum
    // in filter_bayer.h
    const char * bayer_methods[] = {
        "Nearest",
        "Simple",
        "Bilinear",
        "HQ Linear",
        "Edge Sense",
        "VNG",
        NULL,
    };
    self->bayer_method_ctl =
        cam_unit_add_control_enum (super, "method", "Method", 
                0, 1, bayer_methods, NULL);

    // WARNING.  do not change these without also changing the enum
    // in filter_bayer.h
    const char * bayer_tiles[] = {
        "GBRG",
        "GRBG",
        "BGGR",
        "RGGB",
        NULL,
    };
    self->bayer_tile_ctl =
        cam_unit_add_control_enum (super, "tiling", "Tiling", 
                0, 1, bayer_tiles, NULL);

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
cam_bayer_filter_class_init( CamBayerFilterClass *klass )
{
    klass->parent_class.stream_init = cam_bayer_filter_stream_init;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

CamBayerFilter * 
cam_bayer_filter_new()
{
    return CAM_BAYER_FILTER(g_object_new(CAM_TYPE_BAYER_FILTER, NULL));
}

static uint32_t
get_bayer_method( CamBayerFilter *self )
{
    int method = cam_unit_control_get_enum( self->bayer_method_ctl );
    switch( method ) {
        case 0:
            return DC1394_BAYER_METHOD_NEAREST;
        case 1:
            return DC1394_BAYER_METHOD_SIMPLE;
        case 2:
            return DC1394_BAYER_METHOD_BILINEAR;
        case 3:
            return DC1394_BAYER_METHOD_HQLINEAR;
        case 4:
            return DC1394_BAYER_METHOD_EDGESENSE;
        case 5:
            return DC1394_BAYER_METHOD_VNG;
        default:
            return DC1394_BAYER_METHOD_NEAREST;
    }
}

static uint32_t
get_bayer_tile( CamBayerFilter *self )
{
    int tiling = cam_unit_control_get_enum( self->bayer_tile_ctl );
    switch( tiling ) {
        case 0:
            return DC1394_COLOR_FILTER_GBRG;
        case 1:
            return DC1394_COLOR_FILTER_GRBG;
        case 2:
            return DC1394_COLOR_FILTER_BGGR;
        case 3:
            return DC1394_COLOR_FILTER_RGGB;
        default:
            fprintf (stderr, "Warning: invalid tiling selected\n");
            return DC1394_COLOR_FILTER_BGGR;
    }
}

static int
cam_bayer_filter_stream_init (CamUnit * super, const CamUnitFormat * fmt)
{
    CamBayerFilter * self = CAM_BAYER_FILTER (super);

    switch (super->input_unit->fmt->pixelformat) {
        case CAM_PIXEL_FORMAT_BAYER_GBRG:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 0);
            break;
        case CAM_PIXEL_FORMAT_BAYER_GRBG:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 1);
            break;
        case CAM_PIXEL_FORMAT_BAYER_BGGR:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 2);
            break;
        case CAM_PIXEL_FORMAT_BAYER_RGGB:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 3);
            break;
        default:
            return -1;
    }

    return 0;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt)
{
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);

    CamBayerFilter *self = CAM_BAYER_FILTER(super);
    uint32_t method = get_bayer_method(self);
    uint32_t tiling = get_bayer_tile(self);

    dc1394_bayer_decoding_8bit (inbuf->data, outbuf->data,
            super->fmt->width, super->fmt->height, tiling, method);

    cam_framebuffer_copy_metadata (outbuf, inbuf);
    outbuf->bytesused = super->fmt->height * super->fmt->row_stride;

    // queue the outgoing buffer
    cam_unit_produce_frame (super, outbuf, super->fmt);

    g_object_unref (outbuf);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (!infmt) return;

    if (! (infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_RGGB ||
                infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_BGGR ||
                infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_GBRG ||
                infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_GRBG ||
                infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)) return;

    CamPixelFormat out_pixelformat = CAM_PIXEL_FORMAT_RGB;

    int stride = infmt->width * cam_pixel_format_bpp(out_pixelformat) / 8;
    int max_data_size = infmt->height * stride;

    cam_unit_add_output_format_full (super, out_pixelformat,
            NULL, infmt->width, infmt->height, 
            stride, max_data_size);
}
