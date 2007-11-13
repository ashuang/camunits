#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "input_example.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

#define FPS_CTL_NAME_ID 0
#define FPS_CTL_NAME "FPS"

static int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static int fps_numer_options[] = { 1, 5, 15, 30 };

// ============== CamInputExampleDriver ===============

static CamUnit * cam_input_example_driver_create_unit(CamUnitDriver *super,
        const CamUnitDescription * udesc);
static CamUnitDescription * 
cam_input_example_driver_search_unit_description (CamUnitDriver *driver, 
        const char *id);
static void cam_input_example_driver_finalize (GObject *obj);
static int cam_input_example_driver_start (CamUnitDriver *self);

G_DEFINE_TYPE (CamInputExampleDriver, cam_input_example_driver, \
        CAM_TYPE_UNIT_DRIVER);

static void
cam_input_example_driver_init (CamInputExampleDriver *self)
{
    dbg(DBG_DRIVER, "example driver constructor\n");
    // CamInputExampleDriver constructor
 
    CamUnitDriver *super = CAM_UNIT_DRIVER(self);
    cam_unit_driver_set_package (super, "input");
}

static void
cam_input_example_driver_class_init (CamInputExampleDriverClass *klass)
{
    dbg(DBG_DRIVER, "example driver class initializer\n");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    
    // add a class-specific destructor
    gobject_class->finalize = cam_input_example_driver_finalize;

    // override methods
    klass->parent_class.create_unit = cam_input_example_driver_create_unit;
    klass->parent_class.search_unit_description = 
        cam_input_example_driver_search_unit_description;
    klass->parent_class.start = cam_input_example_driver_start;
//    klass->parent_class.stop = cam_input_example_driver_stop;
}

// destructor (more or less)
static void
cam_input_example_driver_finalize (GObject *obj)
{
    dbg(DBG_DRIVER, "example driver finalize\n");
//    CamInputExampleDriver *self = CAM_INPUT_EXAMPLE_DRIVER (obj);

    G_OBJECT_CLASS (cam_input_example_driver_parent_class)->finalize(obj);
}

CamInputExampleDriver *
cam_input_example_driver_new()
{
    return CAM_INPUT_EXAMPLE_DRIVER(
            g_object_new(CAM_INPUT_EXAMPLE_DRIVER_TYPE, NULL));
}

static int 
cam_input_example_driver_start (CamUnitDriver *super)
{
    cam_unit_driver_add_unit_description (super, 
            "Example Input", "input:input_example",
            CAM_UNIT_EVENT_METHOD_TIMEOUT);
    return 0;
}

static CamUnit * 
cam_input_example_driver_create_unit(CamUnitDriver *super,
        const CamUnitDescription * udesc)
{
    dbg(DBG_DRIVER, "example driver creating new CamExample unit\n");
    CamInputExample *result = cam_input_example_new();

    return CAM_UNIT (result);
}

static CamUnitDescription * 
cam_input_example_driver_search_unit_description (CamUnitDriver *super, 
        const char *id)
{
    const char *expected_id = "input_example:1";
    if (! strcmp (id, expected_id)) {
        CamUnitDescription *udesc = 
            cam_unit_driver_add_unit_description (super,
                    "CamExample Unit", id, 
                    CAM_UNIT_EVENT_METHOD_TIMEOUT);
        return udesc;
    }
    return NULL;
}

// ============== CamInputExample ===============

static void cam_input_example_finalize (GObject *obj);
static int cam_input_example_stream_on (CamUnit *super);
static void cam_input_example_try_produce_frame (CamUnit * super);
static int64_t cam_input_example_get_next_event_time (CamUnit *super);
static gboolean cam_example_try_set_control(CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);

G_DEFINE_TYPE (CamInputExample, cam_input_example, CAM_TYPE_UNIT);

static void
cam_input_example_init (CamInputExample *self)
{
    dbg(DBG_INPUT, "example constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->next_frame_time = 0;
    self->fps = fps_numer_options[0];

    const char *menu_options[] = { "1", "5", "15", "30", NULL };
    self->enum_ctl = cam_unit_add_control_enum (super, "enum", "menu",
            0, 1, menu_options, NULL);
    self->bool_ctl = cam_unit_add_control_boolean (super, "boolean", "bool", 
            0, 1);
    self->int1_ctl = cam_unit_add_control_int (super, "int1", "int 1", 
            0, 100, 2, 50, 1);
    self->int2_ctl = cam_unit_add_control_int (super, "int2", "int 2", 
            10, 20, 1, 15, 0);

    cam_unit_add_output_format_full (super, CAM_PIXEL_FORMAT_RGB, 
            "640x480 RGB", 640, 480, 640*3, 640*480*3);
}

static void
cam_input_example_class_init (CamInputExampleClass *klass)
{
    dbg(DBG_INPUT, "example class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_input_example_finalize;

    klass->parent_class.stream_on = cam_input_example_stream_on;
    klass->parent_class.try_produce_frame = cam_input_example_try_produce_frame;
    klass->parent_class.get_next_event_time = 
        cam_input_example_get_next_event_time;

    klass->parent_class.try_set_control = cam_example_try_set_control;
}

static void
cam_input_example_finalize (GObject *obj)
{
    dbg(DBG_INPUT, "example finalize\n");
//    CamInputExample *self = CAM_INPUT_EXAMPLE (obj);

    G_OBJECT_CLASS (cam_input_example_parent_class)->finalize(obj);
}

CamInputExample * 
cam_input_example_new()
{
    return CAM_INPUT_EXAMPLE (g_object_new (CAM_INPUT_EXAMPLE_TYPE, NULL));
}

static int
cam_input_example_stream_on (CamUnit *super)
{
    dbg(DBG_INPUT, "example stream on\n");
    CamInputExample *self = CAM_INPUT_EXAMPLE (super);
    self->next_frame_time = _timestamp_now();
    self->cur_row = 0;
    
    cam_unit_set_status (super, CAM_UNIT_STATUS_STREAMING);
    return 0;
}

static void 
cam_input_example_try_produce_frame (CamUnit *super)
{
    dbg(DBG_INPUT, "iterate\n");
    CamInputExample *self = CAM_INPUT_EXAMPLE (super);

    int64_t frame_delay_usec = 1000000. / self->fps;
    self->next_frame_time += frame_delay_usec;

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);

    memset(outbuf->data, 0, outbuf->length);
    memset(outbuf->data, 255, self->cur_row * super->fmt->row_stride);
    self->cur_row = (self->cur_row + 1) % super->fmt->height;

    outbuf->bytesused = super->fmt->height * super->fmt->row_stride;
    outbuf->timestamp = self->next_frame_time;

    cam_unit_produce_frame (super, outbuf, super->fmt);
    g_object_unref (outbuf);
}

static int64_t
cam_input_example_get_next_event_time (CamUnit *super)
{
    return CAM_INPUT_EXAMPLE (super)->next_frame_time;
}

static gboolean
cam_example_try_set_control(CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual)
{
    CamInputExample *self = CAM_INPUT_EXAMPLE (super);
    if (ctl == self->enum_ctl) {
        self->fps = fps_numer_options[ g_value_get_int(proposed) ];
        self->next_frame_time = _timestamp_now();
    }

    g_value_copy (proposed, actual);
    return TRUE;
}
