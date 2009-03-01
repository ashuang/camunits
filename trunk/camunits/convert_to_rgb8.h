#ifndef __cam_convert_to_rgb8_h__
#define __cam_convert_to_rgb8_h__

#include <glib-object.h>

#include "pixels.h"

#include "unit.h"
#include "unit_driver.h"
#include "unit_manager.h"

G_BEGIN_DECLS

/**
 * SECTION:convert_to_rgb8
 * @short_description: Convenience unit that attempts to convert everything to 24-bit RGB
 *
 * CamConvertToRgb8 is a convenience unit that attempts to convert its input to
 * 24-bit RGB.  It is actually a wrapper around several other conversion units,
 * and tries to intelligently decide which unit to use when faced with a given
 * input image source.
 *
 * CamConvertToRgb8 is a core unit, and is always available with unit
 * id "convert.to_rgb8"
 */

typedef struct _CamConvertToRgb8 CamConvertToRgb8;
typedef struct _CamConvertToRgb8Class CamConvertToRgb8Class;

// boilerplate
#define CAM_TYPE_CONVERT_TO_RGB8  cam_convert_to_rgb8_get_type()
#define CAM_CONVERT_TO_RGB8(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_CONVERT_TO_RGB8, CamConvertToRgb8))
#define CAM_CONVERT_TO_RGB8_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_CONVERT_TO_RGB8, CamConvertToRgb8Class ))
#define CAM_IS_CONVERT_TO_RGB8(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_CONVERT_TO_RGB8 ))
#define CAM_IS_CONVERT_TO_RGB8_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_CONVERT_TO_RGB8))
#define CAM_CONVERT_TO_RGB8_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_CONVERT_TO_RGB8, CamConvertToRgb8Class))

struct _CamConvertToRgb8 {
    CamUnit parent;

    /*< private >*/
    CamUnit *worker;
    CamUnitManager *manager;
};

struct _CamConvertToRgb8Class {
    CamUnitClass parent_class;
};

GType cam_convert_to_rgb8_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamConvertToRgb8 * cam_convert_to_rgb8_new(void);

CamUnitDriver * cam_convert_to_rgb8_driver_new(void);

G_END_DECLS

#endif
