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

typedef struct _CamUnitControlPriv CamUnitControlPriv;
struct _CamUnitControlPriv {
    CamUnitControlType type;
    char * id;
    char * name;

	/*< private >*/
    int enabled;

    CamUnitControlCallback try_set_function;
    void *user_data;

    CamUnitControlEnumValue *enum_values;
    int n_enum_values;

    int max_int;
    int min_int;
    int step_int;

    float max_float;
    float min_float;
    float step_float;
//    int display_width;
//    int display_prec;
    char * display_fmt;

    int ui_hints;

    GValue val;
    GValue initial_val;
};
#define CAM_UNIT_CONTROL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_UNIT_CONTROL, CamUnitControlPriv))

static const char *__control_type_names[] = {
    "invalid",
    "int",
    "boolean",
    "enum",
    "string",
    "float"
};

static guint cam_unit_control_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_control_finalize (GObject *obj);
static void cam_unit_control_init (CamUnitControl *self);
static void cam_unit_control_class_init (CamUnitControlClass *klass);

G_DEFINE_TYPE (CamUnitControl, cam_unit_control, G_TYPE_OBJECT);

static void
cam_unit_control_init (CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    priv->type = 0;
    priv->id = NULL;
    priv->name = NULL;
    priv->enabled = 0;
    priv->try_set_function = NULL;
    priv->user_data = NULL;

    priv->max_int = 0;
    priv->min_int = 0;
    priv->step_int = 0;

    priv->enum_values = NULL;
    priv->n_enum_values = 0;
//    priv->enum_entries = NULL;
//    priv->enum_entries_enabled = NULL;

    memset (&priv->val, 0, sizeof (priv->val));
    memset (&priv->initial_val, 0, sizeof (priv->initial_val));
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

    g_type_class_add_private (gobject_class, sizeof (CamUnitControlPriv));
}

static void
cam_unit_control_finalize (GObject *obj)
{
    CamUnitControl *self = CAM_UNIT_CONTROL (obj);
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);

    free (priv->name);
    free (priv->id);
    for(int i=0; i<priv->n_enum_values; i++) {
        free((char*)(priv->enum_values[i].nickname));
    }
    free(priv->enum_values);
    if(priv->display_fmt)
        g_free(priv->display_fmt);
#if 0
    if (priv->enum_entries) {
        int i;
        for (i=0; i <= priv->max_int; i++) {
            free (priv->enum_entries[i]);
        }
    }
    free (priv->enum_entries);
    free (priv->enum_entries_enabled);
#endif

    g_value_unset (&priv->val);
    g_value_unset (&priv->initial_val);

    G_OBJECT_CLASS (cam_unit_control_parent_class)->finalize (obj);
}

static inline int
warn_if_wrong_type(const CamUnitControl *self, CamUnitControlType correct_type)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    if(priv->type != correct_type) {
        g_warning("CamUnitControl is type %s not %s\n",
                __control_type_names[priv->type],
                __control_type_names[correct_type]);
        return 1;
    }
    return 0;
}

static CamUnitControl *
cam_unit_control_new_basic (const char *id, const char *name, 
        int type, int enabled)
{
    dbg (DBG_CONTROL, "new unit control [%s][%s]\n", id, name);
    CamUnitControl *self = g_object_new (CAM_TYPE_UNIT_CONTROL, NULL);
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    priv->type = type;
    priv->id = strdup (id);
    priv->name = strdup (name);
    priv->enabled = enabled;
    priv->display_fmt = NULL;
    return self;
}

CamUnitControl * 
cam_unit_control_new_enum (const char *id,
        const char *name, int initial_value, int enabled,
        const CamUnitControlEnumValue *entries)
{
    int found_initial_value = FALSE;
    int n_enum_values = 0;

    // count the number of values and do some basic consistency checks (make
    // sure initial value is a valid entry, no duplicate values)
    for(n_enum_values=0; entries[n_enum_values].nickname; n_enum_values++) {
        if(entries[n_enum_values].value == initial_value)
            found_initial_value = TRUE;

        for(int i=0; i<n_enum_values; i++) {
            if(entries[n_enum_values].value == entries[i].value) {
                g_warning("Duplicate enum value %d in control [%s]\n",
                        entries[i].value, id);
                return NULL;
            }
        }
    }
    if(!found_initial_value) {
        g_warning ("Initial value %d for enum control [%s] is not a valid value\n",
                initial_value, id);
        return NULL;
    }

    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_ENUM, enabled);
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    priv->n_enum_values = n_enum_values;

    priv->enum_values = (CamUnitControlEnumValue*) calloc(1, 
            (priv->n_enum_values+1) * sizeof(CamUnitControlEnumValue));
    for(int i=0; i<priv->n_enum_values; i++) {
        CamUnitControlEnumValue *ev = &priv->enum_values[i];
        ev->value    = entries[i].value;
        ev->nickname = strdup(entries[i].nickname);
        ev->enabled  = entries[i].enabled;
    }

    g_value_init (&priv->val, G_TYPE_INT);
    g_value_set_int (&priv->val, initial_value);
    g_value_init (&priv->initial_val, G_TYPE_INT);
    g_value_set_int (&priv->initial_val, initial_value);

    return self;
}

void 
cam_unit_control_modify_enum (CamUnitControl * self, int selected_value, int
        enabled, const CamUnitControlEnumValue *entries)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    if(priv->type != CAM_UNIT_CONTROL_TYPE_ENUM) {
        g_warning("Refusing invocation of %s on non-enum control [%s]\n",
                __FUNCTION__, priv->id);
        return;
    }

    int found_selected_value = FALSE;
    int n_enum_values = 0;

    // count the number of values and do some basic consistency checks (make
    // sure initial value is a valid entry, no duplicate values)
    for(n_enum_values=0; entries[n_enum_values].nickname; n_enum_values++) {
        if(entries[n_enum_values].value == selected_value)
            found_selected_value = TRUE;

        for(int i=0; i<n_enum_values; i++) {
            if(entries[n_enum_values].value == entries[i].value) {
                g_warning("Duplicate enum value %d in control [%s]\n",
                        entries[i].value, priv->id);
                return;
            }
        }
    }
    if(!found_selected_value) {
        g_warning("%s: Selected value %d for enum [%s] is not a valid\n",
                __FILE__, selected_value, priv->id);
        return;
    }

    for(int i=0; i<priv->n_enum_values; i++) {
        free((char*)(priv->enum_values[i].nickname));
    }
    free(priv->enum_values);

    priv->n_enum_values = n_enum_values;

    priv->enum_values = (CamUnitControlEnumValue*) calloc(1, 
            (priv->n_enum_values+1) * sizeof(CamUnitControlEnumValue));
    for(int i=0; i<priv->n_enum_values; i++) {
        CamUnitControlEnumValue *ev = &priv->enum_values[i];
        ev->value    = entries[i].value;
        ev->nickname = strdup(entries[i].nickname);
        ev->enabled  = entries[i].enabled;
    }

    g_value_init (&priv->initial_val, G_TYPE_INT);
    g_value_set_int (&priv->initial_val, selected_value);

    priv->enabled = enabled;
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
}

GList *
cam_unit_control_get_enum_entries(const CamUnitControl *self)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_ENUM))
        return NULL;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    GList *result = NULL;
    for(int i=priv->n_enum_values-1; i>=0; i--) {
        result = g_list_prepend(result, &priv->enum_values[i]);
    }
    return result;
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
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);

    priv->min_int = min;
    priv->max_int = max;
    priv->step_int = step;
    priv->display_fmt = g_strdup("%d");
    g_value_init (&priv->val, G_TYPE_INT);
    g_value_set_int (&priv->val, initial_val);
    g_value_init (&priv->initial_val, G_TYPE_INT);
    g_value_set_int (&priv->initial_val, initial_val);
    return self;
}

void
cam_unit_control_modify_int (CamUnitControl * self,
        int min, int max, int step, int enabled)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    dbg (DBG_CONTROL, "[%s] - <%d, %d> step %d enabled %d\n",
            priv->name, min, max, step, enabled);
    priv->min_int = min;
    priv->max_int = max;
    priv->step_int = step;
    priv->enabled = enabled;
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
}

static void
num_chars_float (float x, int sf, int * width, int * prec)
{
    int i = 1;
    int v = x;
    if(v > 1) {
        for(; v>0; v/=10)
            i++;
    } else if(v > 0) {
        for(; v<0; v*=10)
            i--;
    }
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

void
cam_unit_control_set_display_format(CamUnitControl *self, const char *fmt)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    if(priv->display_fmt)
        g_free(priv->display_fmt);
    priv->display_fmt = g_strdup(fmt);
}

char *
cam_unit_control_get_display_format(CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return g_strdup(priv->display_fmt);
}

CamUnitControl * 
cam_unit_control_new_float (const char *id, const char *name, 
        float min, float max, float step, float initial_val, int enabled)
{
    dbg (DBG_CONTROL, "[%s] - <%e, %e> step %f initial %f enabled %d\n",
            name, min, max, step, initial_val, enabled);
#if 0
    if (min >= max || initial_val < min || initial_val > max || step == 0) {
        dbg (DBG_CONTROL, "refusing to create integer control\n");
        return NULL;
    }
#endif
    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_FLOAT, enabled);
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);

    priv->min_float = min;
    priv->max_float = max;
    priv->step_float = step;
    g_value_init (&priv->val, G_TYPE_FLOAT);
    g_value_set_float (&priv->val, initial_val);
    g_value_init (&priv->initial_val, G_TYPE_FLOAT);
    g_value_set_float (&priv->initial_val, initial_val);

    int disp_width;
    int disp_prec;
    num_chars_float (priv->max_float - priv->min_float, 3,
            &disp_width, &disp_prec);
    if (disp_prec < 2)
        disp_prec = 2;
    priv->display_fmt = g_strdup_printf("%%0%d.%df", disp_width, disp_prec);
    return self;
}

void
cam_unit_control_modify_float (CamUnitControl * self,
        float min, float max, float step, int enabled)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    priv->min_float = min;
    priv->max_float = max;
    priv->step_float = step;
    priv->enabled = enabled;
    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
}


CamUnitControl * 
cam_unit_control_new_boolean (const char *id, const char *name,
        int initial_val, int enabled)
{
    CamUnitControl *self = cam_unit_control_new_basic (id, name, 
            CAM_UNIT_CONTROL_TYPE_BOOLEAN, enabled);
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    dbg (DBG_CONTROL, "[%s] - initial %d enabled %d\n",
            priv->name, initial_val, enabled);

    priv->min_int = 0;
    priv->max_int = 1;
    priv->step_int = 1;
    g_value_init (&priv->val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&priv->val, initial_val);
    g_value_init (&priv->initial_val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&priv->initial_val, initial_val);
    priv->display_fmt = g_strdup("%d");
    return self;
}

CamUnitControl * 
cam_unit_control_new_string (const char *id, const char *name,
        const char *initial_val, int enabled)
{
    CamUnitControl *self = cam_unit_control_new_basic (id, name,
            CAM_UNIT_CONTROL_TYPE_STRING, enabled);
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    g_value_init (&priv->val, G_TYPE_STRING);
    g_value_set_string (&priv->val, initial_val);
    g_value_init (&priv->initial_val, G_TYPE_STRING);
    g_value_set_string (&priv->initial_val, initial_val);
    priv->display_fmt = g_strdup("%s");
    return self;
}

void 
cam_unit_control_set_callback (CamUnitControl *self,
        CamUnitControlCallback callback, void *user_data)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    priv->try_set_function = callback;
    priv->user_data = user_data;
}

static int
check_type (CamUnitControl *self, const GValue *value)
{
    GType expected_type = G_TYPE_INVALID;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);

    switch (priv->type) {
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
                priv->name, 
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
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    g_value_copy (value, &priv->val);
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
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    int i;
    for(i=0; i<priv->n_enum_values; i++) {
        if(priv->enum_values[i].value == val)
            break;
    }
    if(i == priv->n_enum_values) {
        g_warning("%s: invalid value %d for enum [%s]\n", __FILE__,
                val, priv->id);
        return -1;
    }
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
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    if (priv->try_set_function) {
        GValue av = { 0, };
        g_value_init (&av, G_VALUE_TYPE (value));
        if (priv->try_set_function (self, value, &av, priv->user_data)) {
            g_value_copy (&av, &priv->val);
            g_value_unset (&av);
        } else {
            g_value_unset (&av);
            return -1;
        }
    } else {
        g_value_copy (value, &priv->val);
    }

    g_signal_emit (G_OBJECT (self), 
            cam_unit_control_signals[VALUE_CHANGED_SIGNAL], 0);
    
    return 0;
}

int 
cam_unit_control_try_set_int (CamUnitControl *self, int val)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_INT))
        return -1;
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
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_FLOAT))
        return -1;
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_FLOAT);
    g_value_set_float (&gv, val);
    int result = cam_unit_control_try_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_try_set_enum (CamUnitControl *self, int val)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_ENUM))
        return -1;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    int i;
    for(i=0; i<priv->n_enum_values; i++) {
        if(priv->enum_values[i].value == val) {
            if(!priv->enum_values[i].enabled) {
                g_warning("%s: enum [%s] value [%d] is disabled\n",
                        __FILE__, priv->id, val);
                return -1;
            }
            break;
        }
    }
    if(i == priv->n_enum_values) {
        g_warning("%s: invalid value %d for enum [%s]\n", __FILE__,
                val, priv->id);
        return -1;
    }
    GValue gv = { 0, };
    g_value_init (&gv, G_TYPE_INT);
    g_value_set_int (&gv, val);
    int result = cam_unit_control_try_set_val (self, &gv);
    g_value_unset (&gv);
    return result;
}

int 
cam_unit_control_try_set_boolean (CamUnitControl *self, int val)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_BOOLEAN))
        return -1;
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
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_STRING))
        return -1;
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
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    g_value_init (value, G_VALUE_TYPE (&priv->val));
    g_value_copy (&priv->val, value);
}
int 
cam_unit_control_get_int (const CamUnitControl *self)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_INT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return g_value_get_int (&priv->val);
}
float
cam_unit_control_get_float (const CamUnitControl *self)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_FLOAT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return g_value_get_float (&priv->val);
}
int 
cam_unit_control_get_enum (const CamUnitControl *self)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_ENUM))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return g_value_get_int (&priv->val);
}
int 
cam_unit_control_get_boolean (const CamUnitControl *self)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_BOOLEAN))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return g_value_get_boolean (&priv->val);
}
const char *
cam_unit_control_get_string (const CamUnitControl *self)
{
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_STRING))
        return "";
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return g_value_get_string (&priv->val);
}

// =========
int 
cam_unit_control_get_max_int (const CamUnitControl *self)
{ 
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_INT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->max_int; 
}
int 
cam_unit_control_get_min_int (const CamUnitControl *self)
{ 
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_INT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->min_int; 
}
int 
cam_unit_control_get_step_int (const CamUnitControl *self)
{ 
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_INT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->step_int; 
}

float 
cam_unit_control_get_max_float (const CamUnitControl *self)
{ 
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_FLOAT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->max_float; 
}
float
cam_unit_control_get_min_float (const CamUnitControl *self)
{ 
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_FLOAT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->min_float; 
}
float
cam_unit_control_get_step_float (const CamUnitControl *self)
{ 
    if(warn_if_wrong_type(self, CAM_UNIT_CONTROL_TYPE_FLOAT))
        return 0;
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->step_float; 
}

void 
cam_unit_control_set_enabled (CamUnitControl *self, int enabled)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    if (enabled != priv->enabled) {
        priv->enabled = enabled;
        g_signal_emit (G_OBJECT (self), 
                cam_unit_control_signals[PARAMS_CHANGED_SIGNAL], 0);
    } 
}

int 
cam_unit_control_get_enabled (const CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->enabled;
}

const char * 
cam_unit_control_get_name (const CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->name;
}

const char * 
cam_unit_control_get_id (const CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->id;
}

CamUnitControlType 
cam_unit_control_get_control_type (const CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->type;
}

void 
cam_unit_control_set_ui_hints (CamUnitControl *self, int flags)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    priv->ui_hints = flags;
}

int 
cam_unit_control_get_ui_hints (const CamUnitControl *self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return priv->ui_hints;
}

static char * type_str[] = {
    "Invalid",
    "Integer",
    "Boolean",
    "Enumeration",
    "String",
    "Float",
};

const char *
cam_unit_control_get_control_type_str (CamUnitControl * self)
{
    CamUnitControlPriv *priv = CAM_UNIT_CONTROL_GET_PRIVATE(self);
    return type_str[priv->type];
}
