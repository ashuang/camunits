#ifndef __convert_jpeg_compress_h__
#define __convert_jpeg_compress_h__

#include <glib-object.h>

#include <camunits/cam.h>

G_BEGIN_DECLS

/**
 * SECTION:convert_jpeg_compress
 * @short_description: unit to encode and decode JPEG images
 *
 * CamfwJpegCompress can compress RGB, BGRA, and grayscale images to JPEG.
 * CamfwJpegCompress uses libjpeg.
 *
 * CamfwJpegCompress is a core unit, and is always available with unit
 * id "convert.jpeg_compress"
 */

typedef struct _CamfwJpegCompress CamfwJpegCompress;
typedef struct _CamfwJpegCompressClass CamfwJpegCompressClass;

// boilerplate
#define CAMFW_TYPE_JPEG_COMPRESS  camfw_jpeg_compress_get_type()
#define CAMFW_JPEG_COMPRESS(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMFW_TYPE_JPEG_COMPRESS, CamfwJpegCompress))
#define CAMFW_JPEG_COMPRESS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMFW_TYPE_JPEG_COMPRESS, CamfwJpegCompressClass ))
#define CAMFW_IS_JPEG_COMPRESS(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMFW_TYPE_JPEG_COMPRESS ))
#define CAMFW_IS_JPEG_COMPRESS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMFW_TYPE_JPEG_COMPRESS))
#define CAMFW_JPEG_COMPRESS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMFW_TYPE_JPEG_COMPRESS, CamfwJpegCompressClass))

struct _CamfwJpegCompress {
    CamUnit parent;
    
    /*< private >*/
    CamUnitControl * quality_control;
    CamFrameBuffer * outbuf;
};

struct _CamfwJpegCompressClass {
    CamUnitClass parent_class;
};

GType camfw_jpeg_compress_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamfwJpegCompress * camfw_jpeg_compress_new (void);

CamUnitDriver * camfw_jpeg_compress_driver_new (void);

G_END_DECLS

#endif

