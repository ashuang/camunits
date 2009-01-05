#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib-object.h>

#include <camunits/plugin.h>
#include <camunits/unit.h>

// This file demonstrates how to create a simple CamUnit plugin.  When
// implementing a custom MyUnit, replace "FilterPlugin", "FILTER_PLUGIN",
// and "filter_plugin" with your own names.  You'll also want to pick a
// different namespace (i.e. prefix) from "My"

typedef struct _MyFilterPlugin MyFilterPlugin;
typedef struct _MyFilterPluginClass MyFilterPluginClass;

// boilerplate.
#define MY_TYPE_FILTER_PLUGIN  my_filter_plugin_get_type()
#define MY_FILTER_PLUGIN(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        MY_TYPE_FILTER_PLUGIN, MyFilterPlugin))
#define MY_FILTER_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            MY_TYPE_FILTER_PLUGIN, MyFilterPluginClass ))
#define IS_MY_FILTER_PLUGIN(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            MY_TYPE_FILTER_PLUGIN ))
#define IS_MY_FILTER_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), MY_TYPE_FILTER_PLUGIN))
#define MY_FILTER_PLUGIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            MY_TYPE_FILTER_PLUGIN, MyFilterPluginClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

// object definition struct.  member variables go in here
struct _MyFilterPlugin {
    // the first member must always be the superclass struct
    CamUnit parent;

    // add one member variable
    CamUnitControl *enable_ctl;
};

// class definition.  This is pretty much a vtable, and you will rarely need to
// change it
struct _MyFilterPluginClass {
    CamUnitClass parent_class;
};

GType my_filter_plugin_get_type (void);

CAM_PLUGIN_TYPE (MyFilterPlugin, my_filter_plugin, CAM_TYPE_UNIT);

MyFilterPlugin * my_filter_plugin_new();

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    my_filter_plugin_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("filter", "plugin",
            "Example Filter Plugin", 0, 
            (CamUnitConstructor)my_filter_plugin_new,
            module);
}

// ============== MyFilterPlugin ===============
static void my_filter_plugin_finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

// Class initializer
static void
my_filter_plugin_class_init (MyFilterPluginClass *klass)
{
    // override the destructor
    G_OBJECT_CLASS (klass)->finalize = my_filter_plugin_finalize;

    // override the "on_input_frame_ready" method
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

// First part of the constructor
MyFilterPlugin * 
my_filter_plugin_new()
{
    return MY_FILTER_PLUGIN (g_object_new(MY_TYPE_FILTER_PLUGIN, NULL));
}

// Initializer.  This is the second part of the constructor.
static void
my_filter_plugin_init (MyFilterPlugin *self)
{
    // Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    // create a control
    self->enable_ctl = cam_unit_add_control_boolean (super, 
            "enable-intensity", "Swap Red/Green Channels", 1, 1);

    // request notification when the input of the unit changes
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

// destructor.
static void
my_filter_plugin_finalize (GObject *obj)
{
    // If we allocated memory on the heap/freestore, we'd release it here

    // invoke the superclass destructor
    G_OBJECT_CLASS (my_filter_plugin_parent_class)->finalize(obj);
}

// this method is called whenever the input unit produces a frame
static void
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    MyFilterPlugin *self = MY_FILTER_PLUGIN(super);

    if(!cam_unit_control_get_boolean(self->enable_ctl)) {
        cam_unit_produce_frame(super, inbuf, infmt);
        return;
    }

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);
    const CamUnitFormat *outfmt = cam_unit_get_output_format (super);

    // swap the red and green channels
    int row;
    int col;
    for(row=0; row<infmt->height; row++) {
        uint8_t *src_row = inbuf->data + infmt->row_stride*row;
        uint8_t *dst_row = outbuf->data + outfmt->row_stride*row;
        for(col=0; col<infmt->width; col++) {
            dst_row[col*3+0] = src_row[col*3+1];
            dst_row[col*3+1] = src_row[col*3+0];
            dst_row[col*3+2] = src_row[col*3+2];
        }
    }

    // copy the timestamp and metadata dictionary
    cam_framebuffer_copy_metadata (outbuf, inbuf);

    outbuf->bytesused = super->fmt->row_stride * super->fmt->height;

    cam_unit_produce_frame (super, outbuf, super->fmt);
    g_object_unref (outbuf);
}

// this is the signal handler attached in "my_filter_plugin_init", and is
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
