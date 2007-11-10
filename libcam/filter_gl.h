#ifndef __filter_gl_h__
#define __filter_gl_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

#include "gl_texture.h"

G_BEGIN_DECLS

/**
 * SECTION:filter_gl
 * @short_description: passthrough filter that can render to an OpenGL context
 *
 * CamFilterGL is a filter subclass of #CamUnit that passes images through
 * unchanged, but implements the #draw_gl_init, #draw_gl, and #draw_gl_shutdown
 * routines that allow it to render images to an OpenGL context.
 *
 * To create a new CamFilterGL unit, use #cam_unit_chain_add_unit_by_id with
 * unit_id "filter_gl:0"
 */

typedef struct _CamFilterGL CamFilterGL;
typedef struct _CamFilterGLClass CamFilterGLClass;

// boilerplate
#define CAM_TYPE_FILTER_GL  cam_filter_gl_get_type()
#define CAM_FILTER_GL(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_FILTER_GL, CamFilterGL))
#define CAM_FILTER_GL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_FILTER_GL, CamFilterGLClass ))
#define CAM_IS_FILTER_GL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_FILTER_GL ))
#define CAM_IS_FILTER_GL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_FILTER_GL))
#define CAM_FILTER_GL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_FILTER_GL, CamFilterGLClass))

struct _CamFilterGL {
    CamUnit parent;

    GLTexture * gl_texture;
//    int gl_refcount;
    int gl_initialized;
};

struct _CamFilterGLClass {
    CamUnitClass parent_class;
};

GType cam_filter_gl_get_type (void);

/** 
 * cam_filter_gl_new:
 *
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 *
 * Returns: a newly created CamFilterGL
 */
CamFilterGL * cam_filter_gl_new();

/**
 * cam_filter_gl_driver_new:
 *
 * Driver constructor.  Creates a CamUnitDriver that is capable of generating
 * CamFilterGL objects.  You should not need to invoke this manually, as it is
 * a core driver automatically instantiated by the CamUnitManager
 *
 * Returns: a newly created CamUnitDriver
 */
CamUnitDriver * cam_filter_gl_driver_new();

G_END_DECLS

#endif

