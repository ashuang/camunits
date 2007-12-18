#ifndef __input_dc1394_h__
#define __input_dc1394_h__

#include <glib-object.h>
#include <dc1394/control.h>

#include <libcam/cam.h>

G_BEGIN_DECLS

/**
 * A driver for FireWire cameras using libdc1394.
 */


/*
 * LB2Ladybug2Driver
 */

typedef struct _LB2Ladybug2Driver LB2Ladybug2Driver;
typedef struct _LB2Ladybug2DriverClass LB2Ladybug2DriverClass;

// boilerplate
#define LB2_LADYBUG2_DRIVER_TYPE  lb2_ladybug2_driver_get_type()
#define LB2_LADYBUG2_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        LB2_LADYBUG2_DRIVER_TYPE, LB2Ladybug2Driver))
#define LB2_LADYBUG2_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            LB2_LADYBUG2_DRIVER_TYPE, LB2Ladybug2DriverClass ))
#define LB2_IS_LADYBUG2_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
         LB2   _LADYBUG2_DRIVER_TYPE ))
#define LB2_IS_LADYBUG2_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), LB2_LADYBUG2_DRIVER_TYPE))
#define LB2_LADYBUG2_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            LB2_LADYBUG2_DRIVER_TYPE, LB2Ladybug2DriverClass))

struct _LB2Ladybug2Driver {
    CamUnitDriver parent;

    dc1394camera_t ** cameras;
    unsigned int num_cameras;
};

struct _LB2Ladybug2DriverClass {
    CamUnitDriverClass parent_class;
};

GType lb2_ladybug2_driver_get_type (void);

/**
 * Constructor
 */
CamUnitDriver * lb2_ladybug2_driver_new (void);


// =========================================================================

/*
 * LB2Ladybug2
 */

typedef struct _LB2Ladybug2 LB2Ladybug2;
typedef struct _LB2Ladybug2Class LB2Ladybug2Class;

// boilerplate
#define LB2_LADYBUG2_TYPE  lb2_ladybug2_get_type()
#define LB2_LADYBUG2(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        LB2_LADYBUG2_TYPE, LB2Ladybug2))
#define LB2_LADYBUG2_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            LB2_LADYBUG2_TYPE, LB2Ladybug2Class ))
#define IS_LB2_LADYBUG2(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            LB2_LADYBUG2_TYPE ))
#define IS_LB2_LADYBUG2_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), LB2_LADYBUG2_TYPE))
#define LB2_LADYBUG2_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            LB2_LADYBUG2_TYPE, LB2Ladybug2Class))

struct _LB2Ladybug2 {
    CamUnit parent;

    dc1394camera_t * cam;
    unsigned int packet_size;
    int fd;
};

struct _LB2Ladybug2Class {
    CamUnitClass parent_class;
};

GType lb2_ladybug2_get_type (void);

#define LB2_LADYBUG2_CNTL_ID_MASK 0x0000ffff
#define LB2_LADYBUG2_CNTL_ID_FLAGS_MASK 0xffff0000
#define LB2_LADYBUG2_CNTL_FLAG_STATE (1<<16)
#define LB2_LADYBUG2_CNTL_FLAG_ALT (1<<17)

/* This enum matches the libdc1394 enum exactly, and must stay that way. */
enum {
    LB2_LADYBUG2_CNTL_BRIGHTNESS= 416,
    LB2_LADYBUG2_CNTL_EXPOSURE,
    LB2_LADYBUG2_CNTL_SHARPNESS,
    LB2_LADYBUG2_CNTL_WHITE_BALANCE,
    LB2_LADYBUG2_CNTL_HUE,
    LB2_LADYBUG2_CNTL_SATURATION,
    LB2_LADYBUG2_CNTL_GAMMA,
    LB2_LADYBUG2_CNTL_SHUTTER,
    LB2_LADYBUG2_CNTL_GAIN,
    LB2_LADYBUG2_CNTL_IRIS,
    LB2_LADYBUG2_CNTL_FOCUS,
    LB2_LADYBUG2_CNTL_TEMPERATURE,
    LB2_LADYBUG2_CNTL_TRIGGER,
    LB2_LADYBUG2_CNTL_TRIGGER_DELAY,
    LB2_LADYBUG2_CNTL_WHITE_SHADING,
    LB2_LADYBUG2_CNTL_FRAME_RATE,
    /* 16 reserved features */
    LB2_LADYBUG2_CNTL_ZOOM,
    LB2_LADYBUG2_CNTL_PAN,
    LB2_LADYBUG2_CNTL_TILT,
    LB2_LADYBUG2_CNTL_OPTICAL_FILTER,
    /* 12 reserved features */
    LB2_LADYBUG2_CNTL_CAPTURE_SIZE,
    LB2_LADYBUG2_CNTL_CAPTURE_QUALITY
    /* 14 reserved features */
};

enum {
    LB2_LADYBUG2_MENU_OFF=0,
    LB2_LADYBUG2_MENU_AUTO=1,
    LB2_LADYBUG2_MENU_MANUAL=2,
};

/** 
 * lb2_ladybug2_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * LB2Ladybug2Driver call it.
 */
LB2Ladybug2 * lb2_ladybug2_new (dc1394camera_t * camera);

G_END_DECLS

#endif
