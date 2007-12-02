#ifndef __input_dc1394_h__
#define __input_dc1394_h__

#include <glib-object.h>
#include <dc1394/control.h>

#include <libcam/unit.h>
#include <libcam/unit_driver.h>

G_BEGIN_DECLS

/**
 * A driver for FireWire cameras using libdc1394.
 */


/*
 * CamDC1394Driver
 */

typedef struct _CamDC1394Driver CamDC1394Driver;
typedef struct _CamDC1394DriverClass CamDC1394DriverClass;

// boilerplate
#define CAM_DC1394_DRIVER_TYPE  cam_dc1394_driver_get_type()
#define CAM_DC1394_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_DC1394_DRIVER_TYPE, CamDC1394Driver))
#define CAM_DC1394_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_DC1394_DRIVER_TYPE, CamDC1394DriverClass ))
#define CAM_IS_DC1394_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_DC1394_DRIVER_TYPE ))
#define CAM_IS_DC1394_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_DC1394_DRIVER_TYPE))
#define CAM_DC1394_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_DC1394_DRIVER_TYPE, CamDC1394DriverClass))

struct _CamDC1394Driver {
    CamUnitDriver parent;

    dc1394camera_t ** cameras;
    unsigned int num_cameras;
};

struct _CamDC1394DriverClass {
    CamUnitDriverClass parent_class;
};

GType cam_dc1394_driver_get_type (void);

/**
 * Constructor
 */
CamUnitDriver * cam_dc1394_driver_new(void);


// =========================================================================

/*
 * CamDC1394
 */

typedef struct _CamDC1394 CamDC1394;
typedef struct _CamDC1394Class CamDC1394Class;

// boilerplate
#define CAM_DC1394_TYPE  cam_dc1394_get_type()
#define CAM_DC1394(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_DC1394_TYPE, CamDC1394))
#define CAM_DC1394_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_DC1394_TYPE, CamDC1394Class ))
#define CAM_IS_DC1394(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_DC1394_TYPE ))
#define CAM_IS_DC1394_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_DC1394_TYPE))
#define CAM_DC1394_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_DC1394_TYPE, CamDC1394Class))

struct _CamDC1394 {
    CamUnit parent;

    dc1394camera_t * cam;
    unsigned int packet_size;
    int fd;
    int num_buffers;
};

struct _CamDC1394Class {
    CamUnitClass parent_class;
};

GType cam_dc1394_get_type (void);

enum {
    CAM_DC1394_MENU_OFF=0,
    CAM_DC1394_MENU_AUTO=1,
    CAM_DC1394_MENU_MANUAL=2,
};

enum {
    CAM_DC1394_TRIGGER_OFF=0,
    CAM_DC1394_TRIGGER_MODE_0=1,
    CAM_DC1394_TRIGGER_MODE_1=2,
    CAM_DC1394_TRIGGER_MODE_2=3,
    CAM_DC1394_TRIGGER_MODE_3=4,
    CAM_DC1394_TRIGGER_MODE_4=5,
    CAM_DC1394_TRIGGER_MODE_5=6,
    CAM_DC1394_TRIGGER_MODE_14=7,
    CAM_DC1394_TRIGGER_MODE_15=8,
};

enum {
    CAM_DC1394_TRIGGER_SOURCE_0=0,
    CAM_DC1394_TRIGGER_SOURCE_1=1,
    CAM_DC1394_TRIGGER_SOURCE_2=2,
    CAM_DC1394_TRIGGER_SOURCE_3=3,
    CAM_DC1394_TRIGGER_SOURCE_SOFTWARE=4,
};

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

/** 
 * cam_dc1394_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamDC1394Driver call it.
 */
CamDC1394 * cam_dc1394_new( dc1394camera_t * camera );

G_END_DECLS

#endif
