#ifndef __input_dc1394_h__
#define __input_dc1394_h__

#include <glib-object.h>
#include <dc1394/control.h>

#include "unit.h"
#include "unit_driver.h"

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
CamUnitDriver * cam_dc1394_driver_new();


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

#define CAM_DC1394_CNTL_ID_MASK 0x0000ffff
#define CAM_DC1394_CNTL_ID_FLAGS_MASK 0xffff0000
#define CAM_DC1394_CNTL_FLAG_STATE (1<<16)
#define CAM_DC1394_CNTL_FLAG_ALT (1<<17)

/* This enum matches the libdc1394 enum exactly, and must stay that way. */
enum {
    CAM_DC1394_CNTL_BRIGHTNESS= 416,
    CAM_DC1394_CNTL_EXPOSURE,
    CAM_DC1394_CNTL_SHARPNESS,
    CAM_DC1394_CNTL_WHITE_BALANCE,
    CAM_DC1394_CNTL_HUE,
    CAM_DC1394_CNTL_SATURATION,
    CAM_DC1394_CNTL_GAMMA,
    CAM_DC1394_CNTL_SHUTTER,
    CAM_DC1394_CNTL_GAIN,
    CAM_DC1394_CNTL_IRIS,
    CAM_DC1394_CNTL_FOCUS,
    CAM_DC1394_CNTL_TEMPERATURE,
    CAM_DC1394_CNTL_TRIGGER,
    CAM_DC1394_CNTL_TRIGGER_DELAY,
    CAM_DC1394_CNTL_WHITE_SHADING,
    CAM_DC1394_CNTL_FRAME_RATE,
    /* 16 reserved features */
    CAM_DC1394_CNTL_ZOOM,
    CAM_DC1394_CNTL_PAN,
    CAM_DC1394_CNTL_TILT,
    CAM_DC1394_CNTL_OPTICAL_FILTER,
    /* 12 reserved features */
    CAM_DC1394_CNTL_CAPTURE_SIZE,
    CAM_DC1394_CNTL_CAPTURE_QUALITY,
    /* 14 reserved features */

    /* libcam-only features */
    CAM_DC1394_CNTL_TRIGGER_POLARITY=2000,
    CAM_DC1394_CNTL_TRIGGER_SOURCE,
    CAM_DC1394_CNTL_TRIGGER_NOW,
    CAM_DC1394_CNTL_PACKET_SIZE,
};

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

/** 
 * cam_dc1394_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamDC1394Driver call it.
 */
CamDC1394 * cam_dc1394_new( dc1394camera_t * camera );

G_END_DECLS

#endif
