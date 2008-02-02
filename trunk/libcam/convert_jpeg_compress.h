#ifndef __convert_jpeg_compress_h__
#define __convert_jpeg_compress_h__

#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

G_BEGIN_DECLS

/**
 * SECTION:convert_jpeg_compress
 * @short_description: unit to encode and decode JPEG images
 *
 * CamConvertJpegCompress can compress RGB, BGRA, and grayscale images to JPEG.
 * CamConvertJpegCompress uses libjpeg.
 *
 * CamConvertJpegCompress is a core unit, and is always available with unit
 * id "convert.jpeg_compress"
 */

typedef struct _CamConvertJpegCompress CamConvertJpegCompress;
typedef struct _CamConvertJpegCompressClass CamConvertJpegCompressClass;

// boilerplate
#define CAM_TYPE_CONVERT_JPEG_COMPRESS  cam_convert_jpeg_compress_get_type()
#define CAM_CONVERT_JPEG_COMPRESS(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_CONVERT_JPEG_COMPRESS, CamConvertJpegCompress))
#define CAM_CONVERT_JPEG_COMPRESS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_CONVERT_JPEG_COMPRESS, CamConvertJpegCompressClass ))
#define CAM_IS_CONVERT_JPEG_COMPRESS(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_CONVERT_JPEG_COMPRESS ))
#define CAM_IS_CONVERT_JPEG_COMPRESS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_CONVERT_JPEG_COMPRESS))
#define CAM_CONVERT_JPEG_COMPRESS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_CONVERT_JPEG_COMPRESS, CamConvertJpegCompressClass))

struct _CamConvertJpegCompress {
    CamUnit parent;
    
    /*< private >*/
    CamUnitControl * quality_control;
    CamFrameBuffer * outbuf;
};

struct _CamConvertJpegCompressClass {
    CamUnitClass parent_class;
};

GType cam_convert_jpeg_compress_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamConvertJpegCompress * cam_convert_jpeg_compress_new (void);

CamUnitDriver * cam_convert_jpeg_compress_driver_new (void);

G_END_DECLS

#endif

