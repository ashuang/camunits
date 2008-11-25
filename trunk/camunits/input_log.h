#ifndef __input_log_h__
#define __input_log_h__

G_BEGIN_DECLS

typedef struct _CamInputLogDriver CamInputLogDriver;
typedef struct _CamInputLogDriverClass CamInputLogDriverClass;

// boilerplate
#define CAM_INPUT_LOG_DRIVER_TYPE  cam_input_log_driver_get_type()
#define CAM_INPUT_LOG_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_LOG_DRIVER_TYPE, CamInputLogDriver))
#define CAM_INPUT_LOG_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_LOG_DRIVER_TYPE, CamInputLogDriverClass ))
#define IS_CAM_INPUT_LOG_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_LOG_DRIVER_TYPE ))
#define IS_CAM_INPUT_LOG_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_LOG_DRIVER_TYPE))
#define CAM_INPUT_LOG_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_LOG_DRIVER_TYPE, CamInputLogDriverClass))

GType cam_input_log_driver_get_type (void);

/**
 * Constructor
 */
CamInputLogDriver * cam_input_log_driver_new (void);


// =========================================================================

/*
 * CamInputLog
 */

typedef struct _CamInputLog CamInputLog;
typedef struct _CamInputLogClass CamInputLogClass;

// boilerplate
#define CAM_INPUT_LOG_TYPE  cam_input_log_get_type()
#define CAM_INPUT_LOG(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_INPUT_LOG_TYPE, CamInputLog))
#define CAM_INPUT_LOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_INPUT_LOG_TYPE, CamInputLogClass ))
#define IS_CAM_INPUT_LOG(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_INPUT_LOG_TYPE ))
#define IS_CAM_INPUT_LOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_INPUT_LOG_TYPE))
#define CAM_INPUT_LOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_INPUT_LOG_TYPE, CamInputLogClass))

GType cam_input_log_get_type (void);

/** 
 * cam_input_log_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamInputLogDriver call it.
 */
CamInputLog * cam_input_log_new (const char *fname);

enum {
    CAM_INPUT_LOG_ADVANCE_MODE_SOFT = 0,
    CAM_INPUT_LOG_ADVANCE_MODE_HARD
};

G_END_DECLS

#endif
