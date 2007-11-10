#ifndef __cam_unit_control_h__
#define __cam_unit_control_h__

#include <glib-object.h>

#ifdef __cplusplus 
extern "C" {
#endif

/**
 * SECTION:unit_control
 * @short_description:  User adjustable / automatically changing control value.
 * 
 * CamUnitControl represents a control value that can both automatically change
 * (e.g. the frame number of an advancing log playback unit) and can be
 * adjusted by the user.
 *
 * Typically, a #CamUnit will have a few of these controls that allow the user
 * to interact with the unit.  A control is strongly typed on construction, and
 * the following types of controls are currently supported:  Integer, Boolean,
 * Menu (enum), String.  The current value of a control can be queried using
 * the cam_unit_control_get_* methods.  There are two ways to possibly set the
 * value of a control - the polite way, and the forceful way.
 *
 * The forceful way of setting a control involves using the
 * cam_unit_control_force_set_* methods.  The value of the control will always
 * change, and the "value-changed" signal will be emitted. 
 *
 * The polite way of setting a control involves using the
 * #cam_unit_control_set_callback and cam_unit_control_try_set_* methods.  
 * The idea is that it is sometimes possible that the requested control value
 * is not acceptable, and should either not change at all, or should change to
 * a value other than what the user requested.  Each time a
 * cam_unit_control_try_set_* method is called, the callback is invoked to see
 * what the actual control value should be.  For example, the user may request
 * a specific shutter speed for a camera unit that is not allowed when the
 * camera is used in a specific mode.
 *
 * The CamUnitControl class itself does not contain any rendering code to
 * provide a GUI interface.  However, each control may provide "hints" as to
 * how the control should be displayed.  For example, a string control may
 * provide the #CAM_UNIT_CONTROL_FILENAME hint that suggests the control
 * actually represents a filename, and should be displayed with a file chooser
 * widget.
 */

/**
 * CamUnitControlType:
 *
 * enumerates the different types of controls available
 */
typedef enum {
    // no valid control should every have this type
    CAM_UNIT_CONTROL_TYPE_INVALID = 0,

    // Represents an integer control with a minimum value, maximum value, and
    // a step size.
    CAM_UNIT_CONTROL_TYPE_INT = 1,

    // Represents a boolean control that can be either True or False
    CAM_UNIT_CONTROL_TYPE_BOOLEAN = 2,

    // Represents an enumeration of choices from which one can be selected.
    CAM_UNIT_CONTROL_TYPE_ENUM = 3,

    // Represents a string-valued control
    CAM_UNIT_CONTROL_TYPE_STRING = 4,

    // Represents a floating point control
    CAM_UNIT_CONTROL_TYPE_FLOAT = 5,
} CamUnitControlType;

typedef enum {
    CAM_UNIT_CONTROL_CHECK_BOX = (1 << 1),
    CAM_UNIT_CONTROL_TOGGLE_BUTTON = (1 << 2),
    CAM_UNIT_CONTROL_MENU = (1 << 3),
    CAM_UNIT_CONTROL_RADIO_BUTTONS = (1 << 4),
    CAM_UNIT_CONTROL_FILENAME = (1 << 5),
    CAM_UNIT_CONTROL_TEXT_ENTRY = (1 << 6),
    CAM_UNIT_CONTROL_ONE_SHOT = (1 << 7),
    CAM_UNIT_CONTROL_SLIDER = (1 << 8),
    CAM_UNIT_CONTROL_SPINBUTTON = (1 << 9)
} CamUnitControlUIHint;


typedef struct _CamUnitControl CamUnitControl;
typedef struct _CamUnitControlClass CamUnitControlClass;

#define CAM_TYPE_UNIT_CONTROL  cam_unit_control_get_type()
#define CAM_UNIT_CONTROL(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CAM_TYPE_UNIT_CONTROL, CamUnitControl))
#define CAM_UNIT_CONTROL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_UNIT_CONTROL, CamUnitControlClass))
#define CAM_IS_UNIT_CONTROL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_UNIT_CONTROL))
#define CAM_IS_UNIT_CONTROL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE (\
            (klass), CAM_TYPE_UNIT_CONTROL))
#define CAM_UNIT_CONTROL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_UNIT_CONTROL, CamUnitControlClass))

/**
 * CamUnitControlCallback:
 * @ctl:      the CamUnitControl in question
 * @proposed: the value passed in to cam_unit_control_try_set_val
 * @actual:   output parameter.  If the control value is to change, the
 *            function must set actual to the new value for the control.
 *            Often, it is sufficient to simply call g_value_copy (proposed,
 *            actual).  However, it is possible that the control value may
 *            change, but not as requested.  In this case, actual can differ
 *            from proposed.
 * @user_data: the user_data parameter passed in to
 *             cam_unit_control_set_callback
 *
 * Returns: FALSE if the control value should not change.  TRUE if the control
 *          value should change to the output parameter actual.
 */
typedef gboolean (*CamUnitControlCallback)(const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual, void *user_data);

/**
 * CamUnitControl:
 *
 * A control option on a CamUnit that the user or a program can adjust.
 */
struct _CamUnitControl {
    GObject parent;
    CamUnitControlType type;
    int id;
    char * name;

	/*< private >*/
    int enabled;

    CamUnitControlCallback try_set_function;
    void *user_data;

    char **menu_entries;
    int * menu_entries_enabled;
    int max_int;
    int min_int;
    int step_int;

    float max_float;
    float min_float;
    float step_float;
    int display_width;
    int display_prec;

    int ui_hints;

    GValue val;
    GValue initial_val;
};

struct _CamUnitControlClass {
    GObjectClass parent_class;
};

GType cam_unit_control_get_type();

/**
 * cam_unit_control_set_callback:
 * @callback: the callback function to invoke when using the
 *            cam_unit_control_try_set_* methods.
 *
 * Sets the function which will be invoked when any of the
 * cam_unit_control_try_set_* methods are called.  If the callback returns
 * FALSE, then the control will not be set.  If the callback returns TRUE,
 * then the control will be adjusted.  See #CamUnitControlCallback.  In
 * general, you shouldn't need to call this when subclassing from #CamUnit, as
 * #CamUnit does it automagically.
 */
void cam_unit_control_set_callback (CamUnitControl *self,
        CamUnitControlCallback callback, void *user_data);

/**
 * cam_unit_control_new_menu:
 * @id: a numerical identifier for the control.
 * @name: a nickname / human-understandable-name for the control.
 * @initial_index: the initial value for the control.
 * @enabled: TRUE of the control should be initially enabled
 * @entries: a NULL-terminated array of strings specifying the different
 *           options to present to the user.
 * @entries_enabled: an array of integers indicating whether each entry
 *           in @entries should be enabled.  1 indicates enabled, and 0
 *           disabled.  Specify NULL to implicitly enable all entries.
 *
 * Returns: a new menu control.
 */
CamUnitControl * cam_unit_control_new_menu (int id, const char *name, 
        int initial_index, int enabled, const char **entries,
        const int * entries_enabled);

/**
 * cam_unit_control_new_int:
 * @id: a numerical identifier for the control.
 * @name: a nickname / human-understandable-name for the control.
 * @min: the minimum value the control can take on
 * @max: the maximum value the control can take on
 * @step: the step size with which the control is allowed to change.  
 * @initial_val: the initial value for the control.
 * @enabled: TRUE of the control should be initially enabled
 *
 * Returns: a new integer control
 */
CamUnitControl * cam_unit_control_new_int (int id, const char *name, 
        int min, int max, int step, int initial_val, int enabled);

/**
 * cam_unit_control_new_float:
 * @id: a numerical identifier for the control.
 * @name: a nickname / human-understandable-name for the control.
 * @min: the minimum value the control can take on
 * @max: the maximum value the control can take on
 * @step: the step size with which the control is allowed to change.  
 * @initial_val: the initial value for the control.
 * @enabled: TRUE of the control should be initially enabled
 *
 * Returns: a new floating-point control
 */
CamUnitControl * cam_unit_control_new_float (int id, const char *name, 
        float min, float max, float step, float initial_val, int enabled);

/**
 * cam_unit_control_new_boolean:
 * @id: a numerical identifier for the control.
 * @name: a nickname / human-understandable-name for the control.
 * @initial_val: the initial value for the control.
 * @enabled: TRUE of the control should be initially enabled
 * 
 * Returns: a new boolean control
 */
CamUnitControl * cam_unit_control_new_boolean (int id, const char *name,
        int initial_val, int enabled);

/**
 * cam_unit_control_new_string:
 * @id: a numerical identifier for the control.
 * @name: a nickname / human-understandable-name for the control.
 * @initial_val: the initial value for the control.
 * @enabled: TRUE of the control should be initially enabled
 * 
 * Returns: a new string control
 */
CamUnitControl * cam_unit_control_new_string (int id, const char *name,
        const char *initial_val, int enabled);

/**
 * cam_unit_control_unref:
 *
 * Decrements the reference count on the CamUnitControl.  A CamUnitControl
 * starts with a reference count of 1.  If this count ever reaches zero, the
 * control is destroyed.
 */
void cam_unit_control_unref (CamUnitControl *self);

void cam_unit_control_modify_int (CamUnitControl * self,
        int min, int max, int step, int enabled);
void cam_unit_control_modify_float (CamUnitControl * self,
        float min, float max, float step, int enabled);
void cam_unit_control_modify_menu (CamUnitControl * self,
        int enabled, const char ** entries, const int * entries_enabled);

int cam_unit_control_try_set_val (CamUnitControl *self, const GValue *val);
int cam_unit_control_try_set_int (CamUnitControl *self, int val);
int cam_unit_control_try_set_float (CamUnitControl *self, float val);
int cam_unit_control_try_set_enum (CamUnitControl *self, int index);
int cam_unit_control_try_set_boolean (CamUnitControl *self, int val);
int cam_unit_control_try_set_string (CamUnitControl *self, const char *val);

int cam_unit_control_force_set_val(CamUnitControl *self, const GValue *value);
int cam_unit_control_force_set_int(CamUnitControl *self, int val);
int cam_unit_control_force_set_float(CamUnitControl *self, float val);
int cam_unit_control_force_set_enum(CamUnitControl *self, int val);
int cam_unit_control_force_set_boolean(CamUnitControl *self, int val);
int cam_unit_control_force_set_string (CamUnitControl *self, const char *val);

void cam_unit_control_get_val (const CamUnitControl *self, GValue *value);
int cam_unit_control_get_int (const CamUnitControl *self);
float cam_unit_control_get_float (const CamUnitControl *self);
int cam_unit_control_get_enum (const CamUnitControl *self);
int cam_unit_control_get_boolean (const CamUnitControl *self);
const char* cam_unit_control_get_string (const CamUnitControl *self);

int cam_unit_control_get_max_int (const CamUnitControl *self);
int cam_unit_control_get_min_int (const CamUnitControl *self);
int cam_unit_control_get_step_int (const CamUnitControl *self);

float cam_unit_control_get_max_float (const CamUnitControl *self);
float cam_unit_control_get_min_float (const CamUnitControl *self);
float cam_unit_control_get_step_float (const CamUnitControl *self);

void cam_unit_control_set_enabled (CamUnitControl *self, int enabled);

int cam_unit_control_get_enabled (const CamUnitControl *self);

/**
 * cam_unit_control_set_ui_hints:
 * @flags: a logical OR of #CamUnitControlUIHint values
 *
 * See also: #CamUnitControlUIHint
 */
void cam_unit_control_set_ui_hints (CamUnitControl *self, int flags);

/**
 * cam_unit_control_get_ui_hints:
 *
 * See also: #CamUnitControlUIHint
 *
 * Returns: a logical OR of #CamUnitControlUIHint values
 */
int cam_unit_control_get_ui_hints (const CamUnitControl *self);

#ifdef __cplusplus
}
#endif

#endif
