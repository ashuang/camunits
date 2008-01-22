#ifndef __camipp_jpeg_h__
#define __camipp_jpeg_h__

#include <glib-object.h>

#include <libcam/cam.h>

G_BEGIN_DECLS

/**
 * SECTION:filter_jpeg
 * @short_description: unit to encode and decode JPEG images
 *
 * CamippJpeg can compress RGB, BGRA, and grayscale images to JPEG.  It can
 * also decompress JPEG-compressed images to RGB.  CamippJpeg uses libjpeg.
 *
 * CamippJpeg is a core unit, and is always available with unit
 * id "convert.jpeg"
 */

typedef struct _CamippJpeg CamippJpeg;
typedef struct _CamippJpegClass CamippJpegClass;

// boilerplate
#define CAMIPP_TYPE_JPEG  camipp_jpeg_get_type()
#define CAMIPP_JPEG(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMIPP_TYPE_JPEG, CamippJpeg))
#define CAMIPP_JPEG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMIPP_TYPE_JPEG, CamippJpegClass ))
#define CAMIPP_IS_JPEG(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMIPP_TYPE_JPEG ))
#define CAMIPP_IS_JPEG_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMIPP_TYPE_JPEG))
#define CAMIPP_JPEG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMIPP_TYPE_JPEG, CamippJpegClass))

struct _CamippJpeg {
    CamUnit parent;
    CamUnitControl * quality_control;
};

struct _CamippJpegClass {
    CamUnitClass parent_class;
};

GType camipp_jpeg_get_type (void);

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamippJpeg * camipp_jpeg_new (void);

CamUnitDriver * camipp_jpeg_driver_new (void);

G_END_DECLS

#endif

