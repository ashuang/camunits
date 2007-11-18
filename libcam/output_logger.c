#define _GNU_SOURCE // so that basename () is the GNU version, and not the posix
                    // version

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>

#include "output_logger.h"
#include "dbg.h"

#define err(args...) fprintf (stderr, args)

#define MAX_UNWRITTEN_FRAMES 100

enum {
    RECORD_CONTROL,
    FILENAME_CONTROL
};

CamUnitDriver *
cam_logger_unit_driver_new (void)
{
    return cam_unit_driver_new_stock ("output:logger",
            "Logger", 0,
            (CamUnitConstructor)cam_logger_unit_new);
}

static int WRITER_THREAD_QUIT_REQUEST = 0;

// ============== CamLoggerUnit ===============
static void log_finalize (GObject *obj);
static gboolean try_set_control (CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);
static int load_camlog (CamLoggerUnit *self, const char *fname);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void * writer_thread (void *user_data);

G_DEFINE_TYPE (CamLoggerUnit, cam_logger_unit, CAM_TYPE_UNIT);

static void
cam_logger_unit_init (CamLoggerUnit *self)
{
    dbg (DBG_FILTER, "logging filter constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->camlog = NULL;
    self->fname = NULL;

    self->filename_ctl = 
        cam_unit_add_control_string (super, "filename", "Filename",
                "", 1);
    self->record_ctl = 
        cam_unit_add_control_boolean (super, "record", "Record", 0, 1); 

    cam_unit_control_set_ui_hints (self->filename_ctl, 
            CAM_UNIT_CONTROL_FILENAME);

    self->msg_q = g_async_queue_new ();
    self->writer_thread = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
cam_logger_unit_class_init (CamLoggerUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = log_finalize;
    klass->parent_class.try_set_control = try_set_control;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

static void
log_finalize (GObject *obj)
{
    dbg (DBG_FILTER, "LoggerUnit: finalize\n");
    CamLoggerUnit *self = CAM_LOGGER_UNIT (obj);
    if (self->writer_thread) {
        g_async_queue_push (self->msg_q, &WRITER_THREAD_QUIT_REQUEST);
        g_thread_join (self->writer_thread);
    }
    g_async_queue_unref (self->msg_q);
    
    if (self->camlog) { 
        dbg (DBG_FILTER, "LoggerUnit: closing camlog\n");
        cam_log_destroy (self->camlog); 
        self->camlog = NULL;
    }

    if (self->fname) {
        free (self->fname);
        self->fname = NULL;
    }
    G_OBJECT_CLASS (cam_logger_unit_parent_class)->finalize (obj);
}

CamLoggerUnit * 
cam_logger_unit_new ()
{
    return CAM_LOGGER_UNIT (g_object_new (CAM_TYPE_LOGGER_UNIT, NULL));
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);
    if (!infmt)
        return;

    // match the output format of the input unit
    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    dbg (DBG_FILTER, "[%s] iterate\n", cam_unit_get_name (super));
    CamLoggerUnit *self = CAM_LOGGER_UNIT (super);

    int recording = cam_unit_control_get_boolean (self->record_ctl);

    /* If a camlog is not already set, generate one with an
     * auto-generated filename. */
    if (recording && !self->camlog)
        load_camlog (self, NULL);

    if (recording && self->camlog) {
        if (g_async_queue_length (self->msg_q) > MAX_UNWRITTEN_FRAMES * 2) {
            fprintf (stderr, "%s:%d - disk too slow, dropping frame\n",
                    __FILE__, __LINE__);
        }
        else {
            CamUnitFormat *fmt_copy = cam_unit_format_new (infmt->pixelformat,
                    infmt->name, infmt->width, infmt->height, infmt->row_stride,
                    infmt->max_data_size);
            CamFrameBuffer *buf_copy = 
                cam_framebuffer_new_alloc (inbuf->bytesused);
            memcpy (buf_copy->data, inbuf->data, inbuf->bytesused);
            buf_copy->bytesused = inbuf->bytesused;
            cam_framebuffer_copy_metadata (buf_copy, inbuf);

            g_async_queue_push (self->msg_q, fmt_copy);
            g_async_queue_push (self->msg_q, buf_copy);
        }
    }

    cam_unit_produce_frame (super, inbuf, infmt);
}

static int
load_camlog (CamLoggerUnit *self, const char *fname)
{
    if (self->writer_thread) {
        g_async_queue_push (self->msg_q, &WRITER_THREAD_QUIT_REQUEST);
        g_thread_join (self->writer_thread);
        self->writer_thread = NULL;
        // TODO flush data still remaining in the async queue
    }

    char autoname[256];
    if (!fname) {
        time_t t = time (NULL);
        struct tm ti;
        localtime_r (&t, &ti);

        char hostname[80];
        gethostname (hostname, sizeof (hostname)-1);
        snprintf (autoname, sizeof (autoname), "%d-%02d-%02d-cam-%s",
                ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday, hostname);

        fname = autoname;
    }

    if (self->camlog) {
        dbg (DBG_FILTER, "LoggerUnit: destroying existing camlog\n");
        cam_log_destroy (self->camlog);
        self->camlog = NULL;
    }
    if (self->fname) {
        free (self->fname);
        self->fname = NULL;
    }

    /* Loop through possible file names until we find one that doesn't already
     * exist.  This way, we never overwrite an existing file. */
    char filename[256];
    int res;
    int filenum = 0;
    do {
        struct stat statbuf;
        snprintf (filename, sizeof (filename), "%s.%02d", fname, filenum);
        res = stat (filename, &statbuf);
        filenum++;
    } while (res == 0);

    if (errno != ENOENT) {
        perror ("Error: checking for existing log filenames");
        return -1;
    }

    self->fname = strdup (filename);
    char *tmpstr = strdup (filename);
    self->basename = strdup (basename (tmpstr));
    free (tmpstr);

    dbg (DBG_FILTER, "LoggerUnit: Trying to load log file [%s]\n", filename);
    self->camlog = cam_log_new (filename, "w");
    if (!self->camlog) {
        err ("LoggerUnit: unable to open new log file [%s]\n", filename);
        return -1;
    }
    printf ("Logging frames to \"%s\"\n", filename);

    self->writer_thread = g_thread_create (writer_thread, self, TRUE, NULL);

    return 0;
}

static gboolean 
try_set_control (CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual)
{
    CamLoggerUnit *self = CAM_LOGGER_UNIT (super);

    if (ctl == self->record_ctl) {
        g_value_copy (proposed, actual);
        int recording = g_value_get_boolean (proposed);
        cam_unit_control_set_enabled (self->filename_ctl, !recording);
    }
    else if (ctl == self->filename_ctl) {
        const char *fname = g_value_get_string (proposed);
        load_camlog (self, fname);
        if (self->camlog)
            g_value_copy (proposed, actual);
        else
            g_value_set_string (actual, "");
        //cam_unit_control_set_enabled (self->record_ctl, (self->camlog!=NULL));
    }

    return TRUE;
}

static void *
writer_thread (void *user_data)
{
    dbg (DBG_FILTER, "LoggerUnit: writer thread started\n");
    CamLoggerUnit *self = CAM_LOGGER_UNIT (user_data);

    while (1) {
        void *msg = g_async_queue_pop (self->msg_q);
        if (msg == &WRITER_THREAD_QUIT_REQUEST)
            break;

        CamUnitFormat *infmt = CAM_UNIT_FORMAT (msg);
        CamLogFrameFormat format = {
            .pixelformat = infmt->pixelformat,
            .width = infmt->width,
            .height = infmt->height,
            .stride = infmt->row_stride,
        };
        CamFrameBuffer *inbuf =
            CAM_FRAMEBUFFER (g_async_queue_pop (self->msg_q));

        // write the new frame to disk
        if (cam_log_write_frame (self->camlog, &format, inbuf, NULL) < 0)
            err ("LoggerUnit: Unable to write frame...\n");

        g_object_unref (infmt);
        g_object_unref (inbuf);
    }
    dbg (DBG_FILTER, "LoggerUnit: writer thread exiting\n");

    return NULL;
}
