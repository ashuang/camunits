#ifndef __camfw_jpeg_decompress_h__
#define __camfw_jpeg_decompress_h__

#include <glib-object.h>

#include <libcam/cam.h>

G_BEGIN_DECLS

typedef struct _CamfwJpegDecompress CamfwJpegDecompress;
typedef struct _CamfwJpegDecompressClass CamfwJpegDecompressClass;

// boilerplate
#define CAMFW_TYPE_JPEG_DECOMPRESS  camfw_jpeg_decompress_get_type()
#define CAMFW_JPEG_DECOMPRESS(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMFW_TYPE_JPEG_DECOMPRESS, CamfwJpegDecompress))
#define CAMFW_JPEG_DECOMPRESS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMFW_TYPE_JPEG_DECOMPRESS, CamfwJpegDecompressClass ))
#define CAMFW_IS_JPEG_DECOMPRESS(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMFW_TYPE_JPEG_DECOMPRESS ))
#define CAMFW_IS_JPEG_DECOMPRESS_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMFW_TYPE_JPEG_DECOMPRESS))
#define CAMFW_JPEG_DECOMPRESS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMFW_TYPE_JPEG_DECOMPRESS, CamfwJpegDecompressClass))

struct _CamfwJpegDecompress {
    CamUnit parent;
    
    /*< private >*/
    CamFrameBuffer * outbuf;
};

struct _CamfwJpegDecompressClass {
    CamUnitClass parent_class;
};

GType camfw_jpeg_decompress_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamfwJpegDecompress * camfw_jpeg_decompress_new (void);

CamUnitDriver * camfw_jpeg_decompress_driver_new (void);

G_END_DECLS

#endif
