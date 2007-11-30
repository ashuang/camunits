#ifndef __input_example_h__
#define __input_example_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * This demonstrates how to create a CamUnit and a CamUnitDriver.
 */


/*
 * CamInputExampleDriver
 */

typedef struct _CamInputExampleDriver CamInputExampleDriver;
typedef struct _CamInputExampleDriverClass CamInputExampleDriverClass;

// boilerplate
#define CAM_INPUT_EXAMPLE_DRIVER_TYPE  cam_input_example_driver_get_type()
#define CAM_INPUT_EXAMPLE_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_EXAMPLE_DRIVER_TYPE, CamInputExampleDriver))
#define CAM_INPUT_EXAMPLE_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_EXAMPLE_DRIVER_TYPE, CamInputExampleDriverClass ))
#define CAM_IS_INPUT_EXAMPLE_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_EXAMPLE_DRIVER_TYPE ))
#define CAM_IS_INPUT_EXAMPLE_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_EXAMPLE_DRIVER_TYPE))
#define CAM_INPUT_EXAMPLE_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_EXAMPLE_DRIVER_TYPE, CamInputExampleDriverClass))

struct _CamInputExampleDriver {
    CamUnitDriver parent;
};

struct _CamInputExampleDriverClass {
    CamUnitDriverClass parent_class;

};

GType cam_input_example_driver_get_type (void);

/**
 * Constructor
 */
CamInputExampleDriver * cam_input_example_driver_new(void);


// =========================================================================

/*
 * CamInputExample
 */

typedef struct _CamInputExample CamInputExample;
typedef struct _CamInputExampleClass CamInputExampleClass;

// boilerplate
#define CAM_INPUT_EXAMPLE_TYPE  cam_input_example_get_type()
#define CAM_INPUT_EXAMPLE(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_EXAMPLE_TYPE, CamInputExample))
#define CAM_INPUT_EXAMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_EXAMPLE_TYPE, CamInputExampleClass ))
#define CAM_IS_INPUT_EXAMPLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_EXAMPLE_TYPE ))
#define CAM_IS_INPUT_EXAMPLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_EXAMPLE_TYPE))
#define CAM_INPUT_EXAMPLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_EXAMPLE_TYPE, CamInputExampleClass))

struct _CamInputExample {
    CamUnit parent;

    CamUnitControl *enum_ctl;
    CamUnitControl *bool_ctl;
    CamUnitControl *int1_ctl;
    CamUnitControl *int2_ctl;

    int64_t next_frame_time;

    int x;
    int y;
    int dx;
    int dy;

    int fps;
};

struct _CamInputExampleClass {
    CamUnitClass parent_class;
};

GType cam_input_example_get_type (void);

/** 
 * cam_input_example_new:
 *
 * Constructor.
 * Don't call this function manually.  Instead, let the CamInputExampleDriver
 * call it.
 */
CamInputExample * cam_input_example_new(void);

G_END_DECLS

#endif
