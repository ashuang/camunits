#ifndef __convert_jpeg_compress_h__
#define __convert_jpeg_compress_h__

#include <glib-object.h>

#include <libcam/cam.h>

G_BEGIN_DECLS

/**
 * SECTION:convert_jpeg_compress
 * @short_description: unit to encode and decode JPEG images
 *
 * CamippJpegCompress can compress RGB, BGRA, and grayscale images to JPEG.
 * CamippJpegCompress uses libjpeg.
 *
 * CamippJpegCompress is a core unit, and is always available with unit
 * id "convert.jpeg_compress"
 */

typedef struct _CamippJpegCompress CamippJpegCompress;
typedef struct _CamippJpegCompressClass CamippJpegCompressClass;

// boilerplate
#define CAMIPP_TYPE_JPEG_COMPRESS  camipp_jpeg_compress_get_type()
#define CAMIPP_JPEG_COMPRESS(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMIPP_TYPE_JPEG_COMPRESS, CamippJpegCompress))
#define CAMIPP_JPEG_COMPRESS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMIPP_TYPE_JPEG_COMPRESS, CamippJpegCompressClass ))
#define CAMIPP_IS_JPEG_COMPRESS(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMIPP_TYPE_JPEG_COMPRESS ))
#define CAMIPP_IS_JPEG_COMPRESS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMIPP_TYPE_JPEG_COMPRESS))
#define CAMIPP_JPEG_COMPRESS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMIPP_TYPE_JPEG_COMPRESS, CamippJpegCompressClass))

struct _CamippJpegCompress {
    CamUnit parent;
    
    /*< private >*/
    CamUnitControl * quality_control;
    CamFrameBuffer * outbuf;
};

struct _CamippJpegCompressClass {
    CamUnitClass parent_class;
};

GType camipp_jpeg_compress_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamippJpegCompress * camipp_jpeg_compress_new (void);

CamUnitDriver * camipp_jpeg_compress_driver_new (void);

G_END_DECLS

#endif

