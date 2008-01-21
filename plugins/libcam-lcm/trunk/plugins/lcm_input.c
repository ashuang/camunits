#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>

#include <libcam/cam.h>
#include <libcam/plugin.h>

#include <lcm/lcm.h>

#include <camlcm_announce_t.h>
#include <camlcm_image_t.h>

#include "lcm_input.h"
#include "lcm_publish.h"

#define err(...) fprintf (stderr, __VA_ARGS__)
//#define dbg(...) fprintf (stderr, args)
//#define dbgi(...) fprintf (stderr, args)
#define dbg(...)
#define dbgi(...)

static void driver_finalize (GObject *obj);
static int on_announce (const char *channel, 
        const camlcm_announce_t *msg, void *user_data);
static void * lcm_thread (void *user_data);
static gboolean check_for_new_unit_descriptions_timer (void *user_data);
static CamUnit * driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc);
static CamUnitDescription * driver_search_unit_description(
        CamUnitDriver *driver, const char *id);

static CamlcmInput * 
camlcm_input_new (lcm_t *lcm, const camlcm_announce_t *ann);

CAM_PLUGIN_TYPE (CamlcmInput, camlcm_input, CAM_TYPE_UNIT);
CAM_PLUGIN_TYPE (CamlcmInputDriver, camlcm_input_driver, 
        CAM_TYPE_UNIT_DRIVER);

void
cam_plugin_initialize (GTypeModule * module)
{
    camlcm_input_driver_register_type (module);
    camlcm_input_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    CamlcmInputDriver *self = 
        CAMLCM_INPUT_DRIVER (
                g_object_new (CAMLCM_TYPE_INPUT_DRIVER, NULL));

    // create LCM object
    self->lcm = lcm_create ();
    if (!self->lcm) {
        err ("%s:%d -- Couldn't initialize LCM\n", __FILE__, __LINE__);
        goto fail;
    }

    lcm_params_t lcp;
    lcm_params_init_defaults (&lcp);

    // request a really big kernel receive buffer
    lcp.recv_buf_size = 2000000; 

    // initialize LCM
    if (0 != lcm_init (self->lcm, &lcp)) {
        err ("%s:d -- Couldn't initialize LCM\n", __FILE__, __LINE__);
        goto fail;
    }

    self->subscription = camlcm_announce_t_subscribe (self->lcm,
            CAMLCM_ANNOUNCE_CHANNEL, 
            on_announce, self);

    // start a new thread specifically for handling LC requests
    if (!g_thread_supported ()) g_thread_init (NULL);
    self->thread_exit_requested = 0;
    self->source_q = g_async_queue_new ();
    self->lcm_thread = g_thread_create (lcm_thread, self, TRUE, NULL);

    // set a glib timer to periodically check for new image source
    // announcements
    g_timeout_add (300, check_for_new_unit_descriptions_timer, self);

    return CAM_UNIT_DRIVER (self);
fail:
    g_object_unref (self);
    return NULL;
}

static void
camlcm_input_driver_init (CamlcmInputDriver *self)
{
    CamUnitDriver *super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "input", "LCM");
    
    self->lcm = NULL;
    self->known_sources = g_hash_table_new_full (g_str_hash, g_str_equal,
            free, (GDestroyNotify) camlcm_announce_t_destroy);
}

static void
camlcm_input_driver_class_init (CamlcmInputDriverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = driver_finalize;
    klass->parent_class.create_unit = driver_create_unit;
    klass->parent_class.search_unit_description = 
        driver_search_unit_description;
}

static void
driver_finalize (GObject *obj)
{
    CamlcmInputDriver * self = CAMLCM_INPUT_DRIVER (obj);
    if (self->lcm_thread) {
        dbg ("waiting for LCM thread to exit\n");
        self->thread_exit_requested = 1;
        g_thread_join (self->lcm_thread);
        dbg ("reaped LCM thread\n");
        self->lcm_thread = NULL;
    }
    if (self->source_q) {
        for (camlcm_announce_t *ann = g_async_queue_try_pop (self->source_q);
             ann != NULL;
             ann = g_async_queue_try_pop (self->source_q)) {
            camlcm_announce_t_destroy (ann);
        }
        g_async_queue_unref (self->source_q);
        self->source_q = NULL;
    }
    if (self->lcm) {
        camlcm_announce_t_unsubscribe (self->lcm, self->subscription);
        lcm_destroy (self->lcm);
    }
    if (self->known_sources) {
        g_hash_table_destroy (self->known_sources);
    }
}

static CamUnit * 
driver_create_unit (CamUnitDriver *super, const CamUnitDescription * udesc)
{
    dbg ("creating new unit [%s]\n", udesc->unit_id);

    CamlcmInputDriver *self = CAMLCM_INPUT_DRIVER (super);
    camlcm_announce_t *ann = (camlcm_announce_t*)
        g_hash_table_lookup (self->known_sources, udesc->unit_id);
    if (!ann) {
        dbg ("can't find camlcm_announce_t matching %s\n",
                udesc->unit_id);
        return NULL;
    }

    CamlcmInput *result = camlcm_input_new (self->lcm, ann);
    return CAM_UNIT (result);
}

static CamUnitDescription *
check_for_unit_description (CamlcmInputDriver *self, const char *unit_id)
{
    camlcm_input_driver_check_for_new_units (self);
    for (GList *uditer=CAM_UNIT_DRIVER(self)->udescs; uditer; 
            uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        if (!strcmp (udesc->unit_id, unit_id)) {
            return udesc;
        }
    }
    return NULL;
}

static CamUnitDescription * 
driver_search_unit_description (CamUnitDriver *super, 
        const char *id)
{
    CamlcmInputDriver *self = CAMLCM_INPUT_DRIVER (super);

    if (0 != strncmp (id, "input.LCM:", strlen ("input.LCM:"))) return NULL;

    CamUnitDescription *udesc = check_for_unit_description (self, id);
    for (int i = 0; i < 10 && !udesc; i++) {
        // XXX hack
        struct timespec towait = { 
            .tv_sec = 0, 
            .tv_nsec = CAMLCM_PUBLISH_DEFAULT_ANNOUNCE_INTERVAL_USEC * 1000
        };
        nanosleep (&towait, NULL);
        udesc = check_for_unit_description (self, id);
    }

    return udesc;
}

static gboolean
check_for_new_unit_descriptions_timer (void *user_data)
{
    CamlcmInputDriver *self = CAMLCM_INPUT_DRIVER (user_data);
    camlcm_input_driver_check_for_new_units (self);
    return TRUE;
}

void
camlcm_input_driver_check_for_new_units (CamlcmInputDriver *self)
{
    camlcm_announce_t *msg = g_async_queue_try_pop (self->source_q);
    while (msg) {

        char unit_id[1024];
        snprintf (unit_id, sizeof (unit_id), "input.LCM:%s", msg->channel);

        camlcm_announce_t *old_announce = 
            (camlcm_announce_t*) g_hash_table_lookup (self->known_sources, 
                    unit_id);

        // if we already have an announcement for this image source, check to
        // see if any of the parameters have changed
        if (old_announce) {
            if (old_announce->width != msg->width ||
                    old_announce->height != msg->height ||
                    old_announce->stride != msg->stride ||
                    old_announce->pixelformat != msg->pixelformat) {

                if (0 != cam_unit_driver_remove_unit_description (
                            CAM_UNIT_DRIVER (self), unit_id)) {
                    err ("ERROR: tried to remove unit description [%s] "
                            "but couldn't!\n", unit_id);
                }
                dbg ("removing changed unit description [%s]\n", unit_id);
                dbg ("   old: <%d, %d, %d> %s %s\n",
                        old_announce->width, old_announce->height,
                        old_announce->stride,
                        cam_pixel_format_str (old_announce->pixelformat),
                        old_announce->channel);
                dbg ("   new: <%d, %d, %d> %s %s\n",
                        msg->width, msg->height,
                        msg->stride,
                        cam_pixel_format_str (msg->pixelformat),
                        msg->channel);
                g_hash_table_remove (self->known_sources, msg->channel);
                old_announce = NULL;
            }
        }

        if (!old_announce) {
            dbg ("CamlcmInput: adding new unit description [%s]\n",
                    unit_id);
            cam_unit_driver_add_unit_description (CAM_UNIT_DRIVER (self),
                    msg->channel, msg->channel, 
                    CAM_UNIT_EVENT_METHOD_FD);
            g_hash_table_insert (self->known_sources, strdup (unit_id), msg);
        } else {
            // already have this announce, and it hasn't changed
            camlcm_announce_t_destroy (msg);
        }

        msg = g_async_queue_try_pop (self->source_q);
    }
}

// ============ LC thread ========
static int
on_announce (const char *channel, 
        const camlcm_announce_t *msg, void *user_data)
{
    CamlcmInputDriver *self = CAMLCM_INPUT_DRIVER (user_data);
    if (g_async_queue_length (self->source_q) < 500)
        g_async_queue_push (self->source_q, camlcm_announce_t_copy (msg));
    else {
        dbg ("source_q full: discarding announcement\n");
    }
    return 0;
}

static void *
lcm_thread (void *user_data)
{
    CamlcmInputDriver *self = CAMLCM_INPUT_DRIVER (user_data);
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
static int camlcm_input_stream_on (CamUnit * super);
static int camlcm_input_stream_off (CamUnit * super);
static int camlcm_input_get_fileno (CamUnit * super);
static int on_image (const char *channel, const camlcm_image_t *ann, 
        void *user_data);

static void
camlcm_input_init (CamlcmInput *self)
{
    dbgi ("CamlcmInput: constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.

    self->lcm = NULL;
    self->read_fd = -1;
    self->write_fd = -1;
    self->announce = NULL;
    self->subscription = NULL;

    self->buffer_mutex = g_mutex_new ();
    self->received_image = NULL;
}

static void
camlcm_input_class_init (CamlcmInputClass *klass)
{
    dbgi ("CamlcmInput: class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = camlcm_input_finalize;
    klass->parent_class.try_produce_frame = camlcm_input_try_produce_frame;
//    klass->parent_class.stream_init = camlcm_input_stream_init;
//    klass->parent_class.stream_shutdown = camlcm_input_shutdown;
    klass->parent_class.stream_on = camlcm_input_stream_on;
    klass->parent_class.stream_off = camlcm_input_stream_off;
    klass->parent_class.get_fileno = camlcm_input_get_fileno;
}

static void
camlcm_input_finalize (GObject *obj)
{
    dbgi ("CamlcmInput: finalize\n");
    CamlcmInput * self = CAMLCM_INPUT (obj);

    if (self->lcm) {
        // don't own the LC instance, just ignore it
        self->lcm = NULL;
    }
    if (self->read_fd >= 0) {
        close (self->read_fd);
        self->read_fd = -1;
    }
    if (self->write_fd >= 0) {
        close (self->write_fd);
        self->write_fd = -1;
    }
    if (self->announce) {
        camlcm_announce_t_destroy (self->announce);
    }
    if (self->received_image) {
        g_object_unref (self->received_image);
        self->received_image = NULL;
    }
    if (self->subscription) {
        err ("internal error %s:%d\n", __FILE__, __LINE__);
    }
    if (self->buffer_mutex) {
        g_mutex_free (self->buffer_mutex);
    }

    G_OBJECT_CLASS (camlcm_input_parent_class)->finalize (obj);
}

CamlcmInput * 
camlcm_input_new (lcm_t *lcm, const camlcm_announce_t *ann)
{
    CamlcmInput * self = CAMLCM_INPUT (g_object_new (CAMLCM_TYPE_INPUT, NULL));

    self->lcm = lcm;

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
        close (self->read_fd);
        close (self->write_fd);
        goto fail;
    }

    self->announce = camlcm_announce_t_copy (ann);

    // set unit output format
    cam_unit_add_output_format_full (CAM_UNIT (self), ann->pixelformat, NULL, 
            ann->width, ann->height, ann->stride, ann->max_data_size);

    return self;

fail:
    g_object_unref (self);
    return NULL;
}

static int
camlcm_input_stream_on (CamUnit *super)
{
    CamlcmInput *self = CAMLCM_INPUT (super);
    g_mutex_lock (self->buffer_mutex);
    self->unhandled_frame = 0;
    g_mutex_unlock (self->buffer_mutex);
    self->subscription = 
        camlcm_image_t_subscribe (self->lcm, self->announce->channel, 
                on_image, self);
    return 0;
}

static int
camlcm_input_stream_off (CamUnit *super)
{
    dbgi ("unsubscribing image handler\n");
    CamlcmInput *self = CAMLCM_INPUT (super);
    if (0 != camlcm_image_t_unsubscribe (self->lcm, self->subscription)) {
        dbgi ("error unsubscribing image handler\n");
    }
    self->subscription = NULL;
    g_mutex_lock (self->buffer_mutex);
    self->unhandled_frame = 0;
    g_mutex_unlock (self->buffer_mutex);
    return CAM_UNIT_CLASS (camlcm_input_parent_class)->stream_off (super);
}

static int
camlcm_input_get_fileno (CamUnit * super)
{
    CamlcmInput *self = CAMLCM_INPUT (super);
    return self->read_fd;
}

static gboolean 
camlcm_input_try_produce_frame (CamUnit *super)
{
    CamlcmInput *self = CAMLCM_INPUT (super);
    dbgi ("[%s] iterate\n", cam_unit_get_name (super));

    g_mutex_lock (self->buffer_mutex);

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
    CamFrameBuffer *outbuf = self->received_image;
    self->received_image = NULL;
    self->unhandled_frame = 0;

    g_mutex_unlock (self->buffer_mutex);

    cam_unit_produce_frame (super, outbuf, super->fmt);
    g_object_unref (outbuf);

    return TRUE;

fail:
    g_mutex_unlock (self->buffer_mutex);
    return FALSE;
}

static int
on_image (const char *channel, const camlcm_image_t *image, void *user_data)
{
    // this method is always invoked from the LCM thread.
    CamlcmInput *self = CAMLCM_INPUT (user_data);
    g_mutex_lock (self->buffer_mutex);

    if (self->received_image) {
        g_object_unref (self->received_image);
    }

    // TODO check that the image dimensions and pixelformat are what we expect
    // can't just check directly because of threading issues...

    self->received_image = cam_framebuffer_new_alloc (image->size);
    self->received_image->timestamp = image->utime;
    memcpy (self->received_image->data, image->data, image->size);
    self->received_image->bytesused = image->size;
    for (int i=0; i<image->nb; i++) {
        cam_framebuffer_metadata_set (self->received_image, 
                image->metadata_byte[i].key, 
                image->metadata_byte[i].value, 
                image->metadata_byte[i].n);
    }

    if (! self->unhandled_frame) {
        // only write to the pipe if there isn't already data in it
        int wstatus = write (self->write_fd, " ", 1);
        if (1 != wstatus) perror ("input_lcm notify write");
        self->unhandled_frame = 1;
    }

    g_mutex_unlock (self->buffer_mutex);
    return 0;
}
