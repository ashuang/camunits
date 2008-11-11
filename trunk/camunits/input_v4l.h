#ifndef __input_v4l_h__
#define __input_v4l_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * SECTION:input_v4l
 * @short_description: input unit for Video 4 Linux (V4L) devices
 */

typedef struct _CamV4LDriver CamV4LDriver;
typedef struct _CamV4LDriverClass CamV4LDriverClass;

// boilerplate
#define CAM_V4L_DRIVER_TYPE  cam_v4l_driver_get_type()
#define CAM_V4L_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_V4L_DRIVER_TYPE, CamV4LDriver))
#define CAM_V4L_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_V4L_DRIVER_TYPE, CamV4LDriverClass ))
#define CAM_IS_V4L_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_V4L_DRIVER_TYPE ))
#define CAM_IS_V4L_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_V4L_DRIVER_TYPE))
#define CAM_V4L_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_V4L_DRIVER_TYPE, CamV4LDriverClass))

struct _CamV4LDriver {
    CamUnitDriver parent;
};

struct _CamV4LDriverClass {
    CamUnitDriverClass parent_class;
};

GType cam_v4l_driver_get_type (void);

/**
 * Constructor
 */
CamUnitDriver * cam_v4l_driver_new (void);


// =========================================================================

/*
 * CamV4L
 */

typedef struct _CamV4L CamV4L;
typedef struct _CamV4LClass CamV4LClass;

// boilerplate
#define CAM_TYPE_V4L  cam_v4l_get_type()
#define CAM_V4L(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_V4L, CamV4L))
#define CAM_V4L_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_V4L, CamV4LClass ))
#define CAM_IS_V4L(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_V4L ))
#define CAM_IS_V4L_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_V4L))
#define CAM_V4L_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_V4L, CamV4LClass))

struct _CamV4L {
    CamUnit parent;

    CamUnitControl *brightness_ctl;
    CamUnitControl *hue_ctl;
    CamUnitControl *color_ctl;
    CamUnitControl *contrast_ctl;
    CamUnitControl *whiteness_ctl;
    GList *tuner_ctls;
    int fd;
    
    int is_pwc;
    CamUnitControl *pwc_wb_mode_ctl;
    CamUnitControl *pwc_wb_manual_red_ctl;
    CamUnitControl *pwc_wb_manual_blue_ctl;
};

struct _CamV4LClass {
    CamUnitClass parent_class;
};

GType cam_v4l_get_type (void);

/** 
 * cam_v4l_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamV4LDriver call it.
 */
CamV4L * cam_v4l_new (int videonum);

G_END_DECLS

#endif
