#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <libcam/plugin.h>
#include <lcm/lcm.h>

#include "camlcm_announce_t.h"
#include "camlcm_image_t.h"

#include "lcm_publish.h"

#define err(args...) fprintf(stderr, args)

#define CAMLCM_PUBLISH_DEFAULT_ANNOUNCE_INTERVAL_USEC 300000

#define DEFAULT_DATA_RATE_UPDATE_INTERVAL_USEC 1000000
#define DATA_RATE_UPDATE_ALPHA 0.7

#define CONTROL_PUBLISH "publish"
#define CONTROL_CHANNEL "channel"
#define CONTROL_DATA_RATE_INFO "data-rate"

#define DEFAULT_LCM_URL "udpm://?transmit_only=true"

struct _CamlcmPublish {
    CamUnit parent;

    lcm_t * lcm;

    CamUnitControl *publish_ctl;
    CamUnitControl *lcm_name_ctl;
    CamUnitControl *data_rate_ctl;
    CamUnitControl *lcm_url_ctl;

    int64_t next_announce_time;
    int64_t announce_interval;

    int64_t bytes_transferred_since_last_data_rate_update;
    int64_t last_data_rate_time;
    int64_t data_rate_update_interval;
    double data_rate;
};

struct _CamlcmPublishClass {
    CamUnitClass parent_class;
};

GType camlcm_publish_get_type (void);

static inline int64_t timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static CamlcmPublish * camlcm_publish_new(void);
static void camlcm_publish_finalize( GObject *obj );
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static gboolean _try_set_control (CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);

CAM_PLUGIN_TYPE (CamlcmPublish, camlcm_publish, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    camlcm_publish_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("output", "lcm_publish",
            "LCM Publish", 0, (CamUnitConstructor)camlcm_publish_new,
            module);
}

static void
camlcm_publish_class_init( CamlcmPublishClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = camlcm_publish_finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camlcm_publish_init( CamlcmPublish *self )
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT( self );

    self->lcm = lcm_create (DEFAULT_LCM_URL);

    self->next_announce_time = 0;
    self->announce_interval = CAMLCM_PUBLISH_DEFAULT_ANNOUNCE_INTERVAL_USEC;

    self->publish_ctl = cam_unit_add_control_boolean (super, CONTROL_PUBLISH, 
                "Publish", 1, 1); 
    self->lcm_name_ctl = cam_unit_add_control_string (super, CONTROL_CHANNEL,
            "Channel", "CAMLCM_IMAGE", 1);
    self->lcm_url_ctl = cam_unit_add_control_string (super, "lcm-url", 
            "LCM URL", DEFAULT_LCM_URL, 1);
    self->data_rate_ctl = cam_unit_add_control_string (super, 
            CONTROL_DATA_RATE_INFO, "Data Rate", "0", 0);

    self->data_rate = 0;
    self->bytes_transferred_since_last_data_rate_update = 0;
    self->last_data_rate_time = 0;
    self->data_rate_update_interval = DEFAULT_DATA_RATE_UPDATE_INTERVAL_USEC;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
camlcm_publish_finalize( GObject *obj )
{
    CamlcmPublish * self = CAMLCM_PUBLISH (obj);

    if (self->lcm) lcm_destroy (self->lcm);

    G_OBJECT_CLASS (camlcm_publish_parent_class)->finalize(obj);
}

static CamlcmPublish * 
camlcm_publish_new()
{
    CamlcmPublish * self = 
        CAMLCM_PUBLISH(g_object_new(CAMLCM_TYPE_PUBLISH, NULL));

    return self;
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (infmt) {
        cam_unit_add_output_format_full (super, infmt->pixelformat,
                infmt->name, infmt->width, infmt->height, 
                infmt->row_stride, infmt->max_data_size);
    }
    CamlcmPublish *self = CAMLCM_PUBLISH (super);
    self->bytes_transferred_since_last_data_rate_update = 0;
    self->data_rate = 0;
    self->last_data_rate_time = 0;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamlcmPublish *self = CAMLCM_PUBLISH(super);

    if (! cam_unit_control_get_boolean (self->publish_ctl)) {
        cam_unit_produce_frame (super, inbuf, infmt);
        cam_unit_control_force_set_string (self->data_rate_ctl, "0");
        return;
    }

    if (! self->lcm) {
        cam_unit_control_force_set_string (self->data_rate_ctl, "NO LCM");
        cam_unit_produce_frame (super, inbuf, infmt);
        return;
    }

    camlcm_image_t msg;
    msg.utime = inbuf->timestamp;
    msg.width = infmt->width;
    msg.height = infmt->height;
    msg.row_stride = infmt->row_stride;
    msg.pixelformat = infmt->pixelformat;
    msg.size = inbuf->bytesused;
    msg.data = inbuf->data;

    msg.nb = 0;
    msg.metadata_byte = NULL;

    GList *md_keys = cam_framebuffer_metadata_list_keys (inbuf);
    msg.nb = g_list_length (md_keys);
    camlcm_key_byte_t kbpairs[msg.nb];
    msg.metadata_byte = kbpairs;

    int i=0; 
    for (GList *kiter=md_keys; kiter; kiter=kiter->next) {
        int vlen = 0;
        const char *key = kiter->data;
        uint8_t *value = cam_framebuffer_metadata_get (inbuf, key, &vlen);
        kbpairs[i].key = (char*)key;
        kbpairs[i].n = vlen;
        kbpairs[i].value = value;
        i++;
    }
    g_list_free (md_keys);

    const char *channel = cam_unit_control_get_string (self->lcm_name_ctl);

    camlcm_image_t_publish (self->lcm, channel, &msg);

    int64_t now = timestamp_now ();
    if (now > self->next_announce_time) {
        camlcm_announce_t announce = {
            .utime = now,
            .width = infmt->width,
            .height = infmt->height,
            .stride = infmt->row_stride,
            .pixelformat = infmt->pixelformat,
            .max_data_size = infmt->max_data_size,
            .channel = (char*)channel
        };

        camlcm_announce_t_publish (self->lcm, CAMLCM_ANNOUNCE_CHANNEL,
                &announce);
        self->next_announce_time = now + self->announce_interval;
    }

    self->bytes_transferred_since_last_data_rate_update += 
        inbuf->bytesused;
    if (now > self->last_data_rate_time + self->data_rate_update_interval) {
        int64_t dt_usec = now - self->last_data_rate_time;
        double dt = dt_usec * 1e-6;
        double data_rate_instant = 
            self->bytes_transferred_since_last_data_rate_update * 1e-6 / dt;
        self->data_rate = data_rate_instant * DATA_RATE_UPDATE_ALPHA + 
            (1-DATA_RATE_UPDATE_ALPHA) * self->data_rate;

        char text[80];
        snprintf (text, sizeof (text), "%5.3f MB/s", self->data_rate);

        self->last_data_rate_time = now;
        self->bytes_transferred_since_last_data_rate_update = 0;

        cam_unit_control_force_set_string (self->data_rate_ctl, text);
    }

    cam_unit_produce_frame (super, inbuf, infmt);

    return;
}

static gboolean 
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamlcmPublish *self = CAMLCM_PUBLISH (super);
    if (ctl == self->lcm_url_ctl) {
        const char *url = g_value_get_string (proposed);
        if (self->lcm) {
            lcm_destroy (self->lcm);
        }
        self->lcm = lcm_create (url);
    } 

    g_value_copy (proposed, actual);
    return TRUE;
}
