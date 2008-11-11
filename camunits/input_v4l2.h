#ifndef __input_v4l2_h__
#define __input_v4l2_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * SECTION:input_log
 * @short_description: input unit for Camunits log files
 */


/**
 * SECTION:input_v4l2
 * @short_description: input unit for Video 4 Linux 2 (V4L2) devices
 */

typedef struct _CamV4L2Driver CamV4L2Driver;
typedef struct _CamV4L2DriverClass CamV4L2DriverClass;

// boilerplate
#define CAM_V4L2_DRIVER_TYPE  cam_v4l2_driver_get_type()
#define CAM_V4L2_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_V4L2_DRIVER_TYPE, CamV4L2Driver))
#define CAM_V4L2_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_V4L2_DRIVER_TYPE, CamV4L2DriverClass ))
#define CAM_IS_V4L2_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_V4L2_DRIVER_TYPE ))
#define CAM_IS_V4L2_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_V4L2_DRIVER_TYPE))
#define CAM_V4L2_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_V4L2_DRIVER_TYPE, CamV4L2DriverClass))

struct _CamV4L2Driver {
    CamUnitDriver parent;
};

struct _CamV4L2DriverClass {
    CamUnitDriverClass parent_class;
};

GType cam_v4l2_driver_get_type (void);

/**
 * Constructor
 */
CamUnitDriver * cam_v4l2_driver_new (void);


// =========================================================================

/*
 * CamV4L2
 */

typedef struct _CamV4L2 CamV4L2;
typedef struct _CamV4L2Class CamV4L2Class;

// boilerplate
#define CAM_TYPE_V4L2  cam_v4l2_get_type()
#define CAM_V4L2(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_V4L2, CamV4L2))
#define CAM_V4L2_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_V4L2, CamV4L2Class ))
#define CAM_IS_V4L2(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_V4L2 ))
#define CAM_IS_V4L2_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_V4L2))
#define CAM_V4L2_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_V4L2, CamV4L2Class))

struct _CamV4L2 {
    CamUnit parent;

    /*< public >*/

    /*< private >*/
    int fd;
    int num_buffers;
    uint8_t ** buffers;
    int buffer_length;
    int buffers_outstanding;

    CamUnitControl *standard_ctl;
//    CamUnitControl *stream_ctl;
};

struct _CamV4L2Class {
    CamUnitClass parent_class;
};

GType cam_v4l2_get_type (void);

/** 
 * cam_v4l2_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamV4L2Driver call it.
 */
CamV4L2 * cam_v4l2_new (const char *path);

G_END_DECLS

#endif
