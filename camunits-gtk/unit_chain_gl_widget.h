#ifndef __cam_unit_chain_gl_widget_h__
#define __cam_unit_chain_gl_widget_h__

#include <gtk/gtk.h>

#include <camunits/unit_chain.h>

G_BEGIN_DECLS

#define CAM_TYPE_UNIT_CHAIN_GL_WIDGET  cam_unit_chain_gl_widget_get_type()
#define CAM_UNIT_CHAIN_GL_WIDGET(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    CAM_TYPE_UNIT_CHAIN_GL_WIDGET, CamUnitChainGLWidget))
#define CAM_UNIT_CHAIN_GL_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
    CAM_TYPE_UNIT_CHAIN_GL_WIDGET, CamUnitChainGLWidgetClass ))
#define IS_CAM_UNIT_CHAIN_GL_WIDGET(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_UNIT_CHAIN_GL_WIDGET ))
#define IS_CAM_UNIT_CHAIN_GL_WIDGET_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( (klass), CAM_TYPE_UNIT_CHAIN_GL_WIDGET))

typedef struct _CamUnitChainGLWidget CamUnitChainGLWidget;
typedef struct _CamUnitChainGLWidgetClass CamUnitChainGLWidgetClass;

struct _CamUnitChainGLWidget
{
    GtkVBox parent;
};

struct _CamUnitChainGLWidgetClass
{
    GtkVBoxClass parent_class;
};

GType        cam_unit_chain_gl_widget_get_type (void);

CamUnitChainGLWidget *cam_unit_chain_gl_widget_new (CamUnitChain *chain);

int cam_unit_chain_gl_widget_set_chain (CamUnitChainGLWidget* self, 
        CamUnitChain *chain);

CamUnitChain * cam_unit_chain_gl_widget_get_chain (CamUnitChainGLWidget* self);

void cam_unit_chain_gl_widget_request_redraw (CamUnitChainGLWidget *self);

void cam_unit_chain_gl_widget_set_context (CamUnitChainGLWidget *self);

GtkWidget * cam_unit_chain_gl_widget_get_gl_area (CamUnitChainGLWidget * self);

G_END_DECLS

#endif  /* __cam_unit_chain_gl_widget_h__ */
