#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <camunits/log.h>
#include <camunits/plugin.h>
#include <camunits/dbg.h>

#define err(...) fprintf (stderr, __VA_ARGS__)

enum {
    CAM_INPUT_LOG_ADVANCE_MODE_SOFT = 0,
    CAM_INPUT_LOG_ADVANCE_MODE_HARD
};

typedef struct _CamInputLogDriver {
    CamUnitDriver parent;
} CamInputLogDriver;

typedef struct _CamInputLogDriverClass {
    CamUnitDriverClass parent_class;
} CamInputLogDriverClass;

typedef struct _CamInputLog {
    CamUnit parent;

    CamLog *camlog;

    int64_t next_frame_time;

    int nframes;

    int readone;

    CamUnitControl *frame_ctl;
    CamUnitControl *pause_ctl;
    CamUnitControl *adv_mode_ctl;
    CamUnitControl *adv_speed_ctl;
    CamUnitControl *fname_ctl;
    CamUnitControl *loop_ctl;
    CamUnitControl *loop_start_ctl;
    CamUnitControl *loop_end_ctl;
} CamInputLog;

typedef struct _CamInputLogClass {
    CamUnitClass parent_class;
} CamInputLogClass;

GType cam_input_log_driver_get_type (void);
GType cam_input_log_get_type (void);

static CamUnitDriver * cam_input_log_driver_new (void);
static CamInputLog * cam_input_log_new (const char *fname);

CAM_PLUGIN_TYPE(CamInputLogDriver, cam_input_log_driver, CAM_TYPE_UNIT_DRIVER);
CAM_PLUGIN_TYPE(CamInputLog, cam_input_log, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module);
void cam_plugin_initialize(GTypeModule * module)
{
    cam_input_log_driver_register_type(module);
    cam_input_log_register_type(module);
}

CamUnitDriver * cam_plugin_create(GTypeModule * module);
CamUnitDriver * cam_plugin_create(GTypeModule * module)
{
    return cam_input_log_driver_new();
}

// ============== CamInputLogDriver ===============

static CamUnit * driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc);

static int _log_set_file (CamInputLog *self, const char *fname);

static void
cam_input_log_driver_init (CamInputLogDriver *self)
{
    dbg (DBG_DRIVER, "log driver constructor\n");
    CamUnitDriver *super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "input", "log");

    cam_unit_driver_add_unit_description (super, 
            "Log Input", NULL, CAM_UNIT_EVENT_METHOD_TIMEOUT);
}

static void
cam_input_log_driver_class_init (CamInputLogDriverClass *klass)
{
    dbg (DBG_DRIVER, "log driver class initializer\n");
    klass->parent_class.create_unit = driver_create_unit;
}

CamUnitDriver *
cam_input_log_driver_new ()
{
    return 
        CAM_UNIT_DRIVER (g_object_new (cam_input_log_driver_get_type(), NULL));
}

static CamUnit * 
driver_create_unit (CamUnitDriver *super,
        const CamUnitDescription * udesc)
{
    dbg (DBG_DRIVER, "log driver creating new Log unit\n");

    g_assert (cam_unit_description_get_driver(udesc) == super);
    const char *unit_id = cam_unit_description_get_unit_id(udesc);

    char **words = g_strsplit (unit_id, ":", 2);
    CamInputLog *result = cam_input_log_new (words[1]);
    g_strfreev (words);

    return CAM_UNIT (result);
}

// ============== CamInputLog ===============
static void log_finalize (GObject *obj);
static int log_stream_init (CamUnit *super, const CamUnitFormat *fmt);
static gboolean log_try_produce_frame (CamUnit * super);
static int64_t log_get_next_event_time (CamUnit *super);
static gboolean log_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);

static void
cam_input_log_init (CamInputLog *self)
{
    dbg (DBG_INPUT, "log constructor\n");
    CamUnit *super = CAM_UNIT (self);
    self->camlog = NULL;

    self->next_frame_time = 0;
    self->nframes = 0;
    self->readone = 0;

    self->fname_ctl = cam_unit_add_control_string (super, "filename", 
            "Filename", "", 1);
    cam_unit_control_set_ui_hints (self->fname_ctl, CAM_UNIT_CONTROL_FILENAME);

    self->frame_ctl = cam_unit_add_control_int (super,
            "frame", "Frame", 0, 1, 1, 0, 0);
    self->pause_ctl = cam_unit_add_control_boolean (super,
            "pause", "Pause", 0, 1);

    CamUnitControlEnumValue adv_mode_entries[] = { 
        { CAM_INPUT_LOG_ADVANCE_MODE_SOFT, "Never skip frames", 1 },
        { CAM_INPUT_LOG_ADVANCE_MODE_HARD, "Skip if too slow", 1 },
        { 0, NULL, 0 }
    };

    self->adv_mode_ctl = cam_unit_add_control_enum (super, "mode", "Mode", 
            CAM_INPUT_LOG_ADVANCE_MODE_SOFT, 1, adv_mode_entries);

    self->adv_speed_ctl = cam_unit_add_control_float (super,
            "speed", "Playback Speed", 0.1, 20, 0.1, 1, 1);
    cam_unit_control_set_ui_hints(self->adv_speed_ctl, 
            CAM_UNIT_CONTROL_SPINBUTTON);

    self->loop_ctl = cam_unit_add_control_boolean(super,
            "loop", "Loop", 0, 1);
    self->loop_start_ctl = cam_unit_add_control_int(super, 
            "loop-start-frame", "Loop Start Frame", 0, 1, 1, 1, 0);
    self->loop_end_ctl = cam_unit_add_control_int(super, 
            "loop-end-frame", "Loop End Frame", 0, 1, 1, 1, 0);
    cam_unit_control_set_ui_hints(self->loop_start_ctl, 
            CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints(self->loop_end_ctl, 
            CAM_UNIT_CONTROL_SPINBUTTON);
}

static void
cam_input_log_class_init (CamInputLogClass *klass)
{
    dbg (DBG_INPUT, "log class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = log_finalize;

    klass->parent_class.stream_init = log_stream_init;
    klass->parent_class.try_produce_frame = log_try_produce_frame;
    klass->parent_class.get_next_event_time = 
        log_get_next_event_time;

    klass->parent_class.try_set_control = log_try_set_control;
}

static void
log_finalize (GObject *obj)
{
    dbg (DBG_INPUT, "log finalize\n");
    CamInputLog *self = (CamInputLog*)obj;

    if (self->camlog) { cam_log_destroy (self->camlog); }
    G_OBJECT_CLASS (cam_input_log_parent_class)->finalize (obj);
}

CamInputLog * 
cam_input_log_new (const char *fname)
{
    CamInputLog *self = (CamInputLog*) (g_object_new (cam_input_log_get_type(), NULL));

    if (fname && strlen (fname)) {
        if (0 == _log_set_file (self, fname)) {
            cam_unit_control_force_set_string (self->fname_ctl, fname);
        }
    }
    return self;
}

static int 
_log_set_file (CamInputLog *self, const char *fname)
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

    cam_unit_add_output_format (super, format.pixelformat, NULL, 
            format.width, format.height, format.stride);

    self->nframes = cam_log_count_frames (self->camlog);
    int maxframe = self->nframes - 1;
    cam_unit_control_modify_int (self->frame_ctl, 0, maxframe, 1, 1);
    cam_unit_control_force_set_int (self->frame_ctl, 0);

    cam_unit_control_modify_int(self->loop_start_ctl, 0, maxframe, 1, 0);
    cam_unit_control_modify_int(self->loop_end_ctl, 0, maxframe, 1, 0);
    cam_unit_control_force_set_int(self->loop_start_ctl, 0);
    cam_unit_control_force_set_int(self->loop_end_ctl, maxframe);

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
    dbg (DBG_INPUT, "log stream init\n");
    CamInputLog *self = (CamInputLog*)super;

    self->next_frame_time = _timestamp_now ();
    return 0;
}

static gboolean 
log_try_produce_frame (CamUnit *super)
{
    CamInputLog *self = (CamInputLog*)super;
    int64_t now = _timestamp_now ();
    if (now < self->next_frame_time) return FALSE;

    int64_t late = now - self->next_frame_time;

    dbg (DBG_INPUT, "InputLog iterate [%"PRId64", %"PRId64", %"PRId64"]\n",
            now, self->next_frame_time, late);

    int paused = cam_unit_control_get_boolean (self->pause_ctl);
    double speed = cam_unit_control_get_float (self->adv_speed_ctl);

    CamLogFrameInfo cur_info;
    if (0 != cam_log_get_frame_info (self->camlog, &cur_info)) {
        dbg (DBG_INPUT, "InputLog EOF?\n");
        self->next_frame_time = now + 1000000;
        return FALSE;
    }

    if (paused && ! self->readone) { 
        dbg (DBG_INPUT, "InputLog paused\n");
        self->next_frame_time = now + 300000;
        return FALSE; 
    }
    if (paused && self->readone) {
        dbg(DBG_INPUT, "InputLog paused, but reading one frame\n");
    }

    int advance_mode = cam_unit_control_get_enum (self->adv_mode_ctl);
    // check the next frame and see if we should skip the current frame
    // however, don't skip frames when paused
    if (advance_mode == CAM_INPUT_LOG_ADVANCE_MODE_HARD && (! paused)) {
        CamLogFrameInfo new_cur_info;
        memcpy(&new_cur_info, &cur_info, sizeof(new_cur_info));
        int nskipped = 0;
        while (0 == cam_log_next_frame (self->camlog)) {
            CamLogFrameInfo next_info;

            cam_log_get_frame_info (self->camlog, &next_info);

            int64_t dt = (int64_t) ((next_info.timestamp - 
                        cur_info.timestamp) / speed);

            // given the playback speed, when would we expect to play this
            // frame?
            int64_t expected_play_utime = self->next_frame_time + dt;

            // if it's in the past, then mark it as the next frame to play, and
            // then see if we should skip it
            if(expected_play_utime <= now) {
                memcpy(&new_cur_info, &next_info, sizeof(new_cur_info));
                nskipped ++;
            } else {
                // if it's in the future, then stop looking.
                break;
            }
        }
        if(nskipped) {
            dbg(DBG_INPUT, "skipped %d frames\n", nskipped);
        }
        cam_log_seek_to_offset (self->camlog, new_cur_info.offset);
    }

    CamFrameBuffer * buf = cam_log_get_frame (self->camlog);
    if (!buf) {
        dbg (DBG_INPUT, "InputLog EOF?\n");
        self->next_frame_time = now + 1000000;
        return FALSE;
    }
    CamLogFrameInfo frameinfo;
    cam_log_get_frame_info (self->camlog, &frameinfo);

    // should we loop?
    int loop = cam_unit_control_get_boolean(self->loop_ctl);
    int just_looped = 0;
    if(loop) {
        int loop_end = cam_unit_control_get_int(self->loop_end_ctl);
        int loop_start = cam_unit_control_get_int(self->loop_start_ctl);

        if(frameinfo.frameno >= loop_end || frameinfo.frameno < loop_start) {
            cam_log_seek_to_frame(self->camlog, loop_start);
            just_looped = 1;
        }
    }

    // what is the timestamp of the next frame?
    int have_next_frame = (0 == cam_log_next_frame (self->camlog));
    if (! have_next_frame) {
        self->next_frame_time = now + 300000;
    } else {
        // diff log timestamp that with the timestamp of the current
        // frame to get the next frame event time
        CamLogFrameInfo next_frameinfo;
        cam_log_get_frame_info (self->camlog, &next_frameinfo);

        int64_t frame_dt_usec = next_frameinfo.timestamp - frameinfo.timestamp;
        int64_t dt_usec = (int64_t)((int)frame_dt_usec / speed);

        if(just_looped) {
            // frame time diffs are not useful if we just looped
            dt_usec = 30000;
        }

        self->next_frame_time = now + dt_usec;
        dbg (DBG_INPUT, "usec until next frame: %"PRId64"\n", dt_usec);
    }

    cam_unit_control_force_set_int (self->frame_ctl, frameinfo.frameno);

    self->readone = 0;
    cam_unit_produce_frame (super, buf, cam_unit_get_output_format(super));
    g_object_unref (buf);
    return TRUE;
}

static int64_t
log_get_next_event_time (CamUnit *super)
{
    CamInputLog *self = (CamInputLog*)super;
    return self->next_frame_time;
}

static gboolean
log_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamInputLog *self = (CamInputLog*)super;
    if (ctl == self->pause_ctl) {
        g_value_copy (proposed, actual);
        int paused = g_value_get_boolean (proposed);

        if (! paused) {
            self->next_frame_time = _timestamp_now ();
        } 
        return TRUE;
    } else if (ctl == self->frame_ctl) {
        int next_frameno = g_value_get_int (proposed);
        if (! self->camlog) return FALSE;

        dbg (DBG_INPUT, "seeking to frame %d\n", next_frameno);
        if (cam_log_seek_to_frame (self->camlog, next_frameno) == 0) {
            g_value_set_int (actual, next_frameno);
            self->next_frame_time = _timestamp_now ();
            self->readone = 1;
        }
        return TRUE;
    } else if (ctl == self->adv_speed_ctl) {
        g_value_copy (proposed, actual);
        return TRUE;
    } else if (ctl == self->adv_mode_ctl) {
        g_value_copy (proposed, actual);
        return TRUE;
    } else if (ctl == self->fname_ctl) {
        if (cam_unit_is_streaming(super)) {
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
    } else if(ctl == self->loop_ctl) {
        int loop_enable = g_value_get_boolean(proposed);
        cam_unit_control_set_enabled(self->loop_start_ctl, loop_enable);
        cam_unit_control_set_enabled(self->loop_end_ctl, loop_enable);
        g_value_copy (proposed, actual);
        return TRUE;
    } else if(ctl == self->loop_start_ctl) {
        int frameno = g_value_get_int(proposed);
        if(frameno > cam_unit_control_get_int(self->loop_end_ctl)) {
            return FALSE;
        }
        g_value_copy (proposed, actual);
        return TRUE;
    } else if(ctl == self->loop_end_ctl) {
        int frameno = g_value_get_int(proposed);
        if(frameno < cam_unit_control_get_int(self->loop_start_ctl)) {
            return FALSE;
        }
        g_value_copy (proposed, actual);
        return TRUE;
    }
    return FALSE;
}
