#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include <fcntl.h>
#include <poll.h>

#include <camunits/pixels.h>
#include <camunits/plugin.h>

#include <lcm/lcm.h>

#include "lcm_input_legacy.h"

//#define dbg(args...) fprintf (stderr, args)
#define dbg(...)
#define err(...) fprintf (stderr, __VA_ARGS__)

static void on_image (const lcm_recv_buf_t *rbuf, const char *channel, 
        const camlcm_image_legacy_t *msg, void *user_data);
static void change_lcm_channel (CamInputLegacy *self, const char *chan);
static void * lcm_thread (void *user_data);

// ============== CamInputLegacyDriver ===============

static CamUnit * driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc);

CAM_PLUGIN_TYPE (CamInputLegacy, cam_input_legacy, CAM_TYPE_UNIT);
CAM_PLUGIN_TYPE (CamInputLegacyDriver, cam_input_legacy_driver, 
        CAM_TYPE_UNIT_DRIVER);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    cam_input_legacy_driver_register_type (module);
    cam_input_legacy_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    CamInputLegacyDriver *self = 
        CAM_INPUT_LEGACY_DRIVER (
                g_object_new (CAM_INPUT_LEGACY_DRIVER_TYPE, NULL));
    return CAM_UNIT_DRIVER (self);
}

static void
cam_input_legacy_driver_init (CamInputLegacyDriver *self)
{
    dbg ("LCM legacy input driver constructor\n");
    CamUnitDriver *super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "lcm", "input_legacy");

    cam_unit_driver_add_unit_description (super, 
            "Legacy Input", NULL, CAM_UNIT_EVENT_METHOD_FD);
}

static void
cam_input_legacy_driver_class_init (CamInputLegacyDriverClass *klass)
{
    dbg ("LCM legacy input driver class initializer\n");
    klass->parent_class.create_unit = driver_create_unit;
}

CamInputLegacyDriver *
cam_input_legacy_driver_new ()
{
    return 
        CAM_INPUT_LEGACY_DRIVER (g_object_new (
                    CAM_INPUT_LEGACY_DRIVER_TYPE, NULL));
}

static CamUnit * 
driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc)
{
    dbg ("LCM legacy input driver creating new Log unit\n");

    const char *unit_id = cam_unit_description_get_unit_id(udesc);
    char **words = g_strsplit (unit_id, ":", 2);
    CamInputLegacy *result = cam_input_legacy_new (words[1]);
    g_strfreev (words);

    return CAM_UNIT (result);
}

// ============== CamInputLegacy ===============
static void _finalize (GObject *obj);
static int _stream_init (CamUnit *super, const CamUnitFormat *fmt);
static gboolean _try_produce_frame (CamUnit * super);
static int _get_fileno (CamUnit *super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);

static void
cam_input_legacy_init (CamInputLegacy *self)
{
    dbg ("LCM legacy input constructor\n");
    CamUnit *super = CAM_UNIT (self);

    self->channel_ctl = cam_unit_add_control_string (super, "channel", 
            "Channel", "", 1);

    self->lcm = lcm_create (NULL);
    if (!self->lcm) {
        err ("%s:%d -- Couldn't initialize LCM\n", __FILE__, __LINE__);
        // TODO: handle this error better?
        return;
    }

    if (!g_thread_supported ()) g_thread_init (NULL);
    self->thread_exit_requested = 0;
    self->lcm_thread = g_thread_create (lcm_thread, self, TRUE, NULL);

    self->sync_mutex = g_mutex_new ();

    if(0 != pipe (self->notify_pipe)) {
        perror("lcm_input_legacy - pipe");
        return;
    }
    fcntl (self->notify_pipe[1], F_SETFL, O_NONBLOCK);

    if(0 != pipe (self->frame_ready_pipe)) {
        perror("lcm_input_legacy - pipe");
        return;
    }
    fcntl (self->frame_ready_pipe[1], F_SETFL, O_NONBLOCK);

    self->subscription = NULL;

    self->has_legacy_uid = 0;
    self->legacy_uid = 0;
    self->new_image = NULL;

    // add a dummy output format
    cam_unit_add_output_format (CAM_UNIT (self), 
            CAM_PIXEL_FORMAT_GRAY, NULL, 1, 1, 1);
}

static void
cam_input_legacy_class_init (CamInputLegacyClass *klass)
{
    dbg ("LCM legacy input class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = _finalize;

    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.try_produce_frame = _try_produce_frame;
    klass->parent_class.get_fileno = _get_fileno;

    klass->parent_class.try_set_control = _try_set_control;
}

static void
_finalize (GObject *obj)
{
    dbg ("log finalize\n");
    CamInputLegacy *self = CAM_INPUT_LEGACY (obj);

    if (self->lcm_thread) {
        dbg ("waiting for LCM thread to exit\n");
        self->thread_exit_requested = 1;
        g_thread_join (self->lcm_thread);
        dbg ("reaped LCM thread\n");
        self->lcm_thread = NULL;
    }
    g_mutex_free (self->sync_mutex);

    G_OBJECT_CLASS (cam_input_legacy_parent_class)->finalize (obj);

    if (self->lcm) {
        if (self->subscription) {
            camlcm_image_legacy_t_unsubscribe (self->lcm, self->subscription);
        }
        lcm_destroy (self->lcm);
    }
    if (self->notify_pipe[0] >= 0) {
        close (self->notify_pipe[0]);
        close (self->notify_pipe[1]);
    }
    if (self->new_image) {
        camlcm_image_legacy_t_destroy (self->new_image);
    }

}

CamInputLegacy * 
cam_input_legacy_new (const char *channel)
{
    CamInputLegacy *self = 
        CAM_INPUT_LEGACY (g_object_new (CAM_INPUT_LEGACY_TYPE, NULL));
    if (channel && strlen (channel)) {
        change_lcm_channel (self, channel);
        cam_unit_control_force_set_string (self->channel_ctl, channel);
    }
    return self;
}

static inline int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static int
_stream_init (CamUnit *super, const CamUnitFormat *fmt)
{
    dbg ("LCM legacy input stream init\n");
//    CamInputLegacy *self = CAM_INPUT_LEGACY (super);
    return 0;
}

static gboolean 
_try_produce_frame (CamUnit *super)
{
    CamInputLegacy *self = CAM_INPUT_LEGACY (super);
    
    g_mutex_lock (self->sync_mutex);
    assert (self->sync_pending);
    if (self->sync_pending) {
        char c;
        if(1 != read (self->frame_ready_pipe[0], &c, 1)) {
            perror(__FILE__ " - read:");
            return FALSE;
        }
        self->sync_pending = 0;
    }

    camlcm_image_legacy_t *msg = self->new_image;
    self->new_image = NULL;
    g_mutex_unlock (self->sync_mutex);

    if (!msg) {
        return FALSE;
    }

    // if the format of the image received is not the current format of the
    // unit, then shutdown and restart the unit.
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (msg->width != outfmt->width ||
        msg->height != outfmt->height ||
        msg->stride != outfmt->row_stride ||
        msg->pixelformat != outfmt->pixelformat) {

        cam_unit_stream_shutdown (super);
        cam_unit_remove_all_output_formats (super);
        cam_unit_add_output_format (super, msg->pixelformat, NULL,
                msg->width, msg->height, msg->stride);
        cam_unit_stream_init (super, NULL);
        outfmt = cam_unit_get_output_format(super);
    }

    CamFrameBuffer *buf = cam_framebuffer_new (msg->image, msg->size);
    buf->timestamp = msg->utime;
    buf->bytesused = msg->size;
    const char *chan = cam_unit_control_get_string (self->channel_ctl);
    cam_framebuffer_metadata_set (buf, "lcm-channel", 
            (const uint8_t*)chan, strlen(chan) + 1);
    cam_unit_produce_frame (super, buf, outfmt);
    g_object_unref (buf);

    camlcm_image_legacy_t_destroy (msg);

    return TRUE;
}

static int
_get_fileno (CamUnit *super)
{
    return CAM_INPUT_LEGACY (super)->frame_ready_pipe[0];
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamInputLegacy *self = CAM_INPUT_LEGACY (super);

    if (ctl == self->channel_ctl) {
        change_lcm_channel (self, g_value_get_string (proposed));
        g_value_copy (proposed, actual);
        return TRUE;
    }
    return FALSE;
}

static void *
lcm_thread (void *user_data)
{
    CamInputLegacy *self = CAM_INPUT_LEGACY (user_data);
    while (1) {
        struct pollfd pfd = {
            .fd = lcm_get_fileno (self->lcm),
            .events = POLLIN, 
            0
        };
        int status = poll (&pfd, 1, 300);

        if (self->thread_exit_requested) break;

        if (status > 0 && (pfd.events & POLLIN)) {
            lcm_handle (self->lcm);
        }
    }
    return NULL;
}

static void 
_notify_frame_ready (CamInputLegacy *self, const camlcm_image_legacy_t *msg)
{
    g_mutex_lock (self->sync_mutex);
    if (self->new_image)
        camlcm_image_legacy_t_destroy (self->new_image);
    self->new_image = camlcm_image_legacy_t_copy (msg);
    if (!self->sync_pending) {
        char c = ' ';
        if(1 != write (self->frame_ready_pipe[1], &c, 1)) {
            perror(__FILE__ " - write");
        }
        self->sync_pending = 1;
    }
    g_mutex_unlock (self->sync_mutex);
}

static void
on_image (const lcm_recv_buf_t *rbuf, const char *channel, 
         const camlcm_image_legacy_t *msg, void *user_data)
{
    CamInputLegacy *self = CAM_INPUT_LEGACY (user_data);
    _notify_frame_ready (self, msg);
}

static void
change_lcm_channel (CamInputLegacy *self, const char *chan)
{
    if (self->subscription != NULL) {
        camlcm_image_legacy_t_unsubscribe (self->lcm, self->subscription);
        self->subscription = NULL;
    }
    if (!strlen (chan)) return;
    printf ("changing channel to %s\n", chan);
    self->subscription = camlcm_image_legacy_t_subscribe (self->lcm,
            chan, on_image, self);
}
