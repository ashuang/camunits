#include <camunits/plugin.h>

// This file demonstrates how to create a simple CamUnit plugin.  When
// implementing a custom MyUnit, replace "MyFilterPlugin", "MY_FILTER_PLUGIN",
// and "my_filter_plugin" with your own names.  

typedef struct _MyFilterPlugin MyFilterPlugin;
typedef struct _MyFilterPluginClass MyFilterPluginClass;

// This macro is a type-safe alternative to "(MyFilterPlugin*)obj"
// If obj is not a pointer to MyFilterPlugin, then a warning message
// is printed to stderr, and the macro returns NULL.  It's reasonable to delete
// this macro if you don't need it.
#define MY_FILTER_PLUGIN(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        my_filter_plugin_get_type(), MyFilterPlugin))

// Class definition struct.  member variables go in here
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

MyFilterPlugin * my_filter_plugin_new()
{
    return MY_FILTER_PLUGIN(g_object_new(my_filter_plugin_get_type(), NULL));
}

// magic macro.
CAM_PLUGIN_TYPE (MyFilterPlugin, my_filter_plugin, CAM_TYPE_UNIT);

// These next two functions are required as entry points for the
// Camunits plug-in API.
void cam_plugin_initialize (GTypeModule * module)
{
    // this function is defined by the magic macro above, and registers
    // the plugin with the GObject type system.
    my_filter_plugin_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("example", "plugin",
            "Example Filter Plugin", 0, 
            (CamUnitConstructor)my_filter_plugin_new, module);
}

// ============== MyFilterPlugin ===============
static void _finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

// Class initializer.  This function sets up the class vtable, and is most
// commonly used for overriding superclass methods.
static void
my_filter_plugin_class_init (MyFilterPluginClass *klass)
{
    // override the destructor
    G_OBJECT_CLASS (klass)->finalize = _finalize;

    // override the "on_input_frame_ready" method
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

// Initializer.  This is essentially a constructor.
static void
my_filter_plugin_init (MyFilterPlugin *self)
{
    // Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    // create a control
    self->enable_ctl = cam_unit_add_control_boolean (super, 
            "enable", "Swap Red/Green Channels", 1, 1);

    // request notification when the input of the unit changes
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

// destructor.
static void
_finalize (GObject *obj)
{
    // If we allocated memory on the heap/freestore, we'd release it here

    // invoke the superclass destructor.  my_filter_plugin_parent_class is 
    // defined by the magic macro
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

    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    int buf_sz = outfmt->height * outfmt->row_stride;
    CamFrameBuffer *outbuf = cam_framebuffer_new_alloc(buf_sz);

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

    outbuf->bytesused = buf_sz;

    cam_unit_produce_frame (super, outbuf, outfmt);
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

    cam_unit_add_output_format (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride);
}
