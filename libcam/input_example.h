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
CamInputExampleDriver * cam_input_example_driver_new();


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

    int64_t next_frame_time;
    int cur_row;

    int fps;
};

struct _CamInputExampleClass {
    CamUnitClass parent_class;
};

GType cam_input_example_get_type (void);

/** 
 * Constructor.
 * don't call this function manually.  Instead, let the CamInputExampleDriver
 * call it.
 */
CamInputExample * cam_input_example_new();

enum {
    CAM_INPUT_EXAMPLE_CONTROL_ENUM,
    CAM_INPUT_EXAMPLE_CONTROL_BOOLEAN,
    CAM_INPUT_EXAMPLE_CONTROL_INT,
    CAM_INPUT_EXAMPLE_CONTROL_INT2,
    CAM_INPUT_EXAMPLE_CONTROL_STRING,
    CAM_INPUT_EXAMPLE_CONTROL_DOUBLE
};

G_END_DECLS

#endif
