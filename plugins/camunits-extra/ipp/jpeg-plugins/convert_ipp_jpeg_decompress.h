#ifndef __camipp_jpeg_decompress_h__
#define __camipp_jpeg_decompress_h__

#include <glib-object.h>

#include <camunits/cam.h>

G_BEGIN_DECLS

typedef struct _CamippJpegDecompress CamippJpegDecompress;
typedef struct _CamippJpegDecompressClass CamippJpegDecompressClass;

// boilerplate
#define CAMIPP_TYPE_JPEG_DECOMPRESS  camipp_jpeg_decompress_get_type()
#define CAMIPP_JPEG_DECOMPRESS(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMIPP_TYPE_JPEG_DECOMPRESS, CamippJpegDecompress))
#define CAMIPP_JPEG_DECOMPRESS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMIPP_TYPE_JPEG_DECOMPRESS, CamippJpegDecompressClass ))
#define CAMIPP_IS_JPEG_DECOMPRESS(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMIPP_TYPE_JPEG_DECOMPRESS ))
#define CAMIPP_IS_JPEG_DECOMPRESS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMIPP_TYPE_JPEG_DECOMPRESS))
#define CAMIPP_JPEG_DECOMPRESS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMIPP_TYPE_JPEG_DECOMPRESS, CamippJpegDecompressClass))

struct _CamippJpegDecompress {
    CamUnit parent;
    
    /*< private >*/
    CamFrameBuffer * outbuf;
};

struct _CamippJpegDecompressClass {
    CamUnitClass parent_class;
};

GType camipp_jpeg_decompress_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamippJpegDecompress * camipp_jpeg_decompress_new (void);

CamUnitDriver * camipp_jpeg_decompress_driver_new (void);

G_END_DECLS

#endif
