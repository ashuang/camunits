#ifndef __bayer_filter_h__
#define __bayer_filter_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * CamBayerFilter
 *
 * This demonstrates how to create a simple filter
 */

typedef struct _CamBayerFilter CamBayerFilter;
typedef struct _CamBayerFilterClass CamBayerFilterClass;

// boilerplate
#define CAM_TYPE_BAYER_FILTER  cam_bayer_filter_get_type()
#define CAM_BAYER_FILTER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_BAYER_FILTER, CamBayerFilter))
#define CAM_BAYER_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_BAYER_FILTER, CamBayerFilterClass ))
#define CAM_IS_BAYER_FILTER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_BAYER_FILTER ))
#define CAM_IS_BAYER_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_BAYER_FILTER))
#define CAM_BAYER_FILTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_BAYER_FILTER, CamBayerFilterClass))

struct _CamBayerFilter {
    CamUnit parent;
    CamUnitControl *bayer_method_ctl;
    CamUnitControl *bayer_tile_ctl;
};

struct _CamBayerFilterClass {
    CamUnitClass parent_class;
};

GType cam_bayer_filter_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamBayerFilter * cam_bayer_filter_new();

CamUnitDriver * cam_bayer_filter_driver_new();


/**
 * If you're using a bayer filter directly in your application, you can use
 * these values to set the tiling pattern with the "Tiling" control.  e.g.
 *
 * cam_unit_set_control( unit, 
 */
enum {
    CAM_FILTER_BAYER_TILING_GBRG = 0,
    CAM_FILTER_BAYER_TILING_GRBG = 1,
    CAM_FILTER_BAYER_TILING_BGGR = 2,
    CAM_FILTER_BAYER_TILING_RGGB = 3,
};

/**
 * If you're using a bayer filter directly in your application, you can use
 * these values to set the tiling pattern with the "Method" control.  e.g.
 *
 * cam_unit_set_control( unit, 
 */
enum {
    CAM_FILTER_BAYER_METHOD_NEAREST = 0,
    CAM_FILTER_BAYER_METHOD_SMIPLE = 1,
    CAM_FILTER_BAYER_METHOD_BILINEAR = 2,
    CAM_FILTER_BAYER_METHOD_HQLINEAR = 3,
    CAM_FILTER_BAYER_METHOD_EDGE_SENSE = 4,
    CAM_FILTER_BAYER_METHOD_VNG = 5
};

G_END_DECLS

#endif

