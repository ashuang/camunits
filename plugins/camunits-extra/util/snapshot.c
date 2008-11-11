#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include <camunits/plugin.h>

#define err(args...) fprintf(stderr, args)

typedef struct _CamutilSnapshot CamutilSnapshot;
typedef struct _CamutilSnapshotClass CamutilSnapshotClass;

// boilerplate
#define CAMUTIL_TYPE_SNAPSHOT  camutil_snapshot_get_type()
#define CAMUTIL_SNAPSHOT(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAMUTIL_TYPE_SNAPSHOT, CamutilSnapshot))
#define CAMUTIL_SNAPSHOT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAMUTIL_TYPE_SNAPSHOT, CamutilSnapshotClass ))
#define IS_CAMUTIL_SNAPSHOT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAMUTIL_TYPE_SNAPSHOT ))
#define IS_CAMUTIL_SNAPSHOT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAMUTIL_TYPE_SNAPSHOT))
#define CAMUTIL_SNAPSHOT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAMUTIL_TYPE_SNAPSHOT, CamutilSnapshotClass))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);
struct _CamutilSnapshot {
    CamUnit parent;

    CamFrameBuffer *prev_buf;

//    CamUnitControl *format_ctl;
    CamUnitControl *directory_ctl;
    CamUnitControl *snap_raw_ctl;
    CamUnitControl *snap_gl_ctl;
};

struct _CamutilSnapshotClass {
    CamUnitClass parent_class;
};

GType camutil_snapshot_get_type (void);

static CamutilSnapshot * camutil_snapshot_new(void);
static int _stream_shutdown (CamUnit * super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

//static const char *FORMAT_OPTIONS[] = {
//    "Unsupported",
//    "JPEG",
//    "PPM",
//    "PGM",
//    NULL
//};

CAM_PLUGIN_TYPE (CamutilSnapshot, camutil_snapshot, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    camutil_snapshot_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("util", "snapshot", "Snapshot", 
            0, (CamUnitConstructor)camutil_snapshot_new, module);
}

static void
camutil_snapshot_class_init (CamutilSnapshotClass *klass)
{
    // class initializer.  setup the class vtable here.
    klass->parent_class.stream_shutdown = _stream_shutdown;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camutil_snapshot_init (CamutilSnapshot *self)
{
    // "private" constructor.  Initialize the unit with some reasonable
    // defaults here.
    CamUnit *super = CAM_UNIT (self);

//    int format_enabled[] = { 0, 0, 0, 0, 0 };
//    self->format_ctl = cam_unit_add_control_enum(super,
//            "format", "Format", 0, 1, FORMAT_OPTIONS, format_enabled);
    self->directory_ctl = cam_unit_add_control_string (super, "output-dir",
            "Output Directory", "/tmp/", 1);
    self->snap_raw_ctl = cam_unit_add_control_boolean (super,
            "snap-raw", "Snapshot", 0, 0);
//    self->snap_gl_ctl = cam_unit_add_control_boolean (super,
//            "snap-gl", "Snapshot (OpenGL)", 0, 1);
//    cam_unit_control_set_enabled(self->snap_gl_ctl, FALSE);

    cam_unit_control_set_ui_hints (self->snap_raw_ctl, 
            CAM_UNIT_CONTROL_ONE_SHOT);
//    cam_unit_control_set_ui_hints (self->snap_gl_ctl, 
//            CAM_UNIT_CONTROL_ONE_SHOT);

    self->prev_buf = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamutilSnapshot * 
camutil_snapshot_new()
{
    // "public" constructor
    return CAMUTIL_SNAPSHOT(g_object_new(CAMUTIL_TYPE_SNAPSHOT, NULL));
}

static int 
_stream_shutdown (CamUnit * super)
{
    CamutilSnapshot *self = CAMUTIL_SNAPSHOT (super);
    if (self->prev_buf) {
        g_object_unref (self->prev_buf);
        self->prev_buf = NULL;
    }
    cam_unit_control_set_enabled(self->snap_raw_ctl, FALSE);
    return 0;
}

static inline int _ppm_write (FILE *fp, const uint8_t *pixels,
        int width, int height, 
        int rowstride)
{
    fprintf(fp, "P6 %d %d %d\n", width, height, 255);
    int i, count;
    for (i=0; i<height; i++){
        count = fwrite(pixels + i*rowstride, width*3, 1, fp);
        if (1 != count) return -1;
    }
    return 0;
}

static inline int _pgm_write (FILE *fp, const uint8_t *pixels,
        int width, int height, 
        int rowstride)
{
    fprintf(fp, "P5\n%d\n%d\n%d\n", width, height, 255);
    int i, count;
    for (i=0; i<height; i++){
        count = fwrite(pixels + i*rowstride, width, 1, fp);
        if (1 != count) return -1;
    }
    return 0;
}

static void
_take_snapshot (CamutilSnapshot * self)
{
    CamUnit *super = CAM_UNIT(self);

    if(!self->prev_buf) {
        return;
    }

    const char *prefix = cam_unit_control_get_string (self->directory_ctl);
    char fname[PATH_MAX];
    memset(fname, 0, sizeof(fname));

    const char *suffix = "";
    
    switch (super->fmt->pixelformat) {
        case CAM_PIXEL_FORMAT_GRAY:
        case CAM_PIXEL_FORMAT_BAYER_GBRG:
        case CAM_PIXEL_FORMAT_BAYER_RGGB:
        case CAM_PIXEL_FORMAT_BAYER_BGGR:
        case CAM_PIXEL_FORMAT_BAYER_GRBG:
            suffix = "pgm";
            break;
        case CAM_PIXEL_FORMAT_RGB:
            suffix = "ppm";
            break;
        case CAM_PIXEL_FORMAT_MJPEG:
            suffix = "jpg";
            break;
        default:
            break;
    }

    int max_tries = 1000000;
    int i;
    for (i=0; i<max_tries; i++) {
        g_snprintf(fname, sizeof(fname), "%s/camunits-snapshot-%d.%s", 
                prefix, i, suffix);
        if (! g_file_test(fname, G_FILE_TEST_EXISTS))
            break;
    }
    if(max_tries == i) {
        fprintf(stderr, "%s:%d  Couldn't generate suitable filename\n", 
                __FILE__, __LINE__);
        return;
    }

    FILE *fp = fopen(fname, "wb");
    if (!fp) {
        perror("fopen");
        return;
    }
    if(!strcmp(suffix, "pgm")) {
        _pgm_write(fp, self->prev_buf->data, 
                super->fmt->width, super->fmt->height,
                super->fmt->row_stride);
    } else if (!strcmp(suffix, "ppm")) {
        _ppm_write(fp, self->prev_buf->data, 
                super->fmt->width, super->fmt->height,
                super->fmt->row_stride);
    } else if (!strcmp(suffix, "jpg")) {
        fwrite(self->prev_buf->data, self->prev_buf->bytesused, 1, fp);
    }
    fclose(fp);
    fprintf(stderr, "%s:%d  wrote to %s\n", __FILE__, __LINE__, fname);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamutilSnapshot * self = CAMUTIL_SNAPSHOT(super);
    cam_unit_remove_all_output_formats (super);
    if (! infmt) return;

#if 0
    int format_enabled[5] = {
        0, 0, 0, 0, 0
    };
    int format_selected = 0;

    switch(infmt->pixelformat) {
        case CAM_PIXEL_FORMAT_GRAY:
        case CAM_PIXEL_FORMAT_BAYER_GBRG:
        case CAM_PIXEL_FORMAT_BAYER_RGGB:
        case CAM_PIXEL_FORMAT_BAYER_BGGR:
        case CAM_PIXEL_FORMAT_BAYER_GRBG:
            format_enabled[3] = 1;
            format_selected = 3;
            break;
        case CAM_PIXEL_FORMAT_RGB:
        case CAM_PIXEL_FORMAT_BGR:
        case CAM_PIXEL_FORMAT_RGBA:
        case CAM_PIXEL_FORMAT_BGRA:
            format_enabled[2] = 1;
            format_selected = 2;
            break;
        case CAM_PIXEL_FORMAT_MJPEG:
            format_enabled[1] = 1;
            format_selected = 1;
            break;
        default:
            break;
    }

    cam_unit_control_modify_enum (self->format_ctl,
        1, FORMAT_OPTIONS, format_enabled);
    cam_unit_control_force_set_enum(self->format_ctl, format_selected);

#else
    int supported = 0;
    switch(infmt->pixelformat) {
        case CAM_PIXEL_FORMAT_GRAY:
        case CAM_PIXEL_FORMAT_BAYER_GBRG:
        case CAM_PIXEL_FORMAT_BAYER_RGGB:
        case CAM_PIXEL_FORMAT_BAYER_BGGR:
        case CAM_PIXEL_FORMAT_BAYER_GRBG:
        case CAM_PIXEL_FORMAT_RGB:
        case CAM_PIXEL_FORMAT_BGR:
        case CAM_PIXEL_FORMAT_RGBA:
        case CAM_PIXEL_FORMAT_BGRA:
        case CAM_PIXEL_FORMAT_MJPEG:
            supported = 1;
            break;
        default:
            supported = 0;
            break;
    }
    cam_unit_control_set_enabled (self->snap_raw_ctl, supported);
//    cam_unit_control_set_enabled (self->snap_gl_ctl, supported);
    if (!supported) {
        return;
    }
#endif

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);
}

static void
_copy_framebuffer (CamutilSnapshot *self, const CamFrameBuffer *inbuf)
{
    if (self->prev_buf)
        g_object_unref (self->prev_buf);
    self->prev_buf = cam_framebuffer_new_alloc (inbuf->bytesused);
    memcpy (self->prev_buf->data, inbuf->data, inbuf->bytesused);
    cam_framebuffer_copy_metadata (self->prev_buf, inbuf);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamutilSnapshot *self = CAMUTIL_SNAPSHOT(super);
    _copy_framebuffer (self, inbuf);
    cam_unit_produce_frame (super, inbuf, infmt);
    if (!cam_unit_control_get_enabled(self->snap_raw_ctl)) {
        cam_unit_control_set_enabled(self->snap_raw_ctl, TRUE);
    }
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamutilSnapshot *self = CAMUTIL_SNAPSHOT (super);
    if (ctl == self->directory_ctl) {
        // nothing to do...
    } else if (ctl == self->snap_raw_ctl) {
        _take_snapshot(self);
    } else if (ctl == self->snap_gl_ctl) {
        // TODO
    }
    g_value_copy (proposed, actual);
    return TRUE;
}
