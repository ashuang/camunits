#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pixels.h"

#include "input_log.h"
#include "dbg.h"

#define err(...) fprintf (stderr, __VA_ARGS__)

#define LOG_DRIVER_NAME "input.log"

// ============== CamInputLogDriver ===============

static CamUnit * driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc);
static CamUnitDescription * driver_search_unit_description(
        CamUnitDriver *driver, const char *id);

G_DEFINE_TYPE (CamInputLogDriver, cam_input_log_driver, CAM_TYPE_UNIT_DRIVER);

static void
cam_input_log_driver_init (CamInputLogDriver *self)
{
    dbg (DBG_DRIVER, "log driver constructor\n");
    CamUnitDriver *super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_package (super, LOG_DRIVER_NAME);
}

static void
cam_input_log_driver_class_init (CamInputLogDriverClass *klass)
{
    dbg (DBG_DRIVER, "log driver class initializer\n");
    klass->parent_class.create_unit = driver_create_unit;
    klass->parent_class.search_unit_description = 
        driver_search_unit_description;
}

CamInputLogDriver *
cam_input_log_driver_new ()
{
    return 
        CAM_INPUT_LOG_DRIVER (g_object_new (CAM_INPUT_LOG_DRIVER_TYPE, NULL));
}

static CamUnit * 
driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc)
{
    dbg (DBG_DRIVER, "log driver creating new Log unit\n");

    char **words = g_strsplit (udesc->unit_id, ":", 2);
    if (strcmp (words[0], LOG_DRIVER_NAME)) {
        dbg (DBG_DRIVER, "driver name [%s] did not match expected [%s]\n",
                words[0], LOG_DRIVER_NAME);
        g_strfreev (words);
        return NULL;
    }

    CamInputLog *result = cam_input_log_new (words[1]);
    g_strfreev (words);

    return CAM_UNIT (result);
}

static CamUnitDescription * 
driver_search_unit_description (CamUnitDriver *super, 
        const char *id)
{
//    CamInputLogDriver *self = CAM_INPUT_LOG_DRIVER (super);
    char **words = g_strsplit (id, ":", 2);
    if (strcmp (words[0], LOG_DRIVER_NAME)) {
        dbg (DBG_DRIVER, "driver name [%s] did not match expected [%s]\n",
                words[0], LOG_DRIVER_NAME);
        g_strfreev (words);
        return NULL;
    }
    if (g_file_test (words[1], G_FILE_TEST_EXISTS)) {
        CamUnitDescription *desc = 
            cam_unit_driver_add_unit_description (super,
                words[1], id, 
                CAM_UNIT_EVENT_METHOD_TIMEOUT);
        g_strfreev (words);
        return desc;
    }
    dbg (DBG_DRIVER, "file [%s] not found\n", words[1]);
    g_strfreev (words);

    return NULL;
}

// ============== CamInputLog ===============
static void log_finalize (GObject *obj);
static int log_stream_on (CamUnit *super);
static void log_try_produce_frame (CamUnit * super);
static int64_t log_get_next_event_time (CamUnit *super);
static gboolean log_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);

G_DEFINE_TYPE (CamInputLog, cam_input_log, CAM_TYPE_UNIT);

static void
cam_input_log_init (CamInputLog *self)
{
    dbg (DBG_INPUT, "log constructor\n");
    self->camlog = NULL;
    self->filename = NULL;

    self->next_frame_time = 0;
    self->nframes = 0;
    self->readone = 0;
    memset (&self->next_frameinfo, 0, sizeof (self->next_frameinfo));
}

static void
cam_input_log_class_init (CamInputLogClass *klass)
{
    dbg (DBG_INPUT, "log class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = log_finalize;

    klass->parent_class.stream_on = log_stream_on;
    klass->parent_class.try_produce_frame = log_try_produce_frame;
    klass->parent_class.get_next_event_time = 
        log_get_next_event_time;

    klass->parent_class.try_set_control = log_try_set_control;
}

static void
log_finalize (GObject *obj)
{
    dbg (DBG_INPUT, "log finalize\n");
    CamInputLog *self = CAM_INPUT_LOG (obj);

    if (self->camlog) { cam_log_destroy (self->camlog); }
    if (self->filename) { free (self->filename); }
    G_OBJECT_CLASS (cam_input_log_parent_class)->finalize (obj);
}

CamInputLog * 
cam_input_log_new (const char *fname)
{
    CamInputLog *self = CAM_INPUT_LOG (g_object_new (CAM_INPUT_LOG_TYPE, NULL));

    int status = cam_log_set_file (self, fname);
    if (0 != status) {
        g_object_unref (self);
        return NULL;
    }
 
    return self;
}

int 
cam_log_set_file (CamInputLog *self, const char *fname)
{
    if (self->camlog) return -1;

    self->camlog = cam_log_new (fname, "r");
    if (! self->camlog) return -1;

    cam_log_frame_info_t fmd;
    if (0 != cam_log_peek_next_frame_info (self->camlog, &fmd)) {
        cam_log_destroy (self->camlog);
        self->camlog = NULL;
        return -1;
    }

    int max_data_size;
    if (cam_pixel_format_stride_meaningful (fmd.pixelformat)) {
        max_data_size = fmd.height * fmd.stride;
    } else {
        max_data_size = fmd.width * fmd.height * 4;
    }

    CamUnit *super = CAM_UNIT (self);
    cam_unit_add_output_format_full (super, fmd.pixelformat, NULL, 
            fmd.width, fmd.height, fmd.stride, max_data_size);

    self->first_frameinfo = fmd;
    self->filename = strdup (fname);

    self->nframes = cam_log_count_frames (self->camlog);
    self->frame_ctl = cam_unit_add_control_int (super,
            "frame", "Frame", 0, self->nframes-1, 1, 0, 1);
    self->pause_ctl = cam_unit_add_control_boolean (super,
            "pause", "Pause", 0, 1);

    const char *adv_mode_options[] = { "Soft", "Hard", NULL };
    int adv_mode_options_enabled[] = { 1, 0, 0 };

    self->adv_mode_ctl = cam_unit_add_control_enum (super,
            "mode", "Mode", 0, 1, adv_mode_options, adv_mode_options_enabled);

    self->adv_speed_ctl = cam_unit_add_control_float (super,
            "speed", "Speed", 0.1, 20, 0.1, 1, 0);

    return 0;
}

static int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static int
log_stream_on (CamUnit *super)
{
    dbg (DBG_INPUT, "log stream on\n");
    CamInputLog *self = CAM_INPUT_LOG (super);

    cam_log_peek_next_frame_info (self->camlog, &self->next_frameinfo);

    self->next_frame_time = _timestamp_now ();

    cam_unit_set_status (super, CAM_UNIT_STATUS_STREAMING);
    return 0;
}

static void 
log_try_produce_frame (CamUnit *super)
{
    CamInputLog *self = CAM_INPUT_LOG (super);
    int64_t now = _timestamp_now ();
    int64_t late = now - self->next_frame_time;

    dbg (DBG_INPUT, "InputLog iterate [%"PRId64", %"PRId64", %"PRId64"]\n",
            now, self->next_frame_time, late);

    int advance_mode = cam_unit_control_get_enum (self->adv_mode_ctl);
    int paused = cam_unit_control_get_boolean (self->pause_ctl);

    switch (advance_mode) {
        case CAM_INPUT_LOG_ADVANCE_MODE_SOFT:
            // paused?
            if (paused && ! self->readone) { 
                dbg (DBG_INPUT, "InputLog paused\n");
                self->next_frame_time = now + 300000;
                return; 
            }
            if (paused && self->readone) {
                dbg(DBG_INPUT, "InputLog paused, but reading one frame\n");
            }
            break;
        case CAM_INPUT_LOG_ADVANCE_MODE_HARD:
            err ("NYI");
            // TODO
            return;
            break;
    }

    CamFrameBuffer *buf = cam_framebuffer_new_alloc (super->fmt->max_data_size);

    cam_log_frame_info_t frameinfo;
    if (0 != cam_log_read_next_frame (self->camlog, 
                &frameinfo, buf->data, buf->length)) {
        dbg (DBG_INPUT, "InputLog EOF?\n");
        self->next_frame_time = now + 1000000;
        g_object_unref (buf);
        return;
    }

    buf->source_uid = frameinfo.source_uid;
    buf->bytesused = frameinfo.datalen;
    buf->timestamp = frameinfo.timestamp;

    // what is the timestamp of the next frame?
    if (0 != cam_log_peek_next_frame_info (self->camlog, 
                &self->next_frameinfo)) {
        self->next_frame_time = now + 300000;
        // TODO handle EOF properly
    } else {
        // diff log timestamp that with the timestamp of the current
        // frame to get the next frame event time
        int64_t tdiff = 
            self->next_frameinfo.timestamp - frameinfo.timestamp;

        self->next_frame_time = now + tdiff;
        dbg (DBG_INPUT, "usec until next frame: %"PRId64"\n", tdiff);
    }

    cam_unit_control_force_set_int (self->frame_ctl, 
            self->next_frameinfo.frameno);

    self->readone = 0;
    dbg (DBG_INPUT, "pushing buffer\n");
    cam_unit_produce_frame (super, buf, super->fmt);
    g_object_unref (buf);
    return;
}

static int64_t
log_get_next_event_time (CamUnit *super)
{
    return CAM_INPUT_LOG (super)->next_frame_time;
}

static gboolean
log_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamInputLog *self = CAM_INPUT_LOG (super);
    if (ctl == self->pause_ctl) {
        g_value_copy (proposed, actual);
        int paused = g_value_get_boolean (proposed);

        if (! paused) {
            self->next_frame_time = _timestamp_now ();
        } 
        return TRUE;
    } else if (ctl == self->frame_ctl) {
        int next_frameno = g_value_get_int (proposed);
        g_value_set_int (actual, next_frameno);

        dbg (DBG_INPUT, "seeking to frame %d\n", next_frameno);

        cam_log_seek_to_frame (self->camlog, next_frameno);
        cam_log_peek_next_frame_info (self->camlog, &self->next_frameinfo);

        self->next_frame_time = _timestamp_now ();

        self->readone = 1;
        return TRUE;
    } else if (ctl == self->adv_mode_ctl) {
        g_value_copy (proposed, actual);

        return TRUE;
    }
    return FALSE;
}
