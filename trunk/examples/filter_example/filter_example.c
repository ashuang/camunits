#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcam/cam.h>

#include "filter_example.h"

/* Boilerplate */
G_DEFINE_TYPE (CamFilterExample, cam_filter_example, CAM_TYPE_UNIT);

CamUnitDriver *
cam_filter_example_driver_new()
{
    return cam_unit_driver_new_stock ("filter", "example",
            "Example", 0, (CamUnitConstructor)cam_filter_example_new);
}

// ============== CamFilterExample ===============
static void cam_filter_example_finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

// class initializer
static void
cam_filter_example_class_init (CamFilterExampleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_filter_example_finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

CamFilterExample * 
cam_filter_example_new()
{
    return CAM_FILTER_EXAMPLE (g_object_new(CAM_TYPE_FILTER_EXAMPLE, NULL));
}

static void
cam_filter_example_init (CamFilterExample *self)
{
    // Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->patch_intensity_control = cam_unit_add_control_int (super, 
            "patch-intensity", "Patch Intensity", 0, 255, 1, 127, 1);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

static void
cam_filter_example_finalize (GObject *obj)
{
    // destructor.  release heap/freestore memory here
    G_OBJECT_CLASS (cam_filter_example_parent_class)->finalize(obj);
}

static void
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamFilterExample *self = CAM_FILTER_EXAMPLE(super);

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);

    cam_pixel_copy_8u_generic (inbuf->data, infmt->row_stride,
            outbuf->data, super->fmt->row_stride,
            0, 0, 0, 0, infmt->width, infmt->height, 
            cam_pixel_format_bpp (infmt->pixelformat));

    // draw a little rectangle
    int x0 = super->fmt->width / 4;
    int x1 = x0 * 3;
    int rw = x1 - x0;
    int y0 = super->fmt->height / 4;
    int y1 = y0 * 3;

    int val = cam_unit_control_get_int (self->patch_intensity_control);

    int i;
    for (i=y0; i<y1; i++) {
        memset (outbuf->data + i*super->fmt->row_stride + x0*3, val, rw*3);
    }

    // copy the bytesused, timestamp, source_uid, etc. fields.
    cam_framebuffer_copy_metadata (outbuf, inbuf);
    outbuf->bytesused = super->fmt->row_stride * super->fmt->height;

    cam_unit_produce_frame (super, outbuf, super->fmt);
    g_object_unref (outbuf);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);
    if (!infmt || infmt->pixelformat != CAM_PIXEL_FORMAT_RGB) return;

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}
