#include "filter_example.h"

// This file demonstrates how to create a simple CamUnit subclass.  When
// implementing a custom MyUnit, replace "MyFilterExample", "MY_FILTER_EXAMPLE",
// and "my_filter_example" with your own names.  

typedef struct _MyFilterExample MyFilterExample;
typedef struct _MyFilterExampleClass MyFilterExampleClass;

// This macro is a type-safe alternative to "(MyFilterPlugin*)obj"
// If obj is not a pointer to MyFilterPlugin, then a warning message
// is printed to stderr, and the macro returns NULL.  It's reasonable to delete
// this macro if you don't need it.
#define MY_FILTER_EXAMPLE(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        my_filter_example_get_type(), MyFilterExample))

// Class definition struct.  member variables go in here
struct _MyFilterExample {
    // the first member must always be the superclass struct
    CamUnit parent;

    // add one member variable
    CamUnitControl *enable_ctl;
};

// Class definition.  This is pretty much a vtable, and you will rarely need to
// change it
struct _MyFilterExampleClass {
    CamUnitClass parent_class;
};

GType my_filter_example_get_type (void);

CamUnit * my_filter_example_new()
{
    return CAM_UNIT (g_object_new(my_filter_example_get_type(), NULL));
}

// GLib magic macro
G_DEFINE_TYPE (MyFilterExample, my_filter_example, CAM_TYPE_UNIT);

// Most CamUnit subclasses will not need complex drivers.  CamUnit provides a
// "stock" driver that simply calls a programmer-specified constructor when
// needed.
CamUnitDriver *
my_filter_example_driver_new()
{
    return cam_unit_driver_new_stock ("example", "filter",
            "Example Filter", 0, (CamUnitConstructor)my_filter_example_new);
}

// ============== MyFilterExample ===============
static void _finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

// Class initializer.  This function sets up the class vtable, and is most
// commonly used for overriding superclass methods.
static void
my_filter_example_class_init (MyFilterExampleClass *klass)
{
    // override the destructor
    G_OBJECT_CLASS (klass)->finalize = _finalize;

    // override the "on_input_frame_ready" method
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

// Initializer.  This is essentially a constructor.
static void
my_filter_example_init (MyFilterExample *self)
{
    // Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    // create a control
    self->enable_ctl = cam_unit_add_control_boolean (super, 
            "enable-intensity", "Swap Red/Blue Channels", 1, 1);

    // request notification when the input of the unit changes
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

// destructor.
static void
_finalize (GObject *obj)
{
    // If we allocated memory on the heap/freestore, we'd release it here

    // invoke the superclass destructor.  my_filter_example_parent_class is 
    // defined by the magic macro
    G_OBJECT_CLASS (my_filter_example_parent_class)->finalize(obj);
}

// this method is called whenever the input unit produces a frame
static void
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    MyFilterExample *self = MY_FILTER_EXAMPLE(super);

    if(!cam_unit_control_get_boolean(self->enable_ctl)) {
        cam_unit_produce_frame(super, inbuf, infmt);
        return;
    }

    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc(outfmt->height * outfmt->row_stride);

    // swap the red and blue channels
    int row;
    int col;
    for(row=0; row<infmt->height; row++) {
        uint8_t *src_row = inbuf->data + infmt->row_stride*row;
        uint8_t *dst_row = outbuf->data + outfmt->row_stride*row;
        for(col=0; col<infmt->width; col++) {
            dst_row[col*3+0] = src_row[col*3+2];
            dst_row[col*3+1] = src_row[col*3+1];
            dst_row[col*3+2] = src_row[col*3+0];
        }
    }

    // copy the timestamp and metadata dictionary
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
            infmt->row_stride);
}
