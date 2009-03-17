#include <stdio.h>

#include <camunits/plugin.h>
#include <camunits/dbg.h>

#define err(args...) fprintf(stderr, args)

typedef struct _CamConvertToRgb8 {
    CamUnit parent;

    /*< private >*/
    CamUnit *worker;
    CamUnitManager *manager;
} CamConvertToRgb8;

typedef struct _CamConvertToRgb8Class {
    CamUnitClass parent_class;
} CamConvertToRgb8Class;

GType cam_convert_to_rgb8_get_type (void);

static CamConvertToRgb8 * cam_convert_to_rgb8_new(void);

CAM_PLUGIN_TYPE(CamConvertToRgb8, cam_convert_to_rgb8, 
        CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module);
void cam_plugin_initialize(GTypeModule * module)
{
    cam_convert_to_rgb8_register_type(module);
}

CamUnitDriver * cam_plugin_create(GTypeModule * module);
CamUnitDriver * cam_plugin_create(GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("convert", "to_rgb8",
            "Convert to 8-bit RGB", 0,
            (CamUnitConstructor)cam_convert_to_rgb8_new, module);
}

// ============== CamConvertToRgb8 ===============
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
cam_convert_to_rgb8_class_init( CamConvertToRgb8Class *klass )
{
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = _finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
}

static void
cam_convert_to_rgb8_init (CamConvertToRgb8 *self)
{
    cam_unit_set_preferred_format (CAM_UNIT (self), CAM_PIXEL_FORMAT_RGB, 0, 0,
            NULL);
    self->worker = NULL;
    self->manager = cam_unit_manager_get_and_ref();
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

CamConvertToRgb8 * 
cam_convert_to_rgb8_new()
{
    return (CamConvertToRgb8*)(g_object_new(cam_convert_to_rgb8_get_type(), NULL));
}

static void
_finalize (GObject * obj)
{
    CamConvertToRgb8 *self = (CamConvertToRgb8*)obj;
    if (self->worker) {
        g_signal_handlers_disconnect_by_func (self->worker, 
                on_worker_frame_ready, self);
        g_object_unref (self->worker);
    }
    g_object_unref(self->manager);

    G_OBJECT_CLASS (cam_convert_to_rgb8_parent_class)->finalize (obj);
}

static int
_stream_init (CamUnit * super, const CamUnitFormat * outfmt)
{
    CamConvertToRgb8 * self = (CamConvertToRgb8*)super;
    CamUnit *input = cam_unit_get_input(super);
    const CamUnitFormat *infmt = 
        input ? cam_unit_get_output_format(input) : NULL;
    if (input && infmt && infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        return 0;
    }
    if (self->worker) {
        CamUnitFormat *wfmt = 
            CAM_UNIT_FORMAT (g_object_get_data (G_OBJECT (outfmt), 
                "convert_to_rgb8:wfmt"));
        if (!wfmt) return -1;
        return cam_unit_stream_init (self->worker, wfmt);
    } else {
        return -1;
    }
}

static int
_stream_shutdown (CamUnit * super)
{
    CamConvertToRgb8 *self = (CamConvertToRgb8*)super;
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
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB)
        cam_unit_produce_frame (super, inbuf, infmt);
}

static void 
on_worker_frame_ready (CamUnit *worker, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt, void *user_data)
{
    CamUnit *super = CAM_UNIT (user_data);
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        cam_unit_produce_frame (super, inbuf, infmt);
        return;
    } else if (infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA) {
        int buf_sz = infmt->width*infmt->height*3;
        CamFrameBuffer *outbuf = cam_framebuffer_new_alloc (buf_sz);
        const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
        cam_pixel_convert_8u_bgra_to_8u_rgb(outbuf->data, 
                outfmt->row_stride, infmt->width, infmt->height,
                inbuf->data, infmt->row_stride);

        cam_framebuffer_copy_metadata (outbuf, inbuf);
        outbuf->bytesused = buf_sz;
        cam_unit_produce_frame (super, outbuf, outfmt);
        g_object_unref (outbuf);
        return;
    }
}

static void
_maybe_set_worker(CamConvertToRgb8 *self, const char *unit_id)
{
    if(!self->worker && 
        cam_unit_manager_find_unit_description(self->manager, unit_id)) {
        self->worker = 
            cam_unit_manager_create_unit_by_id(self->manager, unit_id);
        if(self->worker) {
            dbg(DBG_DRIVER, "using worker unit [%s]\n", unit_id);
        }
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamConvertToRgb8 *self = (CamConvertToRgb8*)super;
    gboolean was_streaming = cam_unit_is_streaming(super);
    cam_unit_stream_shutdown (super);

    if (self->worker) {
        g_signal_handlers_disconnect_by_func (self->worker, 
                on_worker_frame_ready, self);
        g_object_unref (self->worker);
        self->worker = NULL;
    }
    cam_unit_remove_all_output_formats (super);
    if (!infmt) return;
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        cam_unit_add_output_format (super, infmt->pixelformat,
                infmt->name, infmt->width, infmt->height, 
                infmt->row_stride);
    } else {
        switch (infmt->pixelformat) {
            case CAM_PIXEL_FORMAT_I420:
            case CAM_PIXEL_FORMAT_GRAY:
            case CAM_PIXEL_FORMAT_YUYV:
            case CAM_PIXEL_FORMAT_UYVY:
            case CAM_PIXEL_FORMAT_BGRA:
            case CAM_PIXEL_FORMAT_BGR:
                _maybe_set_worker(self, "convert.colorspace");
                break;
            case CAM_PIXEL_FORMAT_MJPEG:
                // use the Intel IPP library for JPEG decompression if it
                // is available
                _maybe_set_worker(self, "ipp.jpeg_decompress");

                // if not, then try the Framewave library.
                _maybe_set_worker(self, "framewave.jpeg_decompress");

                // Lastly, fall back to libjpeg
                _maybe_set_worker(self, "convert.jpeg_decompress");
                break;
            case CAM_PIXEL_FORMAT_BAYER_BGGR:
            case CAM_PIXEL_FORMAT_BAYER_RGGB:
            case CAM_PIXEL_FORMAT_BAYER_GBRG:
            case CAM_PIXEL_FORMAT_BAYER_GRBG:
                _maybe_set_worker(self, "convert.fast_debayer");
                break;
            default:
                return;
        }

        if(!self->worker) {
            return;
        }
        g_object_ref_sink (self->worker);

        g_signal_connect (G_OBJECT (self->worker), "frame-ready",
                G_CALLBACK (on_worker_frame_ready), self);
        cam_unit_set_input (self->worker, cam_unit_get_input(super));

        GList * worker_formats = cam_unit_get_output_formats (self->worker);
        for (GList *witer=worker_formats; witer; witer=witer->next) {
            CamUnitFormat *wfmt = CAM_UNIT_FORMAT (witer->data);
            if (wfmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
                CamUnitFormat *my_fmt = cam_unit_add_output_format (super,
                        wfmt->pixelformat, wfmt->name, wfmt->width, 
                        wfmt->height, wfmt->row_stride);
                g_object_set_data (G_OBJECT (my_fmt), "convert_to_rgb8:wfmt", 
                        wfmt);

            } else if (! strcmp(cam_unit_get_id(self->worker), 
                        "convert.fast_debayer") &&
                    wfmt->pixelformat == CAM_PIXEL_FORMAT_BGRA) {
                // hack.  fast debayer filter only produces BGRA, so just do an
                // internal conversion to RGB later on
                CamUnitFormat *my_fmt = cam_unit_add_output_format (super,
                        CAM_PIXEL_FORMAT_RGB, wfmt->name, wfmt->width, 
                        wfmt->height, wfmt->width*3);
                g_object_set_data (G_OBJECT (my_fmt), "convert_to_rgb8:wfmt", 
                        wfmt);
            }
        }
        g_list_free (worker_formats);
    }

    if (was_streaming) {
        cam_unit_stream_init (super, NULL);
    }
}
