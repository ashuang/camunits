#ifndef __camlcm_unit_lcm_publish_h__
#define __camlcm_unit_lcm_publish_h__

#include <libcam/cam.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CamlcmPublish CamlcmPublish;
typedef struct _CamlcmPublishClass CamlcmPublishClass;

// boilerplate
#define CAMLCM_TYPE_PUBLISH  camlcm_publish_get_type()
#define CAMLCM_PUBLISH(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMLCM_TYPE_PUBLISH, CamlcmPublish))
#define CAMLCM_PUBLISH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMLCM_TYPE_PUBLISH, CamlcmPublishClass ))
#define IS_CAMLCM_PUBLISH(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMLCM_TYPE_PUBLISH ))
#define IS_CAMLCM_PUBLISH_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMLCM_TYPE_PUBLISH))
#define CAMLCM_PUBLISH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMLCM_TYPE_PUBLISH, CamlcmPublishClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

#ifdef __cplusplus
}
#endif

#endif
