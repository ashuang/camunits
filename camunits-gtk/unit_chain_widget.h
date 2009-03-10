#ifndef __cam_unit_chain_widget_h__
#define __cam_unit_chain_widget_h__

#include <gtk/gtk.h>

#include <camunits/unit.h>
#include <camunits/unit_chain.h>

G_BEGIN_DECLS

#define CAM_TYPE_UNIT_CHAIN_WIDGET  cam_unit_chain_widget_get_type()
#define CAM_UNIT_CHAIN_WIDGET(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    CAM_TYPE_UNIT_CHAIN_WIDGET, CamUnitChainWidget))
#define CAM_UNIT_CHAIN_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
    CAM_TYPE_UNIT_CHAIN_WIDGET, CamUnitChainWidgetClass ))
#define IS_CAM_UNIT_CHAIN_WIDGET(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_UNIT_CHAIN_WIDGET ))
#define IS_CAM_UNIT_CHAIN_WIDGET_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( (klass), CAM_TYPE_UNIT_CHAIN_WIDGET))

typedef struct _CamUnitChainWidget CamUnitChainWidget;
typedef struct _CamUnitChainWidgetClass CamUnitChainWidgetClass;

struct _CamUnitChainWidget
{
    GtkEventBox parent;
};

struct _CamUnitChainWidgetClass
{
    GtkEventBoxClass parent_class;
};

GType cam_unit_chain_widget_get_type(void);

CamUnitChainWidget *cam_unit_chain_widget_new( CamUnitChain *chain );

int cam_unit_chain_widget_set_chain( CamUnitChainWidget* self, 
        CamUnitChain *chain );

void cam_unit_chain_widget_set_orientation( CamUnitChainWidget *self,
       GtkOrientation orientation );

/**
 * cam_unit_chain_widget_find_unit_by_id:
 *
 * Searches for a unit in the chain with the specified id, and returns the
 * first matching unit.  Does not modify the reference count of the unit.
 *
 * Returns: the unit, or NULL if no matching unit was found.
 */
CamUnitControlWidget * cam_unit_chain_widget_find_unit_by_id (
        const CamUnitChainWidget * self, const char * unit_id);

G_END_DECLS

#endif  /* __cam_unit_chain_widget_h__ */
