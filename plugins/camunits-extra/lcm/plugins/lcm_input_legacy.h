#ifndef __lcm_input_legacy_h__
#define __lcm_input_legacy_h__

#include <glib-object.h>

#include <camunits/unit.h>
#include <camunits/unit_driver.h>
#include <camunits/log.h>

#include "camlcm_image_legacy_t.h"

G_BEGIN_DECLS

/*
 * CamInputLegacyDriver
 */

typedef struct _CamInputLegacyDriver CamInputLegacyDriver;
typedef struct _CamInputLegacyDriverClass CamInputLegacyDriverClass;

// boilerplate
#define CAM_INPUT_LEGACY_DRIVER_TYPE  cam_input_legacy_driver_get_type()
#define CAM_INPUT_LEGACY_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_LEGACY_DRIVER_TYPE, CamInputLegacyDriver))
#define CAM_INPUT_LEGACY_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_LEGACY_DRIVER_TYPE, CamInputLegacyDriverClass ))
#define IS_CAM_INPUT_LEGACY_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_LEGACY_DRIVER_TYPE ))
#define IS_CAM_INPUT_LEGACY_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_LEGACY_DRIVER_TYPE))
#define CAM_INPUT_LEGACY_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_LEGACY_DRIVER_TYPE, CamInputLegacyDriverClass))

struct _CamInputLegacyDriver {
    CamUnitDriver parent;
};

struct _CamInputLegacyDriverClass {
    CamUnitDriverClass parent_class;
};

GType cam_input_legacy_driver_get_type (void);

/**
 * Constructor
 */
CamInputLegacyDriver * cam_input_legacy_driver_new (void);


// =========================================================================

/*
 * CamInputLegacy
 */

typedef struct _CamInputLegacy CamInputLegacy;
typedef struct _CamInputLegacyClass CamInputLegacyClass;

// boilerplate
#define CAM_INPUT_LEGACY_TYPE  cam_input_legacy_get_type()
#define CAM_INPUT_LEGACY(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_LEGACY_TYPE, CamInputLegacy))
#define CAM_INPUT_LEGACY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_LEGACY_TYPE, CamInputLegacyClass ))
#define IS_CAM_INPUT_LEGACY(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_LEGACY_TYPE ))
#define IS_CAM_INPUT_LEGACY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_LEGACY_TYPE))
#define CAM_INPUT_LEGACY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_LEGACY_TYPE, CamInputLegacyClass))

struct _CamInputLegacy {
    CamUnit parent;

    CamUnitControl *channel_ctl;

    lcm_t *lcm;
//    camlcm_image_sync_t_subscription_t *subscription;
    camlcm_image_legacy_t_subscription_t *subscription;
    camlcm_image_legacy_t *new_image;
    GThread *lcm_thread;
    int thread_exit_requested;
    int notify_pipe[2];
    int frame_ready_pipe[2];

    int64_t legacy_uid;
    int has_legacy_uid;

    GMutex *sync_mutex;
    int64_t sync_utime;
    int sync_pending;
};

struct _CamInputLegacyClass {
    CamUnitClass parent_class;
};

GType cam_input_legacy_get_type (void);

/** 
 * cam_input_legacy_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamInputLegacyDriver call it.
 */
CamInputLegacy * cam_input_legacy_new (const char *fname);

/**
 * Plugin entry points
 */
void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);


G_END_DECLS

#endif
