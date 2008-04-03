#ifndef __camlcm_unit_lcm_subscribe_h__
#define __camlcm_unit_lcm_subscribe_h__

#include <glib-object.h>

#include <libcam/cam.h>
#include <lcm/lcm.h>

G_BEGIN_DECLS

/*
 * CamlcmInputDriver
 */

typedef struct _CamlcmInputDriver CamlcmInputDriver;
typedef struct _CamlcmInputDriverClass CamlcmInputDriverClass;

// boilerplate
#define CAMLCM_TYPE_INPUT_DRIVER  camlcm_input_driver_get_type()
#define CAMLCM_INPUT_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMLCM_TYPE_INPUT_DRIVER, CamlcmInputDriver))
#define CAMLCM_INPUT_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMLCM_TYPE_INPUT_DRIVER, CamlcmInputDriverClass ))
#define IS_CAMLCM_INPUT_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMLCM_TYPE_INPUT_DRIVER ))
#define IS_CAMLCM_INPUT_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMLCM_TYPE_INPUT_DRIVER))
#define CAMLCM_INPUT_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMLCM_TYPE_INPUT_DRIVER, CamlcmInputDriverClass))

struct _CamlcmInputDriver {
    CamUnitDriver parent;

    lcm_t *lcm;
//    lcm_lc_handler_t *ahid;
    camlcm_announce_t_subscription_t *subscription;

    GHashTable *known_sources;

    GThread *lcm_thread;
    GAsyncQueue *source_q;
    int thread_exit_requested;

    int notify_pipe[2];
};

struct _CamlcmInputDriverClass {
    CamUnitDriverClass parent_class;
};

GType camlcm_input_driver_get_type (void);

/**
 * Constructor
 */
CamlcmInputDriver * camlcm_input_driver_new();

// =========================================================================

/*
 * CamlcmInput
 */

typedef struct _CamlcmInput CamlcmInput;
typedef struct _CamlcmInputClass CamlcmInputClass;

// boilerplate
#define CAMLCM_TYPE_INPUT  camlcm_input_get_type()
#define CAMLCM_INPUT(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMLCM_TYPE_INPUT, CamlcmInput))
#define CAMLCM_INPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMLCM_TYPE_INPUT, CamlcmInputClass ))
#define IS_CAMLCM_INPUT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMLCM_TYPE_INPUT ))
#define IS_CAMLCM_INPUT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMLCM_TYPE_INPUT))
#define CAMLCM_INPUT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMLCM_TYPE_INPUT, CamlcmInputClass))

struct _CamlcmInput {
    CamUnit parent;

    lcm_t *lcm;

    camlcm_announce_t *announce;
    int read_fd;
    int write_fd;
    int unhandled_frame;

    camlcm_image_t_subscription_t *subscription;

    GMutex *buffer_mutex;
    CamFrameBuffer *received_image;
};

struct _CamlcmInputClass {
    CamUnitClass parent_class;
};

GType camlcm_input_get_type (void);

G_END_DECLS

#endif
