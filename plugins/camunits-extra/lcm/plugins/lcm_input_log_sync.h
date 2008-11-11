#ifndef __lcm_input_log_sync_h__
#define __lcm_input_log_sync_h__

#include <glib-object.h>

#include <camunits/unit.h>
#include <camunits/unit_driver.h>
#include <camunits/log.h>

#include "camlcm_image_sync_t.h"
#include "camlcm_image_legacy_sync_t.h"

G_BEGIN_DECLS

/*
 * CamInputLogSyncDriver
 */

typedef struct _CamInputLogSyncDriver CamInputLogSyncDriver;
typedef struct _CamInputLogSyncDriverClass CamInputLogSyncDriverClass;

// boilerplate
#define CAM_INPUT_LOG_SYNC_DRIVER_TYPE  cam_input_log_sync_driver_get_type()
#define CAM_INPUT_LOG_SYNC_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_LOG_SYNC_DRIVER_TYPE, CamInputLogSyncDriver))
#define CAM_INPUT_LOG_SYNC_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_LOG_SYNC_DRIVER_TYPE, CamInputLogSyncDriverClass ))
#define IS_CAM_INPUT_LOG_SYNC_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_LOG_SYNC_DRIVER_TYPE ))
#define IS_CAM_INPUT_LOG_SYNC_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_LOG_SYNC_DRIVER_TYPE))
#define CAM_INPUT_LOG_SYNC_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_LOG_SYNC_DRIVER_TYPE, CamInputLogSyncDriverClass))

struct _CamInputLogSyncDriver {
    CamUnitDriver parent;
};

struct _CamInputLogSyncDriverClass {
    CamUnitDriverClass parent_class;
};

GType cam_input_log_sync_driver_get_type (void);

/**
 * Constructor
 */
CamInputLogSyncDriver * cam_input_log_sync_driver_new (void);


// =========================================================================

/*
 * CamInputLogSync
 */

typedef struct _CamInputLogSync CamInputLogSync;
typedef struct _CamInputLogSyncClass CamInputLogSyncClass;

// boilerplate
#define CAM_INPUT_LOG_SYNC_TYPE  cam_input_log_sync_get_type()
#define CAM_INPUT_LOG_SYNC(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_LOG_SYNC_TYPE, CamInputLogSync))
#define CAM_INPUT_LOG_SYNC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_LOG_SYNC_TYPE, CamInputLogSyncClass ))
#define IS_CAM_INPUT_LOG_SYNC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_LOG_SYNC_TYPE ))
#define IS_CAM_INPUT_LOG_SYNC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_LOG_SYNC_TYPE))
#define CAM_INPUT_LOG_SYNC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_LOG_SYNC_TYPE, CamInputLogSyncClass))

struct _CamInputLogSync {
    CamUnit parent;

    CamLog *camlog;

    int64_t next_frame_time;

    int nframes;

    CamUnitControl *frame_ctl;
    CamUnitControl *pause_ctl;
    CamUnitControl *adv_mode_ctl;
    CamUnitControl *adv_speed_ctl;
    CamUnitControl *fname_ctl;
    CamUnitControl *sync_channel_ctl;

    int readone;

    lcm_t *lcm;
    camlcm_image_sync_t_subscription_t *subscription;
    camlcm_image_legacy_sync_t_subscription_t *legacy_subscription;
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

struct _CamInputLogSyncClass {
    CamUnitClass parent_class;
};

GType cam_input_log_sync_get_type (void);

/** 
 * cam_input_log_sync_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamInputLogSyncDriver call it.
 */
CamInputLogSync * cam_input_log_sync_new (const char *fname);

/**
 * Plugin entry points
 */
void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);


G_END_DECLS

#endif
