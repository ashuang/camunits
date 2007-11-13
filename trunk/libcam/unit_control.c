#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "unit_control.h"

#include "dbg.h"

#define err(args...) fprintf (stderr, args)

enum {
    VALUE_CHANGED_SIGNAL,
    PARAMS_CHANGED_SIGNAL,
    LAST_SIGNAL
};

static guint cam_unit_control_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_control_finalize (GObject *obj);
static void cam_unit_control_init (CamUnitControl *self);
static void cam_unit_control_class_init (CamUnitControlClass *klass);

G_DEFINE_TYPE (CamUnitControl, cam_unit_control, G_TYPE_OBJECT);

static void
cam_unit_control_init (CamUnitControl *self)
{
    self->type = 0;
    self->id = NULL;
    self->name = NULL;
    self->enabled = 0;
    self->try_set_function = NULL;
    self->user_data = NULL;

    self->max_int = 0;
    self->min_int = 0;
    self->step_int = 0;

    self->enum_entries = NULL;
    self->enum_entries_enabled = NULL;

    memset (&self->val, 0, sizeof (self->val));
    memset (&self->initial_val, 0, sizeof (self->initial_val));
}

static void
cam_unit_control_class_init (CamUnitControlClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_control_finalize;

    /**
     * CamUnitControl::value-changed
     * @unit: the CamUnit emitting the signal
     *
     * The value-changed signal is emitted when the value of the control is
     * changed.  This will always happen when any of cam_unit_control_force_set 
     * functions are called, and may happen when the cam_unit_control_try_set
     * functions are called
     */
    cam_unit_control_signals[VALUE_CHANGED_SIGNAL] = 
        g_signal_new ("value-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    /**
     * CamUnitControl::parameters-changed
     * @unit: the CamUnit emitting the signal
     *
     * The parameters-changed signal is emitted when properties of a
     * control are changed, such as min, max, step, or enabled by
     * cam_unit_control_set_enabled (), cam_unit_control_modify_int (),
     * or cam_unit_control_modify_float ().
     */
    cam_unit_control_signals[PARAMS_CHANGED_SIGNAL] = 
        g_signal_new ("parameters-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

static void
cam_unit_control_finalize (GObject *obj)
{
    CamUnitControl *self = CAM_UNIT_CONTROL (obj);

    free (self->name);
    free (self->id);
    if (self->enum_entries) {
        int i;
        for (i=0; i <= self->max_int; i++) {
            free (self->enum_entries[i]);
        }
    }
    free (self->enum_entries);
    free (self->enum_entries_enabled);

    g_value_unset (&self->val);
    g_value_unset (&self->initial_val);

    G_OBJECT_CLASS (cam_unit_control_parent_class)->finalize (obj);
}

static CamUnitControl *
cam_unit_control_new_basic (const char *id, const char *name, 
        int type, int enabled)
{
    dbg (DBG_CONTROL, "new unit control [%s]\n", name);
    CamUnitControl *self = g_object_new (CAM_TYPE_UNIT_CONTROL, NULL);
    self->type = type;
    self->id = strdup (id);
    self->name = strdup (name);
    self->enabled = enabled;
    return self;
}

CamUnitControl * 
cam_unit_control_new_enum (const char *id,
        const char *name, int initial_index, int enabled,
        const char **entries, const int * entries_enabled)
{
    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_ENUM, enabled);
    // how many entries?
    int nentries;
    for (nentries = 0; entries[nentries]; nentries++);
    self->max_int = nentries - 1;
    self->min_int = 0;
    self->enum_entries = (char**) malloc ((self->max_int+1)*sizeof (char*));
    int i;
    for (i=0; i<=self->max_int; i++) {
        self->enum_entries[i] = strdup (entries[i]);
    }
    if (entries_enabled) {
        self->enum_entries_enabled = (int*) malloc (nentries*sizeof (int));
        memcpy (self->enum_entries_enabled, entries_enabled,
                nentries * sizeof (int));
    }
    g_value_init (&self->val, G_TYPE_INT);
    g_value_set_int (&self->val, initial_index);
    g_value_init (&self->initial_val, G_TYPE_INT);
    g_value_set_int (&self->initial_val, initial_index);

    return self;
}

void
cam_unit_control_modify_enum (CamUnitControl * self,
        int enabled, const char ** entries, const int * entries_enabled)
{
    int i;
    for (i = 0; i <= self->max_int; i++)
        free (self->enum_entries[i]);
    free (self->enum_entries);
    free (self->enum_entries_enabled);
    self->enum_entries_enabled = NULL;

    int nentries;
    for (nentries = 0; entries[nentries]; nentries++);
    self->max_int = nentries - 1;
    self->min_int = 0;
    self->enum_entries = (char**) malloc ((self->max_int+1)*sizeof (char*));
    for (i=0; i<=self->max_int; i++) {
        self->enum_entries[i] = strdup (entries[i]);
    }
    if (entries_enabled) {
        self->enum_entries_enabled = (int*) malloc (nentries*sizeof (int));
        memcpy (self->enum_entries_enabled, entries_enabled,
                nentries * sizeof (int));
    }
    self->enabled = enabled;
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
}

CamUnitControl * 
cam_unit_control_new_int (const char *id, const char *name, 
        int min, int max, int step, int initial_val, int enabled)
{
    dbg (DBG_CONTROL, "[%s] - <%d, %d> step %d initial %d enabled %d\n",
            name, min, max, step, initial_val, enabled);
    if (min >= max || initial_val < min || initial_val > max || step == 0) {
        dbg (DBG_CONTROL, "refusing to create integer control\n");
        return NULL;
    }
    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_INT, enabled);

    self->min_int = min;
    self->max_int = max;
    self->step_int = step;
    g_value_init (&self->val, G_TYPE_INT);
    g_value_set_int (&self->val, initial_val);
    g_value_init (&self->initial_val, G_TYPE_INT);
    g_value_set_int (&self->initial_val, initial_val);
    return self;
}

void
cam_unit_control_modify_int (CamUnitControl * self,
        int min, int max, int step, int enabled)
{
    dbg (DBG_CONTROL, "[%s] - <%d, %d> step %d enabled %d\n",
            self->name, min, max, step, enabled);
    self->min_int = min;
    self->max_int = max;
    self->step_int = step;
    self->enabled = enabled;
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
}

static void
num_chars_float (float x, int sf, int * width, int * prec)
{
    int i = floor (log10 (x) + 1.0);
    int j = i - sf;
    if (i <= 0) {
        *width = -j + 2;
        *prec = -j;
    }
    else if (i > 0 && j < 0) {
        *width = i - j + 1;
        *prec = -j;
    }
    else {
        *width = i;
        *prec = 0;
    }
}

CamUnitControl * 
cam_unit_control_new_float (const char *id, const char *name, 
        float min, float max, float step, float initial_val, int enabled)
{
    dbg (DBG_CONTROL, "[%s] - <%f, %f> step %f initial %f enabled %d\n",
            name, min, max, step, initial_val, enabled);
    if (min >= max || initial_val < min || initial_val > max || step == 0) {
        dbg (DBG_CONTROL, "refusing to create integer control\n");
        return NULL;
    }
    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_FLOAT, enabled);

    self->min_float = min;
    self->max_float = max;
    self->step_float = step;
    g_value_init (&self->val, G_TYPE_FLOAT);
    g_value_set_float (&self->val, initial_val);
    g_value_init (&self->initial_val, G_TYPE_FLOAT);
    g_value_set_float (&self->initial_val, initial_val);

    num_chars_float (self->max_float - self->min_float, 3,
            &self->display_width, &self->display_prec);
    return self;
}

void
cam_unit_control_modify_float (CamUnitControl * self,
        float min, float max, float step, int enabled)
{
    self->min_float = min;
    self->max_float = max;
    self->step_float = step;
    self->enabled = enabled;
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
}


CamUnitControl * 
cam_unit_control_new_boolean (const char *id, const char *name,
        int initial_val, int enabled)
{
    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_BOOLEAN, enabled);
    dbg (DBG_CONTROL, "[%s] - initial %d enabled %d\n",
            self->name, initial_val, enabled);

    self->min_int = 0;
    self->max_int = 1;
    self->step_int = 1;
    g_value_init (&self->val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&self->val, initial_val);
    g_value_init (&self->initial_val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&self->initial_val, initial_val);
    return self;
}

CamUnitControl * 
cam_unit_control_new_string (const char *id, const char *name,
        const char *initial_val, int enabled)
{
    CamUnitControl *self = cam_unit_control_new_basic (id, name,
            CAM_UNIT_CONTROL_TYPE_STRING, enabled);
    g_value_init (&self->val, G_TYPE_STRING);
    g_value_set_string (&self->val, initial_val);
    g_value_init (&self->initial_val, G_TYPE_STRING);
    g_value_set_string (&self->initial_val, initial_val);
    return self;
}

void 
cam_unit_control_set_callback (CamUnitControl *self,
        CamUnitControlCallback callback, void *user_data)
{
    self->try_set_function = callback;
    self->user_data = user_data;
}

static int
check_type (CamUnitControl *self, const GValue *value)
{
    GType expected_type = G_TYPE_INVALID;

    switch (self->type) {
        case CAM_UNIT_CONTROL_TYPE_ENUM:
        case CAM_UNIT_CONTROL_TYPE_INT:
            expected_type = G_TYPE_INT;
            break;
        case CAM_UNIT_CONTROL_TYPE_FLOAT:
            expected_type = G_TYPE_FLOAT;
            break;
        case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
            expected_type = G_TYPE_BOOLEAN;
            break;
        case CAM_UNIT_CONTROL_TYPE_STRING:
            expected_type = G_TYPE_STRING;
            break;
        default:
            err ("UnitControl: unrecognized type %s\n", 
                    g_type_name (G_VALUE_TYPE (value)));
            return 0;
    }

    if (expected_type == G_TYPE_INVALID ||
        expected_type != G_VALUE_TYPE (value)) {
        err ("UnitControl: [%s] expected %s, got %s\n",
                self->name, 
                g_type_name (expected_type),
                g_type_name (G_VALUE_TYPE (value)));
        return 0;
    }
    return 1;
}

// ============ force set ============
int
cam_unit_control_force_set_val (CamUnitControl *self, const GValue *value)
{
    if (! check_type (self, value)) return -1;
    g_value_copy (value, &self->val);
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[VALUE_CHANGED_SIGNAL], 0);
    return 0;
}

int 
cam_unit_control_force_set_int (CamUnitControl *self, int val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_INT);
    g_value_set_int (&gv, val);
    int result = cam_unit_control_force_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_force_set_float (CamUnitControl *self, float val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_FLOAT);
    g_value_set_float (&gv, val);
    int result = cam_unit_control_force_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_force_set_enum (CamUnitControl *self, int val)
{
    return cam_unit_control_force_set_int (self, val);
}

int 
cam_unit_control_force_set_boolean (CamUnitControl *self, int val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_BOOLEAN);
    g_value_set_boolean (&gv, val);
    int result = cam_unit_control_force_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_force_set_string (CamUnitControl *self, const char *val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_STRING);
    g_value_set_string (&gv, val);
    int result = cam_unit_control_force_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

// ============ try set ============
int
cam_unit_control_try_set_val (CamUnitControl *self, const GValue *value)
{
    if (! check_type (self, value)) return -1;

    // check if something (i.e. a CamUnit) has asked for veto power over
    // setting the control value
    if (self->try_set_function) {
        GValue av = { 0, };
        g_value_init (&av, G_VALUE_TYPE (value));
        if (self->try_set_function (self, value, &av, self->user_data)) {
            g_value_copy (&av, &self->val);
            g_value_unset (&av);
        } else {
            g_value_unset (&av);
            return -1;
        }
    } else {
        g_value_copy (value, &self->val);
    }

    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[VALUE_CHANGED_SIGNAL], 0);
    
    return 0;
}

int 
cam_unit_control_try_set_int (CamUnitControl *self, int val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_INT);
    g_value_set_int (&gv, val);
    int result = cam_unit_control_try_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_try_set_float (CamUnitControl *self, float val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_FLOAT);
    g_value_set_float (&gv, val);
    int result = cam_unit_control_try_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_try_set_enum (CamUnitControl *self, int index)
{
    return cam_unit_control_try_set_int (self, index);
}

int 
cam_unit_control_try_set_boolean (CamUnitControl *self, int val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_BOOLEAN);
    g_value_set_boolean (&gv, val);
    int result = cam_unit_control_try_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_try_set_string (CamUnitControl *self, const char *val)
{
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_STRING);
    g_value_set_string (&gv, val);
    int result = cam_unit_control_try_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

// ============== get ==============
void
cam_unit_control_get_val (const CamUnitControl *self, GValue *value)
{
    g_value_init (value, G_VALUE_TYPE (&self->val));
    g_value_copy (&self->val, value);
}
int 
cam_unit_control_get_int (const CamUnitControl *self)
{
    return g_value_get_int (&self->val);
}
float
cam_unit_control_get_float (const CamUnitControl *self)
{
    return g_value_get_float (&self->val);
}
int 
cam_unit_control_get_enum (const CamUnitControl *self)
{
    return g_value_get_int (&self->val);
}
int 
cam_unit_control_get_boolean (const CamUnitControl *self)
{
    return g_value_get_boolean (&self->val);
}
const char *
cam_unit_control_get_string (const CamUnitControl *self)
{
    return g_value_get_string (&self->val);
}

// =========
int 
cam_unit_control_get_max_int (const CamUnitControl *self)
{ return self->max_int; }
int 
cam_unit_control_get_min_int (const CamUnitControl *self)
{ return self->min_int; }
int 
cam_unit_control_get_step_int (const CamUnitControl *self)
{ return self->step_int; }

float 
cam_unit_control_get_max_float (const CamUnitControl *self)
{ return self->max_float; }
float
cam_unit_control_get_min_float (const CamUnitControl *self)
{ return self->min_float; }
float
cam_unit_control_get_step_float (const CamUnitControl *self)
{ return self->step_float; }

void 
cam_unit_control_set_enabled (CamUnitControl *self, int enabled)
{
    if (enabled != self->enabled) {
        self->enabled = enabled;
        g_signal_emit (G_OBJECT (self), 
                cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
    } 
}

int 
cam_unit_control_get_enabled (const CamUnitControl *self)
{
    return self->enabled;
}

void 
cam_unit_control_set_ui_hints (CamUnitControl *self, int flags)
{
    self->ui_hints = flags;
}

int 
cam_unit_control_get_ui_hints (const CamUnitControl *self)
{
    return self->ui_hints;
}
