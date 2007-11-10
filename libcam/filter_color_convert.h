#ifndef __color_conversion_filter_h__
#define __color_conversion_filter_h__

#include <glib-object.h>

#include "pixels.h"

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * CamColorConversionFilter
 *
 * This demonstrates how to create a simple filter
 */

typedef struct _CamColorConversionFilter CamColorConversionFilter;
typedef struct _CamColorConversionFilterClass CamColorConversionFilterClass;

// boilerplate
#define CAM_TYPE_COLOR_CONVERSION_FILTER  cam_color_conversion_filter_get_type()
#define CAM_COLOR_CONVERSION_FILTER(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_COLOR_CONVERSION_FILTER, CamColorConversionFilter))
#define CAM_COLOR_CONVERSION_FILTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_COLOR_CONVERSION_FILTER, CamColorConversionFilterClass ))
#define CAM_IS_COLOR_CONVERSION_FILTER(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_COLOR_CONVERSION_FILTER ))
#define CAM_IS_COLOR_CONVERSION_FILTER_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_COLOR_CONVERSION_FILTER))
#define CAM_COLOR_CONVERSION_FILTER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_COLOR_CONVERSION_FILTER, CamColorConversionFilterClass))

struct _CamColorConversionFilter {
    CamUnit parent;

    int (*cc_func)(CamColorConversionFilter *self, 
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf,
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf);
    GList *conversions;

    CamPixelFormat output_pixelformat_override;
};

struct _CamColorConversionFilterClass {
    CamUnitClass parent_class;
};

GType cam_color_conversion_filter_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamColorConversionFilter * cam_color_conversion_filter_new();

CamUnitDriver * cam_color_conversion_filter_driver_new();

/**
 * cam_color_conversion_filter_set_output_format:
 *
 * Use this function to programmatically restrict the output format of the
 * color conversion unit to a specific pixelformat.  If the filter is already
 * streaming or ready, then it is shutdown and re-initialized with the new
 * format.
 *
 * Note that this does not guarantee that you will definitely get images of the
 * specified pixelformat as output.  The color conversion filter will still
 * fail to initialize if it is unable to convert input frames to the requested
 * format.
 */
void cam_color_conversion_filter_set_output_format (
        CamColorConversionFilter *self, CamPixelFormat pixelformat);

G_END_DECLS

#endif
