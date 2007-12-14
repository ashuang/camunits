#ifndef __example_filter_h__
#define __example_filter_h__

#include <glib-object.h>

#include <libcam/unit.h>

G_BEGIN_DECLS

/**
 * CamFilterExample
 *
 * This demonstrates how to create a simple filter
 */

typedef struct _CamFilterExample CamFilterExample;
typedef struct _CamFilterExampleClass CamFilterExampleClass;

// boilerplate
#define CAM_TYPE_FILTER_EXAMPLE  cam_filter_example_get_type()
#define CAM_FILTER_EXAMPLE(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_FILTER_EXAMPLE, CamFilterExample))
#define CAM_FILTER_EXAMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_FILTER_EXAMPLE, CamFilterExampleClass ))
#define IS_CAM_FILTER_EXAMPLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_FILTER_EXAMPLE ))
#define IS_CAM_FILTER_EXAMPLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_FILTER_EXAMPLE))
#define CAM_FILTER_EXAMPLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_FILTER_EXAMPLE, CamFilterExampleClass))

struct _CamFilterExample {
    CamUnit parent;
    CamUnitControl *patch_intensity_control;
};

struct _CamFilterExampleClass {
    CamUnitClass parent_class;
};

GType cam_filter_example_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamFilterExample * cam_filter_example_new();

CamUnitDriver * cam_filter_example_driver_new (void);

G_END_DECLS

#endif
