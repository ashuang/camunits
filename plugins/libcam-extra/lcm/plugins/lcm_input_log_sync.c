#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <poll.h>

#include <libcam/pixels.h>
#include <libcam/plugin.h>

#include <lcm/lcm.h>

#include "lcm_input_log_sync.h"

#define dbg(args...) fprintf (stderr, args)
//#define dbg(...)
#define err(...) fprintf (stderr, __VA_ARGS__)

//#define USE_ADV_MODE

static void on_sync (const lcm_recv_buf_t *rbuf, const char *channel, 
        const camlcm_image_sync_t *msg, void *user_data);
static void change_sync_channel (CamInputLogSync *self, const char *chan);
static void * lcm_thread (void *user_data);
static void on_legacy_sync (const lcm_recv_buf_t *rbuf, const char *channel, 
        const camlcm_image_legacy_sync_t *msg, void *user_data);

#ifdef USE_ADV_MODE
enum {
    ADVANCE_MODE_SYNC,
    ADVANCE_MODE_SOFT,
    ADVANCE_MODE_HARD,
};
#endif

// ============== CamInputLogSyncDriver ===============

static CamUnit * driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc);

static int _log_set_file (CamInputLogSync *self, const char *fname);

CAM_PLUGIN_TYPE (CamInputLogSync, cam_input_log_sync, CAM_TYPE_UNIT);
CAM_PLUGIN_TYPE (CamInputLogSyncDriver, cam_input_log_sync_driver, 
        CAM_TYPE_UNIT_DRIVER);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    cam_input_log_sync_driver_register_type (module);
    cam_input_log_sync_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    CamInputLogSyncDriver *self = 
        CAM_INPUT_LOG_SYNC_DRIVER (
                g_object_new (CAM_INPUT_LOG_SYNC_DRIVER_TYPE, NULL));
    return CAM_UNIT_DRIVER (self);
}

static void
cam_input_log_sync_driver_init (CamInputLogSyncDriver *self)
{
    dbg ("log driver constructor\n");
    CamUnitDriver *super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "lcm", "input_log");

    cam_unit_driver_add_unit_description (super, 
            "Synchronized Log", NULL, CAM_UNIT_EVENT_METHOD_FD);
}

static void
cam_input_log_sync_driver_class_init (CamInputLogSyncDriverClass *klass)
{
    dbg ("log driver class initializer\n");
    klass->parent_class.create_unit = driver_create_unit;
}

CamInputLogSyncDriver *
cam_input_log_sync_driver_new ()
{
    return 
        CAM_INPUT_LOG_SYNC_DRIVER (g_object_new (
                    CAM_INPUT_LOG_SYNC_DRIVER_TYPE, NULL));
}

static CamUnit * 
driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc)
{
    dbg ("log driver creating new Log unit\n");

    if (udesc->driver != super)
        return NULL;

    char **words = g_strsplit (udesc->unit_id, ":", 2);
    CamInputLogSync *result = cam_input_log_sync_new (words[1]);
    g_strfreev (words);

    return CAM_UNIT (result);
}

// ============== CamInputLogSync ===============
static void log_finalize (GObject *obj);
static int log_stream_init (CamUnit *super, const CamUnitFormat *fmt);
static gboolean log_try_produce_frame (CamUnit * super);
static int _get_fileno (CamUnit *super);
static gboolean log_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);

static void
cam_input_log_sync_init (CamInputLogSync *self)
{
    dbg ("log constructor\n");
    CamUnit *super = CAM_UNIT (self);
    self->camlog = NULL;

    self->next_frame_time = 0;
    self->nframes = 0;
    self->readone = 0;

    self->fname_ctl = cam_unit_add_control_string (super, "filename", 
            "Filename", "", 1);
    cam_unit_control_set_ui_hints (self->fname_ctl, CAM_UNIT_CONTROL_FILENAME);

//    self->sync_ctl = cam_unit_add_control_boolean (super, "sync", "Sync", 1, 1);
    self->sync_channel_ctl = cam_unit_add_control_string (super, "channel",
            "Channel", "CAMLCM_SYNC", 1);

#ifdef USE_ADV_MODE
    // advance mode
    const char *adv_mode_options[] = { 
        "Synchronize",
        "Never skip frames", 
        "Skip if too slow",
        NULL 
    };
    int adv_mode_options_enabled[] = { 1, 1, 1, 0 };

    self->adv_mode_ctl = cam_unit_add_control_enum (super,
            "mode", "Mode", ADVANCE_MODE_SYNC, 1, 
            adv_mode_options, adv_mode_options_enabled);

    // advance speed
    self->adv_speed_ctl = cam_unit_add_control_float (super,
            "speed", "Playback Speed", 0.1, 20, 0.1, 1, 0);
#endif

    // frame number
    self->frame_ctl = cam_unit_add_control_int (super,
            "frame", "Frame", 0, 1, 1, 0, 0);

#ifdef USE_ADV_MODE
    // pause
    self->pause_ctl = cam_unit_add_control_boolean (super,
            "pause", "Pause", 0, 1);
#endif

    self->lcm = lcm_create ("udpm://?recv_buf_size=2000000");
    if (!self->lcm) {
        err ("%s:%d -- Couldn't initialize LCM\n", __FILE__, __LINE__);
        // TODO: handle this error better?
        return;
    }

    if (!g_thread_supported ()) g_thread_init (NULL);
    self->thread_exit_requested = 0;
    self->lcm_thread = g_thread_create (lcm_thread, self, TRUE, NULL);

    self->sync_mutex = g_mutex_new ();

    pipe (self->notify_pipe);
    fcntl (self->notify_pipe[1], F_SETFL, O_NONBLOCK);

    pipe (self->frame_ready_pipe);
    fcntl (self->frame_ready_pipe[1], F_SETFL, O_NONBLOCK);

    self->subscription = NULL;
    self->legacy_subscription = camlcm_image_legacy_sync_t_subscribe(self->lcm,
           "IMAGE_LCSYNC", on_legacy_sync, self);

    self->has_legacy_uid = 0;
    self->legacy_uid = 0;
    self->sync_utime = 0;
}

static void
cam_input_log_sync_class_init (CamInputLogSyncClass *klass)
{
    dbg ("log class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = log_finalize;

    klass->parent_class.stream_init = log_stream_init;
    klass->parent_class.try_produce_frame = log_try_produce_frame;
    klass->parent_class.get_fileno = _get_fileno;

    klass->parent_class.try_set_control = log_try_set_control;
}

static void
log_finalize (GObject *obj)
{
    dbg ("log finalize\n");
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (obj);

    if (self->lcm_thread) {
        dbg ("waiting for LCM thread to exit\n");
        self->thread_exit_requested = 1;
        g_thread_join (self->lcm_thread);
        dbg ("reaped LCM thread\n");
        self->lcm_thread = NULL;
    }
    g_mutex_free (self->sync_mutex);

    if (self->camlog) { cam_log_destroy (self->camlog); }
    G_OBJECT_CLASS (cam_input_log_sync_parent_class)->finalize (obj);

    if (self->lcm) {
        camlcm_image_sync_t_unsubscribe (self->lcm, self->subscription);
        camlcm_image_legacy_sync_t_unsubscribe (self->lcm, 
                self->legacy_subscription);
        lcm_destroy (self->lcm);
    }
    if (self->notify_pipe[0] >= 0) {
        close (self->notify_pipe[0]);
        close (self->notify_pipe[1]);
    }

}

CamInputLogSync * 
cam_input_log_sync_new (const char *fname)
{
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (g_object_new (CAM_INPUT_LOG_SYNC_TYPE, NULL));

    if (fname && strlen (fname)) {
        if (0 == _log_set_file (self, fname)) {
            cam_unit_control_force_set_string (self->fname_ctl, fname);
        }
    }
    return self;
}

static int 
_log_set_file (CamInputLogSync *self, const char *fname)
{
    CamUnit *super = CAM_UNIT (self);
    if (self->camlog) cam_log_destroy (self->camlog);
    cam_unit_remove_all_output_formats (super);

    self->camlog = cam_log_new (fname, "r");
    if (!self->camlog) {
        goto fail;
    }

    CamLogFrameFormat format;
    if (cam_log_get_frame_format (self->camlog, &format) < 0) {
        goto fail;
    }
     
    // check if the log file has a DC1394 UID stored.  This is for legacy
    // logfile support
    CamFrameBuffer *fbuf = cam_log_get_frame (self->camlog);
    int log_uid_len = 0;
    const uint8_t *log_uid = cam_framebuffer_metadata_get (fbuf,
            "Source GUID", &log_uid_len);
    if (log_uid && sscanf((char*)log_uid, "0x%"PRIx64, &self->legacy_uid)) {
        self->has_legacy_uid = 1;
    } else {
        self->has_legacy_uid = 0;
    }
    g_object_unref (fbuf);


    int max_data_size;
    if (cam_pixel_format_nicknameide_meaningful (format.pixelformat))
        max_data_size = format.height * format.stride;
    else
        max_data_size = format.width * format.height * 4;

    cam_unit_add_output_format_full (super, format.pixelformat, NULL, 
            format.width, format.height, format.stride, max_data_size);

    self->nframes = cam_log_count_frames (self->camlog);
    cam_unit_control_modify_int (self->frame_ctl, 0, self->nframes-1, 1, 1);
    cam_unit_control_force_set_int (self->frame_ctl, 0);

#ifdef USE_ADV_MODE
    if (ADVANCE_MODE_SYNC == cam_unit_control_get_enum (self->adv_mode_ctl)) {
        cam_unit_control_set_enabled (self->frame_ctl, 0);
    }
#else
    cam_unit_control_set_enabled (self->frame_ctl, 0);
#endif

    return 0;

fail:
    if (self->camlog) {
        cam_log_destroy (self->camlog);
        self->camlog = NULL;
    }
    cam_unit_control_modify_int (self->frame_ctl, 0, 1, 1, 0);
    return -1;
}

static inline int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static int
log_stream_init (CamUnit *super, const CamUnitFormat *fmt)
{
    dbg ("log stream init\n");
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (super);

    self->next_frame_time = _timestamp_now ();
    return 0;
}

static gboolean 
log_try_produce_frame (CamUnit *super)
{
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (super);

    g_mutex_lock (self->sync_mutex);
    int64_t seek_to_utime = -1;
    if (self->sync_pending) {
        char c;
        read (self->frame_ready_pipe[0], &c, 1);
        self->sync_pending = 0;
        seek_to_utime = self->sync_utime;
    }
    g_mutex_unlock (self->sync_mutex);

    if (seek_to_utime < 0) 
        return FALSE;

    printf ("try produce frame %"PRId64"\n", seek_to_utime);
    if (0 != cam_log_seek_to_timestamp (self->camlog, seek_to_utime)) {
        return FALSE;
    }

//    CamLogFrameInfo cur_info;
//    if (0 != cam_log_get_frame_info (self->camlog, &cur_info)) {
//        dbg ("InputLogSync EOF?\n");
//        self->next_frame_time = now + 1000000;
//        return FALSE;
//    }
//
//    int paused = cam_unit_control_get_boolean (self->pause_ctl);
//    if (paused && ! self->readone) { 
//        dbg ("InputLogSync paused\n");
//        self->next_frame_time = now + 300000;
//        return FALSE; 
//    }
//    if (paused && self->readone) {
//        dbg("InputLogSync paused, but reading one frame\n");
//    }
//
//
//    int sync = cam_unit_control_get_boolean (self->sync_ctl);
//
//    double speed = cam_unit_control_get_float (self->adv_speed_ctl);
//    int advance_mode = cam_unit_control_get_enum (self->adv_mode_ctl);
//    // check the next frame and see if we should skip the current frame
//    // however, don't skip frames when paused
//    if (!sync && (advance_mode == CAM_INPUT_LOG_SYNC_ADVANCE_MODE_HARD) &&
//        (!paused)) {
//        while (0 == cam_log_next_frame (self->camlog)) {
//            CamLogFrameInfo next_info;
//
//            cam_log_get_frame_info (self->camlog, &next_info);
//
//            int64_t dt = (int64_t) ((next_info.timestamp - 
//                                     cur_info.timestamp) / speed);
//            int64_t expected_play_utime = self->next_frame_time + dt;
//
//            if (expected_play_utime <= now) {
//                cur_info = next_info;
//                self->next_frame_time = expected_play_utime;
//                break;
//            }
//        }
//        cam_log_seek_to_offset (self->camlog, cur_info.offset);
//    }
//
    CamFrameBuffer * buf = cam_log_get_frame (self->camlog);
    if (!buf) {
        return FALSE;
    }
    CamLogFrameInfo frameinfo;
    cam_log_get_frame_info (self->camlog, &frameinfo);
    cam_unit_control_force_set_int (self->frame_ctl, frameinfo.frameno);

//    // what is the timestamp of the next frame?
//    if (!sync) {
//        int have_next_frame = (0 == cam_log_next_frame (self->camlog));
//        if (! have_next_frame) {
//            self->next_frame_time = now + 300000;
//        } else {
//            // diff log timestamp that with the timestamp of the current
//            // frame to get the next frame event time
//            CamLogFrameInfo next_frameinfo;
//            cam_log_get_frame_info (self->camlog, &next_frameinfo);
//            int64_t dt =  (int64_t)((next_frameinfo.timestamp -
//                                     frameinfo.timestamp) / speed);
//
//            self->next_frame_time = now + dt;
//            dbg ("usec until next frame: %"PRId64"\n", dt);
//        }
//
//        cam_unit_control_force_set_int (self->frame_ctl, frameinfo.frameno);
//    } else {
//        CamLogFrameInfo next_frameinfo;
//        cam_log_get_frame_info (self->camlog, &next_frameinfo);
//        self->next_frame_time = now +
//            (next_frameinfo.timestamp - frameinfo.timestamp);
//    }
//
//    self->readone = 0;
    cam_unit_produce_frame (super, buf, super->fmt);
    g_object_unref (buf);
    return TRUE;
}

static int
_get_fileno (CamUnit *super)
{
    return CAM_INPUT_LOG_SYNC (super)->frame_ready_pipe[0];
}

static gboolean
log_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (super);

    if (ctl == self->fname_ctl) {
        if (super->is_streaming) {
            cam_unit_stream_shutdown (super);
        }
        const char *fname = g_value_get_string (proposed);
        int fstatus = _log_set_file (self, fname);
        if (0 == fstatus) {
            cam_unit_stream_init (super, NULL);
            g_value_copy (proposed, actual);
            return TRUE;
        } else {
            g_value_set_string (actual, "");
            return FALSE;
        }
    } else if (ctl == self->sync_channel_ctl) {
        g_value_copy (proposed, actual);
        const char *chan = g_value_get_string (proposed);
        change_sync_channel (self, chan);
        return TRUE;
    }
#ifdef USE_ADV_MODE
    else if (ctl == self->pause_ctl) {
        g_value_copy (proposed, actual);
        int paused = g_value_get_boolean (proposed);

        if (! paused) {
            self->next_frame_time = _timestamp_now ();
        } 
        return TRUE;
    } 
    else if (ctl == self->frame_ctl) {
        int next_frameno = g_value_get_int (proposed);
        if (! self->camlog) return FALSE;

        dbg ("seeking to frame %d\n", next_frameno);
        if (cam_log_seek_to_frame (self->camlog, next_frameno) == 0) {
            g_value_set_int (actual, next_frameno);
            self->next_frame_time = _timestamp_now ();
            self->readone = 1;
        }
        return TRUE;
    } else if (ctl == self->adv_speed_ctl) {
        g_value_copy (proposed, actual);
        return TRUE;
    }
    else if (ctl == self->adv_mode_ctl) {
        g_value_copy (proposed, actual);

        int enable_frame_ctls = 
            ADVANCE_MODE_SYNC != g_value_get_int (proposed);

        cam_unit_control_set_enabled (self->frame_ctl, enable_frame_ctls);
        cam_unit_control_set_enabled (self->adv_speed_ctl, enable_frame_ctls);
//        cam_unit_control_set_enabled (self->sync_channel_ctl, !enable_frame_ctls);

        return TRUE;
    }
#endif
    return FALSE;
}



static void *
lcm_thread (void *user_data)
{
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (user_data);
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
on_sync (const lcm_recv_buf_t *rbuf, const char *channel, 
         const camlcm_image_sync_t *msg, void *user_data)
{
    printf ("TODO!!\n");
//    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (user_data);
//    if (self->camlog) {
//        if (0 == cam_log_seek_to_timestamp (self->camlog, msg->utime)) {
//            self->readone = 1;
//            self->next_frame_time = _timestamp_now();
//        }
//    }
}

static void 
on_legacy_sync (const lcm_recv_buf_t *rbuf, const char *channel, 
        const camlcm_image_legacy_sync_t *msg, void *user_data)
{
    CamInputLogSync *self = CAM_INPUT_LOG_SYNC (user_data);

    if (self->has_legacy_uid && msg->source_uid == self->legacy_uid) {
        g_mutex_lock (self->sync_mutex);
        self->sync_utime = msg->utime;
        if (!self->sync_pending) {
            char c = ' ';
            write (self->frame_ready_pipe[1], &c, 1);
            self->sync_pending = 1;
        }
        g_mutex_unlock (self->sync_mutex);
    }
}

static void
change_sync_channel (CamInputLogSync *self, const char *chan)
{
    if (self->subscription != NULL) {
        camlcm_image_sync_t_unsubscribe (self->lcm, self->subscription);
    }
    self->subscription = camlcm_image_sync_t_subscribe (self->lcm,
            chan, on_sync, self);
}
