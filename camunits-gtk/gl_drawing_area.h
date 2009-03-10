#ifndef __CAM_GL_GL_DRAWING_AREA_H__
#define __CAM_GL_GL_DRAWING_AREA_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdrawingarea.h>

G_BEGIN_DECLS

#define CAM_TYPE_GL_DRAWING_AREA            (cam_gl_drawing_area_get_type ())
#define CAM_GL_DRAWING_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAM_TYPE_GL_DRAWING_AREA, CamGLDrawingArea))
#define CAM_GL_DRAWING_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAM_TYPE_GL_DRAWING_AREA, CamGLDrawingAreaClass))
#define CAM_IS_GL_DRAWING_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_GL_DRAWING_AREA))
#define CAM_IS_GL_DRAWING_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAM_TYPE_GL_DRAWING_AREA))
#define CAM_GL_DRAWING_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAM_TYPE_GL_DRAWING_AREA, CamGLDrawingAreaClass))

typedef struct _CamGLDrawingArea        CamGLDrawingArea;
typedef struct _CamGLDrawingAreaClass   CamGLDrawingAreaClass;

struct _CamGLDrawingArea {
    GtkDrawingArea  parent;
};

struct _CamGLDrawingAreaClass {
    GtkDrawingAreaClass parent_class;
};

GType       cam_gl_drawing_area_get_type (void);
GtkWidget * cam_gl_drawing_area_new (gboolean vblank_sync);
void        cam_gl_drawing_area_set_vblank_sync (CamGLDrawingArea * glarea,
        gboolean vblank_sync);
void        cam_gl_drawing_area_swap_buffers (CamGLDrawingArea * glarea);
int         cam_gl_drawing_area_set_context (CamGLDrawingArea * glarea);
void        cam_gl_drawing_area_invalidate (CamGLDrawingArea * glarea);

G_END_DECLS

#endif
