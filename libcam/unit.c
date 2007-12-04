#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>

#include "libcam-gmarshal.h"
#include "unit.h"

#include "dbg.h"

#define err(args...) fprintf(stderr, args)

#define DEFAULT_NBUFFERS 60

enum {
    CONTROL_VALUE_CHANGED_SIGNAL,
    CONTROL_PARAMETERS_CHANGED_SIGNAL,
    OUTPUT_FORMATS_CHANGED_SIGNAL,
    STATUS_CHANGED_SIGNAL,
    INPUT_CHANGED_SIGNAL,
    INPUT_FORMAT_CHANGED_SIGNAL,
    FRAME_READY_SIGNAL,
    LAST_SIGNAL
};

static guint cam_unit_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_finalize (GObject *obj);

static int cam_unit_default_stream_init (CamUnit *self, 
        const CamUnitFormat *format);
static int cam_unit_default_stream_shutdown (CamUnit *self);
static int cam_unit_default_stream_on (CamUnit *self);
static int cam_unit_default_stream_off (CamUnit *self);
static int cam_unit_default_get_fileno (CamUnit *self);
static int64_t cam_unit_default_get_next_event_time (CamUnit *self);
static int cam_unit_default_draw_gl_init (CamUnit * self);
static int cam_unit_default_draw_gl (CamUnit *self);
static int cam_unit_default_draw_gl_shutdown (CamUnit * self);

static void on_input_unit_status_changed (CamUnit *input_unit, int old_status,
        void *user_data);
static void on_input_frame_ready (CamUnit *input_unit, 
        const CamFrameBuffer *buf, const CamUnitFormat *infmt, 
        void *user_data);

G_DEFINE_TYPE (CamUnit, cam_unit, G_TYPE_INITIALLY_UNOWNED);

static void
cam_unit_init (CamUnit *self)
{
    dbg(DBG_UNIT, "constructor\n");
    self->name = NULL;
    self->flags = 0;
    self->unit_id = NULL;

    self->input_unit = NULL;
    self->status = CAM_UNIT_STATUS_IDLE;

    // create a hash table for the controls that automatically frees the 
    // memory used by a value when the value is removed
    self->controls = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, 
           (GDestroyNotify) g_object_unref);
    // also keep a strictly ordered linked list around.  This is mostly so that
    // when list_controls is called, the result is a list with controls listed
    // in the order that they were added.
    self->controls_list = NULL;

    self->output_formats = NULL;
    self->fmt = NULL;

    self->requested_pixelformat = CAM_PIXEL_FORMAT_INVALID;
    self->requested_width = 0;
    self->requested_height = 0;
}

static void
cam_unit_finalize (GObject *obj)
{
    CamUnit *self = CAM_UNIT (obj);
    dbg(DBG_UNIT, "CamUnit finalize [%s]\n", self->unit_id);

    if (self->name) { free (self->name); }
    if (self->unit_id) { free (self->unit_id); }
    if (self->input_unit) { 
        g_signal_handlers_disconnect_by_func (self->input_unit, 
                on_input_unit_status_changed, self);
        g_signal_handlers_disconnect_by_func (self->input_unit,
                on_input_frame_ready, self);
        g_object_unref (self->input_unit); 
    }

    g_hash_table_destroy (self->controls);
    g_list_free (self->controls_list);

    for (GList *ofiter=self->output_formats; ofiter; ofiter=ofiter->next) {
        g_object_unref (ofiter->data);
    }
    g_list_free (self->output_formats);

    G_OBJECT_CLASS (cam_unit_parent_class)->finalize(obj);
}

static void
cam_unit_class_init (CamUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_unit_finalize;

    klass->stream_init = cam_unit_default_stream_init;
    klass->stream_shutdown = cam_unit_default_stream_shutdown;
    klass->stream_on = cam_unit_default_stream_on;
    klass->stream_off = cam_unit_default_stream_off;

    klass->try_produce_frame = NULL;
    klass->get_fileno = cam_unit_default_get_fileno;
    klass->get_next_event_time = cam_unit_default_get_next_event_time;

    klass->on_input_frame_ready = NULL;

    klass->draw_gl_init = cam_unit_default_draw_gl_init;
    klass->draw_gl = cam_unit_default_draw_gl;
    klass->draw_gl_shutdown = cam_unit_default_draw_gl_shutdown;

    klass->try_set_control = NULL;

    // signals

    /**
     * CamUnit::control-value-changed
     * @unit: the CamUnit emitting the signal
     * @control: the affected CamUnitControl 
     *
     * The control-value-changed signal is emitted when a CamUnitControl of the
     * unit takes on a new value
     */
    cam_unit_signals[CONTROL_VALUE_CHANGED_SIGNAL] = 
        g_signal_new("control-value-changed",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_CONTROL);
    /**
     * CamUnit::control-parameters-changed
     * @unit: the CamUnit emitting the signal
     * @control: the affected CamUnitControl 
     *
     * The control-parameters-changed signal is emitted when a CamUnitControl of
     * the unit has its min, max, or enabled property changed.
     */
    cam_unit_signals[CONTROL_PARAMETERS_CHANGED_SIGNAL] = 
        g_signal_new("control-parameters-changed",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_CONTROL);
    /**
     * CamUnit::output-formats-changed
     * @unit: the CamUnit emitting the signal
     *
     * The control-value-changed signal is emitted when the list of selectable
     * output formats changes
     */
    cam_unit_signals[OUTPUT_FORMATS_CHANGED_SIGNAL] = 
        g_signal_new("output-formats-changed",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
    /**
     * CamUnit::status-changed
     * @unit: the CamUnit emitting the signal
     * @old_status: the previous status of the unit.  The new status can be
     *              obtained via cam_unit_get_status
     *
     * The status-changed signal is emitted when the unit's status changes.
     */
    cam_unit_signals[STATUS_CHANGED_SIGNAL] = 
        g_signal_new("status-changed",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__INT,
                G_TYPE_NONE, 1, G_TYPE_INT);
    /**
     * CamUnit::input-changed
     * @unit: the CamUnit emitting the signal
     *
     * The input-changed signal is emitted when the input unit changes.
     * Specifically, when cam_unit_set_input is called.
     */
    cam_unit_signals[INPUT_CHANGED_SIGNAL] = 
        g_signal_new("input-changed",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, CAM_TYPE_UNIT);
    /**
     * CamUnit::input-format-changed
     * @unit: the CamUnit emitting the signal
     *
     * The input-changed signal is emitted when the input unit changes, and
     * when the input-unit is initialized.
     */
    cam_unit_signals[INPUT_FORMAT_CHANGED_SIGNAL] = 
        g_signal_new("input-format-changed",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, CAM_TYPE_UNIT_FORMAT);
    /**
     * CamUnit::frame-ready
     * @unit: the CamUnit emitting the signal
     * @fbuf: the FrameBuffer containing the actual frame data
     * @fmt:  the CamUnitFormat describing the frame data
     *
     * The frame-ready signal is emitted when the unit generates a new frame
     * (i.e. when the unit calls cam_unit_produce_frame) .
     */
    cam_unit_signals[FRAME_READY_SIGNAL] = 
        g_signal_new("frame-ready",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
                G_TYPE_NONE, 2, 
                CAM_TYPE_FRAMEBUFFER,
                CAM_TYPE_UNIT_FORMAT);
}

CamUnitStatus
cam_unit_get_status (const CamUnit * self) { return self->status; }

const char *
cam_unit_get_name (const CamUnit *self) { return self->name; }

const char *
cam_unit_get_id (const CamUnit *self) { return self->unit_id; }

uint32_t
cam_unit_get_flags (const CamUnit *self) { return self->flags; }

GList * 
cam_unit_list_controls(CamUnit * self)
{ return g_list_copy (self->controls_list); }

const CamUnitFormat* 
cam_unit_get_output_format(CamUnit *self) { return self->fmt; }

GList * 
cam_unit_get_output_formats(CamUnit *self)
{ return g_list_copy (self->output_formats); }

int
cam_unit_set_input (CamUnit * self, CamUnit * input)
{ 
    dbg(DBG_UNIT, "setting input of %s to %s\n", self->name, 
            input ? input->name : "<NULL>");

    if (self->status != CAM_UNIT_STATUS_IDLE) {
        err("Unit: refusing to set unit input when not in IDLE state. (%d)\n",
                self->status);
        return -1;
    }

    // disengage from the previous input unit, if it exists
    if (self->input_unit) {
        g_signal_handlers_disconnect_by_func (self->input_unit, 
                on_input_unit_status_changed, self);
        g_signal_handlers_disconnect_by_func (self->input_unit,
                on_input_frame_ready, self);
        g_object_unref (self->input_unit);
        self->input_unit = NULL;
    }

    // reference and connect to the new input unit
    self->input_unit = input;
    if (input) {
        g_object_ref (input);
        g_signal_connect (G_OBJECT (self->input_unit), "status-changed",
                G_CALLBACK (on_input_unit_status_changed), self);
        g_signal_connect (G_OBJECT (self->input_unit), "frame-ready",
                G_CALLBACK (on_input_frame_ready), self);
    }

    g_signal_emit (G_OBJECT(self), cam_unit_signals[INPUT_CHANGED_SIGNAL], 0,
            input);

    if (! input) {
        g_signal_emit (G_OBJECT (self), 
                cam_unit_signals[INPUT_FORMAT_CHANGED_SIGNAL], 0,
                NULL);
        return 0;
    }

    const CamUnitFormat *infmt = cam_unit_get_output_format (self->input_unit);
    g_signal_emit (G_OBJECT (self), 
            cam_unit_signals[INPUT_FORMAT_CHANGED_SIGNAL], 0,
            infmt);

    return 0;
}

static void
on_input_unit_status_changed (CamUnit *input_unit, int old_status,
        void *user_data)
{
    CamUnit *self = CAM_UNIT (user_data);
    dbg (DBG_UNIT, "[%s] input unit status changed (%s)\n", self->unit_id,
            cam_unit_status_to_str (input_unit->status));
    // if the input unit was just initialized, then its frame format
    // may have changed, so emit a signal
    // if it's set.
    if (old_status == CAM_UNIT_STATUS_IDLE &&
        input_unit->status == CAM_UNIT_STATUS_READY) {
        const CamUnitFormat *infmt = 
            cam_unit_get_output_format (self->input_unit);
        if (infmt) {
            g_signal_emit (G_OBJECT (self), 
                    cam_unit_signals[INPUT_FORMAT_CHANGED_SIGNAL], 0,
                    infmt);
        }
    } else if (input_unit->status == CAM_UNIT_STATUS_IDLE) {
        if (self->status == CAM_UNIT_STATUS_STREAMING) {
            cam_unit_stream_off (self);
            cam_unit_stream_shutdown (self);
        } else if (self->status == CAM_UNIT_STATUS_READY) {
            cam_unit_stream_shutdown (self);
        }
        g_signal_emit (G_OBJECT (self), 
                cam_unit_signals[INPUT_FORMAT_CHANGED_SIGNAL], 0,
                NULL);
    }
}

static void
on_input_frame_ready (CamUnit *input_unit, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt, void *user_data)
{
    CamUnit *self = CAM_UNIT (user_data);
    CamUnitClass *klass = CAM_UNIT_GET_CLASS (self);
    if (klass->on_input_frame_ready && 
            self->status == CAM_UNIT_STATUS_STREAMING) {
        klass->on_input_frame_ready (self, inbuf, infmt);
    }
}

static CamUnitFormat *
find_output_format (CamUnit *self, const CamUnitFormat *format)
{
    GList *fiter;
    for (fiter=self->output_formats; fiter; fiter=fiter->next) {
        CamUnitFormat *mfmt = CAM_UNIT_FORMAT(fiter->data);
        if (cam_unit_format_equals (format, mfmt)) return mfmt;
    }
    return NULL;
}

int
cam_unit_stream_init (CamUnit * self, const CamUnitFormat *format)
{ 
    if (self->status == CAM_UNIT_STATUS_READY) return 0;
    if (self->status != CAM_UNIT_STATUS_IDLE) {
        err("Unit: refusing to init non idle unit %s (%d)\n",
                self->unit_id, self->status);
        return -1;
    }

    if (! format) {
        err("Unit: [%s] cannot initialize without a format\n", self->unit_id);
        return -1;
    }

    // check that the format belongs to this unit
    self->fmt = find_output_format (self, format);
    if (! self->fmt) {
        err("Unit: [%s] refusing to init with an unrecognized format\n",
                self->unit_id);
        return -1;
    }
    dbg(DBG_UNIT, "[%s] default stream init [%s]\n",
            self->unit_id, self->fmt->name);
    int status = CAM_UNIT_GET_CLASS (self)->stream_init (self, format); 

    if (0 == status) {
        cam_unit_set_status (self, CAM_UNIT_STATUS_READY);
        return 0;
    } else {
        return -1;
    }
}

int
cam_unit_stream_on (CamUnit * self)
{ 
    if (self->status == CAM_UNIT_STATUS_STREAMING) return 0;
    if (self->status != CAM_UNIT_STATUS_READY) {
        err("Unit: cannot stream_on.  status is not READY\n");
        return -1;
    }
    if (0 == CAM_UNIT_GET_CLASS (self)->stream_on (self)) {
        cam_unit_set_status (self, CAM_UNIT_STATUS_STREAMING);
        return 0;
    } else {
        return -1;
    }
}

int
cam_unit_stream_off (CamUnit * self)
{ 
    if (self->status == CAM_UNIT_STATUS_READY) return 0;
    if (self->status != CAM_UNIT_STATUS_STREAMING) {
        err("Unit: cannot stream_off.  status is not STREAMING\n");
        return -1;
    }
    if (0 == CAM_UNIT_GET_CLASS (self)->stream_off (self)) {
        cam_unit_set_status (self, CAM_UNIT_STATUS_READY);
        return 0;
    } else { 
        return -1;
    }
}

int
cam_unit_stream_shutdown (CamUnit * self)
{ 
    if (self->status == CAM_UNIT_STATUS_IDLE) return 0;
    if (self->status == CAM_UNIT_STATUS_STREAMING) {
        if (0 != cam_unit_stream_off (self)) {
            return -1;
        }
    }
    if (self->status != CAM_UNIT_STATUS_READY) {
        err("Unit: cannot shut down a unit that is not READY\n");
        return -1;
    }
    if (0 == CAM_UNIT_GET_CLASS (self)->stream_shutdown (self)) {
        cam_unit_set_status (self, CAM_UNIT_STATUS_IDLE);
        self->fmt = NULL;
        return 0;
    } else {
        return -1;
    }
}

int 
cam_unit_get_fileno(CamUnit *self)
{ return CAM_UNIT_GET_CLASS (self)->get_fileno(self); }

int64_t 
cam_unit_get_next_event_time(CamUnit *self)
{ return CAM_UNIT_GET_CLASS (self)->get_next_event_time(self); }

int 
cam_unit_draw_gl_init (CamUnit * self)
{ return CAM_UNIT_GET_CLASS (self)->draw_gl_init(self); }

int 
cam_unit_draw_gl (CamUnit *self)
{ return CAM_UNIT_GET_CLASS (self)->draw_gl(self); }

int 
cam_unit_draw_gl_shutdown (CamUnit * self)
{ return CAM_UNIT_GET_CLASS (self)->draw_gl_shutdown(self); }

static int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

gboolean
cam_unit_try_produce_frame (CamUnit *self, int timeout_ms)
{ 
    CamUnitClass *klass = CAM_UNIT_GET_CLASS (self);
    if (!klass->try_produce_frame) {
        g_warning ("Unit does not provide try_produce_frame\n");
        return FALSE;
    }
    if (self->status != CAM_UNIT_STATUS_STREAMING) {
        g_warning ("Unit is not streaming!");
        return FALSE;
    }

    if (self->flags & CAM_UNIT_EVENT_METHOD_FD) {
        // special case: don't call poll if no timeout
        if (0 == timeout_ms) return klass->try_produce_frame (self); 

        int fd = cam_unit_get_fileno  (self);
        struct pollfd pfd = { fd, POLLIN, 0 };
        int status = 0;
        do {
            status = poll (&pfd, 1, timeout_ms);
        } while (status < 0 && errno == EINTR);

        if (status == 1 && pfd.revents & POLLIN) {
            return klass->try_produce_frame (self); 
        }

        return FALSE;
    } else if (self->flags & CAM_UNIT_EVENT_METHOD_TIMEOUT) {
        int64_t next_evt_time = cam_unit_get_next_event_time (self);
        int64_t now = _timestamp_now ();

        if (now >= next_evt_time || timeout_ms == 0) {
            // unit reports that it's ready, or we're not willing to wait.
            return klass->try_produce_frame (self); 
        }

        int64_t wait_usec = next_evt_time - now;
        if (wait_usec > timeout_ms * 1000) wait_usec = timeout_ms * 1000;

        // sleep until the time that the unit reports
        struct timespec ts = { wait_usec / 1000000, wait_usec % 1000000 };
        struct timespec rem;
        int status = 0;
        do {
            status = nanosleep (&ts, &rem);
            ts = rem;
        } while (status == -1 && EINTR == errno);
        if (0 != status) {
            return FALSE;
        }
        return klass->try_produce_frame (self);
    } else {
        g_warning ("Badly configured unit!  invalid flags!");
        return FALSE;
    }
}

static int cam_unit_default_stream_init (CamUnit *self, 
        const CamUnitFormat *format) { return 0; }
static int cam_unit_default_stream_shutdown (CamUnit *self) { return 0; }
static int cam_unit_default_stream_on (CamUnit *self) { return 0; }
static int cam_unit_default_stream_off (CamUnit *self) { return 0; }

static int cam_unit_default_get_fileno (CamUnit *self) { return -1; }

static int64_t
cam_unit_default_get_next_event_time (CamUnit *self) { return -1; }

static int cam_unit_default_draw_gl_init (CamUnit * self) { return -1; }
static int cam_unit_default_draw_gl (CamUnit *self) { return -1; }
static int cam_unit_default_draw_gl_shutdown (CamUnit * self) { return -1; }

// ===========

CamUnit * 
cam_unit_get_input (CamUnit *self) { return self->input_unit; }

int
cam_unit_stream_init_any_format (CamUnit *self)
{
    if (! self->output_formats) return -1;

    const CamUnitFormat *fmt = NULL;
    int best_score = 0;

    // check that the preferred format is still valid
    for (GList *fiter=self->output_formats; fiter; fiter=fiter->next) {
        CamUnitFormat *cfmt = CAM_UNIT_FORMAT(fiter->data);
        int score = 0;
        if (self->requested_pixelformat != CAM_PIXEL_FORMAT_INVALID &&
                self->requested_pixelformat == cfmt->pixelformat) {
            score += 3;
        }
        if (self->requested_width > 0 && 
                self->requested_width == cfmt->width) {
            score ++;
        }
        if (self->requested_height > 0 &&
                self->requested_height == cfmt->height) {
            score ++;
        }
        if (!fmt || score > best_score) {
            best_score = score;
            fmt = cfmt;
        }
    }

    cam_unit_stream_init (self, fmt);
    return 0;
}

int 
cam_unit_set_preferred_format (CamUnit *self, 
        CamPixelFormat pixelformat, int width, int height)
{
    self->requested_pixelformat = pixelformat;
    self->requested_width = width;
    self->requested_height = height;
    return 0;
}

// ============= non-virtual protected methods =============
void
cam_unit_set_name (CamUnit *self, const char *name)
{
    if (self->name) free (self->name);
    self->name = strdup (name);
}

void
cam_unit_set_id (CamUnit *self, const char *unit_id)
{
    if (self->unit_id) free (self->unit_id);
    self->unit_id = strdup (unit_id);
}

/*
 * control_callback:
 *
 * When the user tries to set a control value, this function will be invoked.
 * If the implementing unit has overriden the try_set_control method, then
 * that will be invoked to see if the proposed value is acceptable.  Otherwise, 
 * the default action is to simply say that proposed value is acceptable.
 */
static gboolean
control_callback (const CamUnitControl *ctl, const GValue *proposed, 
        GValue *actual, void *user_data)
{
    CamUnit *self = CAM_UNIT(user_data);
    CamUnitClass *klass = CAM_UNIT_GET_CLASS (self);
    if (klass->try_set_control) {
        return klass->try_set_control (self, ctl, proposed, actual);
    } else {
        g_value_copy (proposed, actual);
        return TRUE;
    }
}

static void
on_control_value_changed (CamUnitControl *ctl, CamUnit *self)
{
    g_signal_emit (G_OBJECT(self), 
            cam_unit_signals[CONTROL_VALUE_CHANGED_SIGNAL], 0, ctl);
}

static void
on_control_parameters_changed (CamUnitControl *ctl, CamUnit *self)
{
    g_signal_emit (G_OBJECT(self),
            cam_unit_signals[CONTROL_PARAMETERS_CHANGED_SIGNAL], 0, ctl);
}

static CamUnitControl*
try_add_control (CamUnit *self, CamUnitControl *new_ctl)
{
    if (!new_ctl) {
        dbg (DBG_UNIT, "failed to create new control\n");
        return NULL;
    }
    CamUnitControl *oldctl = cam_unit_find_control (self, new_ctl->name);
    if (oldctl) {
        err("WARNING:  Refusing to replace existing control [%s]\n"
            "          with new control [%s]\n",
            oldctl->name, new_ctl->name);
        g_object_unref (new_ctl);
        return NULL;
    }
    self->controls_list = g_list_append (self->controls_list, new_ctl);
    g_hash_table_insert (self->controls, new_ctl->id, new_ctl);

    cam_unit_control_set_callback (new_ctl, control_callback, self);
    g_signal_connect (G_OBJECT(new_ctl), "value-changed",
            G_CALLBACK(on_control_value_changed), self);
    g_signal_connect (G_OBJECT(new_ctl), "parameters-changed",
            G_CALLBACK(on_control_parameters_changed), self);
    return new_ctl;
}

CamUnitControl*
cam_unit_add_control_enum (CamUnit *self, const char *id, const char *name, 
        int default_index, int enabled,
        const char **menu_entries, const int * entries_enabled)
{
    CamUnitControl *ctl = cam_unit_control_new_enum (id, name, 
            default_index, enabled, menu_entries, entries_enabled);
    return try_add_control (self, ctl);
}
CamUnitControl* 
cam_unit_add_control_int (CamUnit *self, const char *id,
        const char *name, int min, int max, int step, int default_val,
        int enabled)
{
    CamUnitControl *ctl = cam_unit_control_new_int (id, name,
            min, max, step, default_val, enabled);
    return try_add_control (self, ctl);
}
CamUnitControl* 
cam_unit_add_control_float (CamUnit *self, const char *id,
        const char *name, float min, float max, float step, float default_val,
        int enabled)
{
    CamUnitControl *ctl = cam_unit_control_new_float (id, name,
            min, max, step, default_val, enabled);
    return try_add_control (self, ctl);
}
CamUnitControl* 
cam_unit_add_control_boolean (CamUnit *self, const char *id,
        const char *name, int default_val, int enabled)
{
    CamUnitControl *ctl = cam_unit_control_new_boolean (id, name,
            default_val, enabled);
    return try_add_control (self, ctl);
}
CamUnitControl*
cam_unit_add_control_string (CamUnit *self, const char *id,
        const char *name, const char * default_val, int enabled)
{
    CamUnitControl *ctl = cam_unit_control_new_string (id, name,
            default_val, enabled);
    return try_add_control (self, ctl);
}

CamUnitControl* 
cam_unit_find_control (CamUnit *self, const char *id)
{
    return g_hash_table_lookup (self->controls, id);
}

#define DEFINE_SET_CONTROL_FUNC(type,arg) \
gboolean \
cam_unit_set_control_##type (CamUnit *self, const char *id, arg val)\
{\
    CamUnitControl *ctl = cam_unit_find_control (self, id);\
    if (!ctl) return FALSE;\
    return cam_unit_control_try_set_##type (ctl, val);\
}

DEFINE_SET_CONTROL_FUNC (int, int)
DEFINE_SET_CONTROL_FUNC (float, float)
DEFINE_SET_CONTROL_FUNC (enum, int)
DEFINE_SET_CONTROL_FUNC (boolean, int)
DEFINE_SET_CONTROL_FUNC (string, const char *)

#undef DEFINE_SET_CONTROL_FUNC

#define DEFINE_GET_CONTROL_FUNC(type,arg) \
gboolean \
cam_unit_get_control_##type (CamUnit *self, const char *id, arg *val) \
{ \
    CamUnitControl *ctl = cam_unit_find_control (self, id); \
    if (!ctl) return FALSE; \
    *val = cam_unit_control_get_##type (ctl); \
    return TRUE; \
}

DEFINE_GET_CONTROL_FUNC (int, int)
DEFINE_GET_CONTROL_FUNC (float, float)
DEFINE_GET_CONTROL_FUNC (enum, int)
DEFINE_GET_CONTROL_FUNC (boolean, int)

#undef DEFINE_GET_CONTROL_FUNC

gboolean 
cam_unit_get_control_string (CamUnit *self, const char *id, 
        char **val)
{
    CamUnitControl *ctl = cam_unit_find_control (self, id);
    if (!ctl) return FALSE;
    *val = strdup (cam_unit_control_get_string (ctl));
    return TRUE;
}

CamUnitFormat *
cam_unit_add_output_format_full (CamUnit *self, CamPixelFormat pfmt, 
        const char *name, 
        int width, int height, int row_stride, int max_data_size)
{
    if (! name) {
        char buf[128];
        sprintf(buf, "%dx%d %s", width, height, cam_pixel_format_str(pfmt));
        name = buf;
    }

    if (width <= 0 || height <= 0) {
        err("CamUnit: %dx%d is not a valid output format size (unit %s)\n",
                width, height, self->unit_id);
        return NULL;
    }
    CamUnitFormat *new_format = cam_unit_format_new (pfmt, name, 
            width, height, row_stride, max_data_size);
    self->output_formats = g_list_append (self->output_formats, new_format);

    dbg(DBG_UNIT, "[%s] adding output format [%s] %p\n", self->unit_id, name,
            new_format);

    g_signal_emit (G_OBJECT(self), 
            cam_unit_signals[OUTPUT_FORMATS_CHANGED_SIGNAL], 0);

    return new_format;
}

void 
cam_unit_remove_output_format (CamUnit *self, CamUnitFormat *fmt)
{
    CamUnitFormat *mfmt = find_output_format(self,fmt);
    if (mfmt) {
        self->output_formats = g_list_remove (self->output_formats, mfmt);
        g_object_unref (mfmt);
        g_signal_emit (G_OBJECT(self), 
                cam_unit_signals[OUTPUT_FORMATS_CHANGED_SIGNAL], 0);
    }
}

void 
cam_unit_remove_all_output_formats (CamUnit *self)
{
    dbg (DBG_UNIT, "[%s] removing all output formats\n", self->unit_id);
    for (GList *ofiter=self->output_formats; ofiter; ofiter=ofiter->next) {
        g_object_unref (ofiter->data);
    }
    g_list_free (self->output_formats);
    self->output_formats = NULL;
    self->fmt = NULL;
    g_signal_emit (G_OBJECT(self), 
            cam_unit_signals[OUTPUT_FORMATS_CHANGED_SIGNAL], 0);
}

void 
cam_unit_set_status (CamUnit *self, CamUnitStatus newstatus)
{
    if (newstatus != self->status) {
        int oldstatus = self->status;
        self->status = newstatus;
        g_signal_emit (G_OBJECT(self),
                cam_unit_signals[STATUS_CHANGED_SIGNAL], 0, oldstatus);
    }
}

void
cam_unit_produce_frame (CamUnit *self, const CamFrameBuffer *buffer,
        const CamUnitFormat *fmt)
{
    g_signal_emit (G_OBJECT (self),
            cam_unit_signals[FRAME_READY_SIGNAL], 0, buffer, fmt);
}

// ============ utility functions ============
const char *
cam_unit_status_to_str (CamUnitStatus status)
{
    char *a[] = {
        "Idle",
        "Ready",
        "Streaming",
        "INVALID"
    };
    if ((status < 0) || (status >= CAM_UNIT_STATUS_MAX)) return NULL;
    return a[status];
}
