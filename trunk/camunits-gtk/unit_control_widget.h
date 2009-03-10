#ifndef __cam_unit_control_widget_h__
#define __cam_unit_control_widget_h__

#include <gtk/gtk.h>

#include <camunits/unit.h>
#include <camunits/unit_control.h>

G_BEGIN_DECLS

#define CAM_TYPE_UNIT_CONTROL_WIDGET  cam_unit_control_widget_get_type()
#define CAM_UNIT_CONTROL_WIDGET(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    CAM_TYPE_UNIT_CONTROL_WIDGET, CamUnitControlWidget))
#define CAM_UNIT_CONTROL_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
    CAM_TYPE_UNIT_CONTROL_WIDGET, CamUnitControlWidgetClass ))
#define IS_CAM_UNIT_CONTROL_WIDGET(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_UNIT_CONTROL_WIDGET ))
#define IS_CAM_UNIT_CONTROL_WIDGET_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( (klass), CAM_TYPE_UNIT_CONTROL_WIDGET))

typedef struct _CamUnitControlWidget CamUnitControlWidget;
typedef struct _CamUnitControlWidgetClass CamUnitControlWidgetClass;

struct _CamUnitControlWidget
{
    GtkEventBox parent;
};

struct _CamUnitControlWidgetClass
{
    GtkEventBoxClass parent_class;
};

GType        cam_unit_control_widget_get_type(void);

CamUnitControlWidget *cam_unit_control_widget_new( CamUnit *unit );

int cam_unit_control_widget_set_unit( CamUnitControlWidget* self, 
        CamUnit *unit );

CamUnit * cam_unit_control_widget_get_unit(CamUnitControlWidget* self);

void cam_unit_control_widget_set_expanded (CamUnitControlWidget * self,
        gboolean expanded);

extern GtkTargetEntry cam_unit_control_widget_target_entry;

void cam_unit_control_widget_detach( CamUnitControlWidget *self );

// this must be different from all other drag and drop id's used (the info
// field of a GtkTargetEntry struct)
#define CAM_UNIT_CONTROL_WIDGET_DND_ID 101

G_END_DECLS

#endif  /* __cam_unit_control_widget_h__ */
