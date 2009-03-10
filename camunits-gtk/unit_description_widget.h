#ifndef __cam_unit_description_widget_h__
#define __cam_unit_description_widget_h__

#include <gtk/gtk.h>

#include <camunits/unit.h>
#include "unit_manager_widget.h"


G_BEGIN_DECLS

#define CAM_TYPE_UNIT_DESCRIPTION_WIDGET  cam_unit_description_widget_get_type()
#define CAM_UNIT_DESCRIPTION_WIDGET(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    CAM_TYPE_UNIT_DESCRIPTION_WIDGET, CamUnitDescriptionWidget))
#define CAM_UNIT_DESCRIPTION_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
    CAM_TYPE_UNIT_DESCRIPTION_WIDGET, CamUnitDescriptionWidgetClass ))
#define IS_CAM_UNIT_DESCRIPTION_WIDGET(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_UNIT_DESCRIPTION_WIDGET ))
#define IS_CAM_UNIT_DESCRIPTION_WIDGET_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( (klass), CAM_TYPE_UNIT_DESCRIPTION_WIDGET))

typedef struct _CamUnitDescriptionWidget CamUnitDescriptionWidget;
typedef struct _CamUnitDescriptionWidgetClass CamUnitDescriptionWidgetClass;

struct _CamUnitDescriptionWidget
{
    GtkTextView parent;
};

struct _CamUnitDescriptionWidgetClass
{
    GtkTextViewClass parent_class;
};

GType cam_unit_description_widget_get_type(void);

CamUnitDescriptionWidget *cam_unit_description_widget_new (void);

int cam_unit_description_widget_set_manager( CamUnitDescriptionWidget* self, 
        CamUnitManagerWidget *manager );

G_END_DECLS

#endif  /* __cam_unit_description_widget_h__ */
