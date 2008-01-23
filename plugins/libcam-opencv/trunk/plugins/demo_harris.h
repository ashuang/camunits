#ifndef __camlcm_unit_lcm_publish_h__
#define __camlcm_unit_lcm_publish_h__

#include <libcam/cam.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CamcvHarris CamcvHarris;
typedef struct _CamcvHarrisClass CamcvHarrisClass;

// boilerplate
#define CAMCV_TYPE_HARRIS  camcv_harris_get_type()
#define CAMCV_HARRIS(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMCV_TYPE_HARRIS, CamcvHarris))
#define CAMCV_HARRIS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMCV_TYPE_HARRIS, CamcvHarrisClass ))
#define IS_CAMCV_HARRIS(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMCV_TYPE_HARRIS ))
#define IS_CAMCV_HARRIS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMCV_TYPE_HARRIS))
#define CAMCV_HARRIS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMCV_TYPE_HARRIS, CamcvHarrisClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

#ifdef __cplusplus
}
#endif

#endif
