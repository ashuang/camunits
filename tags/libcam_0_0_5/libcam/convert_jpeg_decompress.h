#ifndef __convert_jpeg_decompress_h__
#define __convert_jpeg_decompress_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * SECTION:convert_jpeg_decompress
 * @short_description: unit to encode and decode JPEG images
 *
 * CamConvertJpegDecompress can decompress JPEG-compressed images to RGB.
 * CamConvertJpegDecompress uses libjpeg.
 *
 * CamConvertJpegDecompress is a core unit, and is always available with unit
 * id "convert.jpeg_decompress"
 */

typedef struct _CamConvertJpegDecompress CamConvertJpegDecompress;
typedef struct _CamConvertJpegDecompressClass CamConvertJpegDecompressClass;

// boilerplate
#define CAM_TYPE_CONVERT_JPEG_DECOMPRESS  cam_convert_jpeg_decompress_get_type()
#define CAM_CONVERT_JPEG_DECOMPRESS(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_CONVERT_JPEG_DECOMPRESS, CamConvertJpegDecompress))
#define CAM_CONVERT_JPEG_DECOMPRESS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_CONVERT_JPEG_DECOMPRESS, CamConvertJpegDecompressClass ))
#define CAM_IS_CONVERT_JPEG_DECOMPRESS(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_CONVERT_JPEG_DECOMPRESS ))
#define CAM_IS_CONVERT_JPEG_DECOMPRESS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_CONVERT_JPEG_DECOMPRESS))
#define CAM_CONVERT_JPEG_DECOMPRESS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_CONVERT_JPEG_DECOMPRESS, CamConvertJpegDecompressClass))

struct _CamConvertJpegDecompress {
    CamUnit parent;
    
    /*< private >*/
    CamFrameBuffer * outbuf;
};

struct _CamConvertJpegDecompressClass {
    CamUnitClass parent_class;
};

GType cam_convert_jpeg_decompress_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamConvertJpegDecompress * cam_convert_jpeg_decompress_new (void);

CamUnitDriver * cam_convert_jpeg_decompress_driver_new (void);

G_END_DECLS

#endif
