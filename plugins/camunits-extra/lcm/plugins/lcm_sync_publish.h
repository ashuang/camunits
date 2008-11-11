#ifndef __camlcm_unit_lcm_syncpub_h__
#define __camlcm_unit_lcm_syncpub_h__

#include <camunits/cam.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CamlcmSyncPub CamlcmSyncPub;
typedef struct _CamlcmSyncPubClass CamlcmSyncPubClass;

// boilerplate
#define CAMLCM_TYPE_SYNCPUB  camlcm_syncpub_get_type()
#define CAMLCM_SYNCPUB(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMLCM_TYPE_SYNCPUB, CamlcmSyncPub))
#define CAMLCM_SYNCPUB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMLCM_TYPE_SYNCPUB, CamlcmSyncPubClass ))
#define IS_CAMLCM_SYNCPUB(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMLCM_TYPE_SYNCPUB ))
#define IS_CAMLCM_SYNCPUB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMLCM_TYPE_SYNCPUB))
#define CAMLCM_SYNCPUB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMLCM_TYPE_SYNCPUB, CamlcmSyncPubClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

#define CAMLCM_DEFAULT_SYNC_CHANNEL   "CAMLCM_SYNC"

#ifdef __cplusplus
}
#endif

#endif
