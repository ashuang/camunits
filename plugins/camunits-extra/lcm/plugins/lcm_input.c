#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>

#include <camunits/cam.h>
#include <camunits/plugin.h>

#include <lcm/lcm.h>

#include "camlcm_image_announce_t.h"
#include "camlcm_image_t.h"

#include "lcm_publish.h"

#define err(...) fprintf (stderr, __VA_ARGS__)
//#define dbg(...) fprintf (stderr, args)
//#define dbgi(...) fprintf (stderr, args)
#define dbg(...)
#define dbgi(...)

typedef struct _CamlcmInput {
    CamUnit parent;

    CamUnitControl *channel_ctl;
    CamUnitControl *lcm_url_ctl;

    int read_fd;
    int write_fd;

    GThread *lcm_thread;
    GAsyncQueue *source_q;
    int thread_exit_requested;

    GMutex *mutex;
    // these members are thread-synchronized by the mutex
    char *lcm_url;
    char *channel;
    int unhandled_frame;
    camlcm_image_t *received_image;
} CamlcmInput;

typedef struct _CamlcmInputClass {
    CamUnitClass parent_class;
} CamlcmInputClass;

GType camlcm_input_driver_get_type (void);
GType camlcm_input_get_type (void);

static CamlcmInput * camlcm_input_new (void);

CAM_PLUGIN_TYPE (CamlcmInput, camlcm_input, CAM_TYPE_UNIT);

void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camlcm_input_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("lcm", "input",
            "LCM Input", 0, (CamUnitConstructor)camlcm_input_new, module);
}

// ============ LCM thread ========

static void
on_image (const lcm_recv_buf_t *rbuf, const char *channel, 
        const camlcm_image_t *image, void *user_data)
{
    // this method is always invoked from the LCM thread.
    CamlcmInput *self = (CamlcmInput*) user_data;
    g_mutex_lock (self->mutex);

    if (self->received_image) {
        camlcm_image_t_destroy(self->received_image);
    }
    self->received_image = camlcm_image_t_copy(image);

    if (! self->unhandled_frame) {
        // only write to the pipe if there isn't already data in it
        int wstatus = write (self->write_fd, " ", 1);
        if (1 != wstatus) perror ("input_lcm notify write");
        self->unhandled_frame = 1;
    }

    g_mutex_unlock (self->mutex);
}

static void *
lcm_thread (void *user_data)
{
    CamlcmInput *self = (CamlcmInput*)user_data;
    g_mutex_lock(self->mutex);
    char *lcm_url = strdup(self->lcm_url);
    char *channel = strdup(self->channel);

    camlcm_image_t_subscription_t *subscription = NULL;
    lcm_t *lcm = lcm_create(lcm_url);

    while (1) {
        // check to see if the LCM url has changed
        g_mutex_lock(self->mutex);
        if(strcmp(self->lcm_url, lcm_url)) {
            lcm_destroy(lcm);
            lcm = lcm_create(self->lcm_url);
            subscription = NULL;
            free(lcm_url);

            lcm_url = strdup(self->lcm_url);
            if(strlen(channel)) {
                subscription = camlcm_image_t_subscribe (lcm,
                        channel, on_image, self);
            }
        }

        // check to see if the channel has changed
        if(lcm && strcmp(self->channel, channel)) {
            free(channel);
            channel = strdup(self->channel);

            if(subscription)
                camlcm_image_t_unsubscribe(lcm, subscription);
            subscription = NULL;

            if(strlen(channel))
                subscription = camlcm_image_t_subscribe (lcm,
                        channel, on_image, self);
        }

        g_mutex_unlock(self->mutex);

        // don't try to do anything if there is not LCM network available
        if(!lcm) {
            struct timespec tv = {
                .tv_sec = 0,
                .tv_nsec = 300 * 1000 * 1000
            };
            nanosleep(&tv, NULL);
            continue;
        }

        struct pollfd pfd = {
            .fd = lcm_get_fileno(lcm),
            .events = POLLIN, 
            0
        };

        int status = poll (&pfd, 1, 300);

        if (self->thread_exit_requested) break;

        if (status > 0 && (pfd.events & POLLIN)) {
            lcm_handle (lcm);
        }
    }

    if(lcm) {
        lcm_destroy(lcm);
    }

    free(lcm_url);
    free(channel);
    dbg ("lcm thread exiting\n");
    return NULL;
}
// ===========================

// =====================================

/** 
 * Constructor.
 * don't call this function manually.  Instead, let the CamlcmInputDriver
 * call it.
 */
static void camlcm_input_finalize (GObject *obj);
static gboolean camlcm_input_try_produce_frame (CamUnit * super);
static gboolean _try_set_control (CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);
static int camlcm_input_get_fileno (CamUnit * super);
static void on_image (const lcm_recv_buf_t *rbuf, const char *channel, 
        const camlcm_image_t *ann, void *user_data);

static void
camlcm_input_init (CamlcmInput *self)
{
    CamUnit *super = CAM_UNIT(self);
    dbgi ("CamlcmInput: constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.

    self->read_fd = -1;
    self->write_fd = -1;
    self->source_q = NULL;
    self->lcm_thread = NULL;
    self->mutex = NULL;

    self->channel = strdup("CAMLCM_IMAGE");
    self->channel_ctl = cam_unit_add_control_string (super, "channel",
            "Channel", self->channel, 1);
    self->lcm_url = strdup("");
    self->lcm_url_ctl = cam_unit_add_control_string (super, "lcm-url", 
            "LCM URL", self->lcm_url, 1);

    // setup a notification pipe
    int fds[2];
    if (0 != pipe (fds)) {
        perror ("pipe");
        goto fail;
    }
    self->read_fd = fds[0];
    self->write_fd = fds[1];
    if (0 != fcntl (self->read_fd, F_SETFL, O_NONBLOCK) || 
        0 != fcntl (self->write_fd, F_SETFL, O_NONBLOCK)) {
        goto fail;
    }

    // start a new thread specifically for handling LCM requests
    if (!g_thread_supported ()) g_thread_init (NULL);
    self->thread_exit_requested = 0;
    self->source_q = g_async_queue_new ();
    self->mutex = g_mutex_new ();
    self->lcm_thread = g_thread_create (lcm_thread, self, TRUE, NULL);

    // set a dummy output format
    cam_unit_add_output_format_full (CAM_UNIT(self), CAM_PIXEL_FORMAT_GRAY, 
            "Dummy output format", 1, 1, 1);

    return;

fail:
    assert(!self->source_q);
    assert(!self->lcm_thread);
    assert(!self->mutex);
}

static void
camlcm_input_class_init (CamlcmInputClass *klass)
{
    dbgi ("CamlcmInput: class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = camlcm_input_finalize;
    klass->parent_class.try_produce_frame = camlcm_input_try_produce_frame;
    klass->parent_class.get_fileno = camlcm_input_get_fileno;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camlcm_input_finalize (GObject *obj)
{
    dbgi ("CamlcmInput: finalize\n");
    CamlcmInput * self = (CamlcmInput*) obj;

    if (self->lcm_thread) {
        dbg ("waiting for LCM thread to exit\n");
        self->thread_exit_requested = 1;
        g_thread_join (self->lcm_thread);
        dbg ("reaped LCM thread\n");
        self->lcm_thread = NULL;
    }

    if(self->received_image) {
        camlcm_image_t_destroy(self->received_image);
        self->received_image = NULL;
    }

    if (self->read_fd >= 0) {
        close (self->read_fd);
        self->read_fd = -1;
    }
    if (self->write_fd >= 0) {
        close (self->write_fd);
        self->write_fd = -1;
    }

    if (self->mutex) {
        g_mutex_free (self->mutex);
    }
    free(self->lcm_url);
    free(self->channel);

    G_OBJECT_CLASS (camlcm_input_parent_class)->finalize (obj);
}

static CamlcmInput * 
camlcm_input_new ()
{
    CamlcmInput *self = 
        (CamlcmInput*) (g_object_new (camlcm_input_get_type(), NULL));
    return self;
}

static int
camlcm_input_get_fileno (CamUnit * super)
{
    CamlcmInput *self = (CamlcmInput*) super;
    return self->read_fd;
}

static gboolean 
camlcm_input_try_produce_frame (CamUnit *super)
{
    dbgi ("[%s] iterate\n", cam_unit_get_name (super));
    CamlcmInput *self = (CamlcmInput*) super;
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    g_mutex_lock (self->mutex);

    // check if there's a new buffer available
    char c;
    if (1 != read (self->read_fd, &c, 1)) {
        dbgi ("CamlcmInput: expected read, but got (%s) %s:%d\n",
                strerror (errno), __FILE__, __LINE__);
        goto fail;
    }
    assert (self->unhandled_frame);
    if (1 == read (self->read_fd, &c, 1)) {
        err ("%s:%s Unexpected data in read pipe!\n", __FILE__, __FUNCTION__);
    }

    // new buffer is available.

    // has the output format changed?
    if(self->received_image->width != outfmt->width ||
       self->received_image->height != outfmt->height ||
       self->received_image->row_stride != outfmt->row_stride ||
       self->received_image->pixelformat != outfmt->pixelformat) {

        if (cam_unit_is_streaming(super)) {
            cam_unit_stream_shutdown (super);
        }
        cam_unit_remove_all_output_formats(super);
        cam_unit_add_output_format_full(super, 
                self->received_image->pixelformat, NULL,
                self->received_image->width,
                self->received_image->height, 
                self->received_image->row_stride);

        cam_unit_stream_init (super, NULL);
    }

    CamFrameBuffer *outbuf = cam_framebuffer_new(self->received_image->data,
            self->received_image->size);
    outbuf->timestamp = self->received_image->utime;
    outbuf->bytesused = self->received_image->size;
    for (int i=0; i<self->received_image->nmetadata; i++) {
        cam_framebuffer_metadata_set (outbuf, 
                self->received_image->metadata[i].key, 
                self->received_image->metadata[i].value, 
                self->received_image->metadata[i].n);
    }

    self->unhandled_frame = 0;

    g_mutex_unlock (self->mutex);

    cam_unit_produce_frame (super, outbuf, super->fmt);
    g_object_unref (outbuf);

    return TRUE;

fail:
    g_mutex_unlock (self->mutex);
    return FALSE;
}

static gboolean 
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamlcmInput *self = (CamlcmInput*) super;
    if (ctl == self->lcm_url_ctl) {
        const char *url = g_value_get_string (proposed);
        g_mutex_lock(self->mutex);
        free(self->lcm_url);
        self->lcm_url = strdup(url);
        g_strstrip(self->lcm_url);

        g_value_set_string(actual, self->lcm_url);
        g_mutex_unlock(self->mutex);
    } else if(ctl == self->channel_ctl) {
        const char *channel = g_value_get_string (proposed);
        g_mutex_lock(self->mutex);
        free(self->channel);
        self->channel = strdup(channel);
        g_strstrip(self->channel);

        g_value_set_string(actual, self->channel);
        g_mutex_unlock(self->mutex);
    }

    return TRUE;
}
