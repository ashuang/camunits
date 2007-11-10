#ifndef __filter_jpeg_h__
#define __filter_jpeg_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * CamFilterJpeg
 *
 * This demonstrates how to create a simple filter
 */

typedef struct _CamFilterJpeg CamFilterJpeg;
typedef struct _CamFilterJpegClass CamFilterJpegClass;

// boilerplate
#define CAM_TYPE_FILTER_JPEG  cam_filter_jpeg_get_type()
#define CAM_FILTER_JPEG(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_FILTER_JPEG, CamFilterJpeg))
#define CAM_FILTER_JPEG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_FILTER_JPEG, CamFilterJpegClass ))
#define CAM_IS_FILTER_JPEG(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_FILTER_JPEG ))
#define CAM_IS_FILTER_JPEG_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_FILTER_JPEG))
#define CAM_FILTER_JPEG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_FILTER_JPEG, CamFilterJpegClass))

struct _CamFilterJpeg {
    CamUnit parent;
    CamUnitControl * quality_control;
};

struct _CamFilterJpegClass {
    CamUnitClass parent_class;
};

GType cam_filter_jpeg_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamFilterJpeg * cam_filter_jpeg_new();

CamUnitDriver * cam_filter_jpeg_driver_new();

G_END_DECLS

#endif

