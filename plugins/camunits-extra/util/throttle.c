#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include <camunits/plugin.h>

#define err(args...) fprintf(stderr, args)

typedef enum {
    THROTTLE_DISABLED,
    THROTTLE_BY_REALTIME,
    THROTTLE_BY_REPORTED,
} throttle_mode_t;

typedef struct {
    CamUnit parent;

    CamFrameBuffer *prev_buf;

    CamUnitControl *pause_ctl;
    CamUnitControl *repeat_last_frame_ctl;
    CamUnitControl *throttle_mode_ctl;
    CamUnitControl *throttle_hz_ctl;

    int64_t last_allowed_actual_timestamp;
    int64_t last_allowed_reported_timestamp;
} CamutilThrottle;

typedef struct {
    CamUnitClass parent_class;
} CamutilThrottleClass;

static CamutilThrottle * camutil_throttle_new(void);
static int _stream_shutdown (CamUnit * super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

GType camutil_throttle_get_type (void);
CAM_PLUGIN_TYPE (CamutilThrottle, camutil_throttle, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camutil_throttle_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("util", "throttle", "Throttle", 
            0, (CamUnitConstructor)camutil_throttle_new, module);
}

static void
camutil_throttle_class_init (CamutilThrottleClass *klass)
{
    // class initializer.  setup the class vtable here.
    klass->parent_class.stream_shutdown = _stream_shutdown;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camutil_throttle_init (CamutilThrottle *self)
{
    // "private" constructor.  Initialize the unit with some reasonable
    // defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->pause_ctl = cam_unit_add_control_boolean (super, "pause",
            "Pause", 0, 1);
    self->repeat_last_frame_ctl = cam_unit_add_control_boolean (super, 
            "repeat", "Repeat Last Frame", 0, 1);
    cam_unit_control_set_ui_hints (self->repeat_last_frame_ctl, 
            CAM_UNIT_CONTROL_ONE_SHOT);

    CamUnitControlEnumValue throttle_mode_options[] = { 
        { THROTTLE_DISABLED   , "Do Not Throttle", 1 },
        { THROTTLE_BY_REALTIME, "Actual Time (Wall clock)", 1 },
        { THROTTLE_BY_REPORTED, "Image Timestamps", 1 },
        { 0, NULL, 0 }
    };

    self->throttle_mode_ctl = cam_unit_add_control_enum (super, 
            "throttle-mode", "Throttle Mode", THROTTLE_DISABLED, 1, 
            throttle_mode_options);

    self->throttle_hz_ctl = cam_unit_add_control_float (super, 
            "throttle-rate", "Max Rate (Hz)", 0.001, 1000, 1, 100, 1);
    cam_unit_control_set_ui_hints (self->throttle_hz_ctl, 
            CAM_UNIT_CONTROL_SPINBUTTON);

    self->prev_buf = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamutilThrottle * 
camutil_throttle_new()
{
    // "public" constructor
    return (CamutilThrottle*)(g_object_new(camutil_throttle_get_type(), NULL));
}

static int 
_stream_shutdown (CamUnit * super)
{
    CamutilThrottle *self = (CamutilThrottle*) (super);
    if (self->prev_buf) {
        g_object_unref (self->prev_buf);
        self->prev_buf = NULL;
    }
    return 0;
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);
    if (! infmt) return;
    cam_unit_add_output_format (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride);
}

static int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static void
_copy_framebuffer (CamutilThrottle *self, const CamFrameBuffer *inbuf)
{
    if (self->prev_buf)
        g_object_unref (self->prev_buf);
    self->prev_buf = cam_framebuffer_new_alloc (inbuf->bytesused);
    memcpy (self->prev_buf->data, inbuf->data, inbuf->bytesused);
    cam_framebuffer_copy_metadata (self->prev_buf, inbuf);
    self->prev_buf->bytesused = inbuf->bytesused;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamutilThrottle *self = (CamutilThrottle*)(super);

    if (cam_unit_control_get_boolean (self->pause_ctl)) {
        // unit is paused.  Only allow a frame if the user says to allow one
        return;
    }

    int64_t now = _timestamp_now ();

    int tm = cam_unit_control_get_enum (self->throttle_mode_ctl);
    if (tm == THROTTLE_DISABLED) {
        self->last_allowed_actual_timestamp = _timestamp_now ();
        self->last_allowed_reported_timestamp = inbuf->timestamp;
        _copy_framebuffer (self, inbuf);
        cam_unit_produce_frame (super, inbuf, infmt);
        return;
    }

    double dt = -1;
    if (tm == THROTTLE_BY_REALTIME) {
        dt = (now - self->last_allowed_actual_timestamp) * 1e-6;
    } else if (tm == THROTTLE_BY_REPORTED) {
        dt = (inbuf->timestamp - self->last_allowed_reported_timestamp) * 1e-6;
    }

    double max_rate = cam_unit_control_get_float (self->throttle_hz_ctl);
    double min_interval = 1. / max_rate;

//    printf ("rate ctl: %f min_interval: %f dt: %f\n", 
//            max_rate, min_interval, dt);

    if (min_interval < dt || dt < 0) {
        self->last_allowed_actual_timestamp = now;
        self->last_allowed_reported_timestamp = inbuf->timestamp;
        _copy_framebuffer (self, inbuf);
        cam_unit_produce_frame (super, inbuf, infmt);
    }
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamutilThrottle *self = (CamutilThrottle*) (super);
    if (ctl == self->repeat_last_frame_ctl && self->prev_buf) {
        cam_unit_produce_frame (super, self->prev_buf, 
                cam_unit_get_output_format(super));
    }
    g_value_copy (proposed, actual);
    return TRUE;
}
