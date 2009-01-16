#include <stdio.h>
#include <camunits/plugin.h>

typedef struct _CamConvertToGray CamConvertToGray;
typedef struct _CamConvertToGrayClass CamConvertToGrayClass;

struct _CamConvertToGray {
    CamUnit parent;
    CamUnit *worker;
};

struct _CamConvertToGrayClass {
    CamUnitClass parent_class;
};

GType cam_convert_to_gray_get_type (void);

CamConvertToGray * cam_convert_to_gray_new(void);

// boilerplate
#define CAM_TYPE_CONVERT_TO_GRAY  cam_convert_to_gray_get_type()
#define CAM_CONVERT_TO_GRAY(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_CONVERT_TO_GRAY, CamConvertToGray))

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

CAM_PLUGIN_TYPE (CamConvertToGray, cam_convert_to_gray, CAM_TYPE_UNIT);

void
cam_plugin_initialize (GTypeModule * module)
{
    cam_convert_to_gray_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("convert", "to_gray", 
            "Convert to 8-bit Gray", 0, 
            (CamUnitConstructor)cam_convert_to_gray_new, module);
}

// ============== CamConvertToGray ===============
extern CamUnit * cam_color_conversion_filter_new(void);
extern CamUnit * cam_convert_jpeg_decompress_new (void);
extern CamUnit * cam_fast_bayer_filter_new (void);

static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);
static void _finalize (GObject * obj);

static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_worker_frame_ready (CamUnit *worker, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt, void *user_data);

static void
cam_convert_to_gray_class_init( CamConvertToGrayClass *klass )
{
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = _finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
}

static void
cam_convert_to_gray_init (CamConvertToGray *self)
{
    cam_unit_set_preferred_format (CAM_UNIT (self), CAM_PIXEL_FORMAT_RGB, 0, 0);
    self->worker = NULL;
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

CamConvertToGray * 
cam_convert_to_gray_new()
{
    return CAM_CONVERT_TO_GRAY(g_object_new(CAM_TYPE_CONVERT_TO_GRAY, NULL));
}

static void
_finalize (GObject * obj)
{
    CamConvertToGray *self = CAM_CONVERT_TO_GRAY (obj);
    if (self->worker) {
        g_signal_handlers_disconnect_by_func (self->worker, 
                on_worker_frame_ready, self);
        g_object_unref (self->worker);
    }

    G_OBJECT_CLASS (cam_convert_to_gray_parent_class)->finalize (obj);
}

static int
_stream_init (CamUnit * super, const CamUnitFormat * outfmt)
{
    CamConvertToGray * self = CAM_CONVERT_TO_GRAY (super);
    if (super->input_unit && super->input_unit->fmt &&
        super->input_unit->fmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        return 0;
    }
    if (self->worker) {
        CamUnitFormat *wfmt = 
            CAM_UNIT_FORMAT (g_object_get_data (G_OBJECT (outfmt), 
                "convert_to_gray:wfmt"));
        if (!wfmt) return -1;
        return cam_unit_stream_init (self->worker, wfmt);
    } else {
        return -1;
    }
}

static int
_stream_shutdown (CamUnit * super)
{
    CamConvertToGray *self = CAM_CONVERT_TO_GRAY (super);
    if (self->worker) {
        return cam_unit_stream_shutdown (self->worker);
    } else {
        return 0;
    }
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        cam_unit_produce_frame (super, inbuf, infmt);
}

static void 
on_worker_frame_ready (CamUnit *worker, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt, void *user_data)
{
    CamUnit *super = CAM_UNIT (user_data);
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        cam_unit_produce_frame (super, inbuf, infmt);
        return;
    } else {
        g_warning("%s has bad internal state", cam_unit_get_name(super));
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamConvertToGray *self = CAM_CONVERT_TO_GRAY (super);
    gboolean was_streaming = super->is_streaming;
    cam_unit_stream_shutdown (super);

    if (self->worker) {
        g_signal_handlers_disconnect_by_func (self->worker, 
                on_worker_frame_ready, self);
        g_object_unref (self->worker);
        self->worker = NULL;
    }
    cam_unit_remove_all_output_formats (super);
    if (!infmt) return;
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        cam_unit_add_output_format_full (super, infmt->pixelformat,
                infmt->name, infmt->width, infmt->height, 
                infmt->row_stride, infmt->max_data_size);
    } else {
        switch (infmt->pixelformat) {
            case CAM_PIXEL_FORMAT_I420:
            case CAM_PIXEL_FORMAT_YUYV:
            case CAM_PIXEL_FORMAT_BGRA:
            case CAM_PIXEL_FORMAT_BGR:
            case CAM_PIXEL_FORMAT_RGB:
                self->worker = CAM_UNIT (cam_color_conversion_filter_new());
                break;
            case CAM_PIXEL_FORMAT_MJPEG:
                self->worker = CAM_UNIT (cam_convert_jpeg_decompress_new ());
                break;
            case CAM_PIXEL_FORMAT_BAYER_BGGR:
            case CAM_PIXEL_FORMAT_BAYER_RGGB:
            case CAM_PIXEL_FORMAT_BAYER_GBRG:
            case CAM_PIXEL_FORMAT_BAYER_GRBG:
                self->worker = CAM_UNIT (cam_fast_bayer_filter_new ());
                break;
            default:
                return;
        }

        g_object_ref_sink (self->worker);

        g_signal_connect (G_OBJECT (self->worker), "frame-ready",
                G_CALLBACK (on_worker_frame_ready), self);
        cam_unit_set_input (self->worker, super->input_unit);

        GList * worker_formats = cam_unit_get_output_formats (self->worker);
        for (GList *witer=worker_formats; witer; witer=witer->next) {
            CamUnitFormat *wfmt = CAM_UNIT_FORMAT (witer->data);
            if (wfmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
                CamUnitFormat *my_fmt = cam_unit_add_output_format_full (super,
                        wfmt->pixelformat, wfmt->name, wfmt->width, 
                        wfmt->height, wfmt->row_stride, wfmt->max_data_size);
                g_object_set_data (G_OBJECT (my_fmt), "convert_to_gray:wfmt", 
                        wfmt);
            }
        }
        g_list_free (worker_formats);
    }

    if (was_streaming) {
        cam_unit_stream_init (super, NULL);
    }
}
