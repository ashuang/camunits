#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcam/cam.h>

#include "filter_example.h"

/* Boilerplate */
G_DEFINE_TYPE (MyFilterExample, my_filter_example, CAM_TYPE_UNIT);

CamUnitDriver *
my_filter_example_driver_new()
{
    return cam_unit_driver_new_stock ("filter", "example",
            "Example", 0, (CamUnitConstructor)my_filter_example_new);
}

// ============== MyFilterExample ===============
static void my_filter_example_finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

// Class initializer
static void
my_filter_example_class_init (MyFilterExampleClass *klass)
{
    // override the destructor
    G_OBJECT_CLASS (klass)->finalize = my_filter_example_finalize;

    // override the "on_input_frame_ready" method
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

// First part of the constructor
MyFilterExample * 
my_filter_example_new()
{
    return MY_FILTER_EXAMPLE (g_object_new(MY_TYPE_FILTER_EXAMPLE, NULL));
}

// Initializer.  This is the second part of the constructor.
static void
my_filter_example_init (MyFilterExample *self)
{
    // Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    // create a control
    self->patch_intensity_control = cam_unit_add_control_int (super, 
            "patch-intensity", "Patch Intensity", 0, 255, 1, 127, 1);

    // request notification when the input of the unit changes
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

// destructor.
static void
my_filter_example_finalize (GObject *obj)
{
    // If we allocated memory on the heap/freestore, we'd release it here

    // invoke the superclass destructor
    G_OBJECT_CLASS (my_filter_example_parent_class)->finalize(obj);
}

// this method is called whenever the input unit produces a frame
static void
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    MyFilterExample *self = MY_FILTER_EXAMPLE(super);

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

// this is the signal handler attached in "my_filter_example_init", and is
// called when the format of the input data changes.
static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    // first, clear all available output formats from this unit
    cam_unit_remove_all_output_formats (super);

    // If there is no input, then we can't produce output.  
    if (!infmt) return;
   
    // actually, we can only handle 8-bit RGB input data.
    if (infmt->pixelformat != CAM_PIXEL_FORMAT_RGB) return;

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}
