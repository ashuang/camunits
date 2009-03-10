#ifndef __cam_unit_manager_widget_h__
#define __cam_unit_manager_widget_h__

#include <gtk/gtk.h>

#include <camunits/unit.h>
#include <camunits/unit_manager.h>

G_BEGIN_DECLS

#define CAM_TYPE_UNIT_MANAGER_WIDGET  cam_unit_manager_widget_get_type()
#define CAM_UNIT_MANAGER_WIDGET(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    CAM_TYPE_UNIT_MANAGER_WIDGET, CamUnitManagerWidget))
#define CAM_UNIT_MANAGER_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
    CAM_TYPE_UNIT_MANAGER_WIDGET, CamUnitManagerWidgetClass ))
#define IS_CAM_UNIT_MANAGER_WIDGET(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_UNIT_MANAGER_WIDGET ))
#define IS_CAM_UNIT_MANAGER_WIDGET_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( (klass), CAM_TYPE_UNIT_MANAGER_WIDGET))

typedef struct _CamUnitManagerWidget CamUnitManagerWidget;
typedef struct _CamUnitManagerWidgetClass CamUnitManagerWidgetClass;

struct _CamUnitManagerWidget
{
    GtkTreeView parent;
};

struct _CamUnitManagerWidgetClass
{
    GtkTreeViewClass parent_class;
};

GType cam_unit_manager_widget_get_type(void);

CamUnitManagerWidget *cam_unit_manager_widget_new(void);

CamUnitDescription * cam_unit_manager_widget_get_selected_description (
        CamUnitManagerWidget * self);

extern GtkTargetEntry cam_unit_manager_widget_target_entry;

// this must be different from all other drag and drop id's used (the info
// field of a GtkTargetEntry struct)
#define CAM_UNIT_MANAGER_WIDGET_DND_ID 100

G_END_DECLS

#endif  /* __cam_unit_manager_widget_h__ */
