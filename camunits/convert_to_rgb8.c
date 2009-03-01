#include <stdio.h>

#include "convert_colorspace.h"
#include "convert_jpeg_decompress.h"
#include "filter_fast_bayer.h"
#include "convert_to_rgb8.h"

#define err(args...) fprintf(stderr, args)

CamUnitDriver *
cam_convert_to_rgb8_driver_new()
{
    return cam_unit_driver_new_stock ("convert", "to_rgb8",
            "Convert to 8-bit RGB", 0,
            (CamUnitConstructor)cam_convert_to_rgb8_new);
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

G_DEFINE_TYPE (CamConvertToRgb8, cam_convert_to_rgb8, CAM_TYPE_UNIT);

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
    cam_unit_set_preferred_format (CAM_UNIT (self), CAM_PIXEL_FORMAT_RGB, 0, 0);
    self->worker = NULL;
    self->manager = cam_unit_manager_get_and_ref();
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

CamConvertToRgb8 * 
cam_convert_to_rgb8_new()
{
    return CAM_CONVERT_TO_RGB8(g_object_new(CAM_TYPE_CONVERT_TO_RGB8, NULL));
}

static void
_finalize (GObject * obj)
{
    CamConvertToRgb8 *self = CAM_CONVERT_TO_RGB8 (obj);
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
    CamConvertToRgb8 * self = CAM_CONVERT_TO_RGB8 (super);
    if (super->input_unit && super->input_unit->fmt &&
            super->input_unit->fmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
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
    CamConvertToRgb8 *self = CAM_CONVERT_TO_RGB8 (super);
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
        CamFrameBuffer *outbuf = 
            cam_framebuffer_new_alloc (infmt->width*infmt->height*3);
        cam_pixel_convert_8u_bgra_to_8u_rgb(outbuf->data, 
                super->fmt->row_stride, infmt->width, infmt->height,
                inbuf->data, infmt->row_stride);

        cam_framebuffer_copy_metadata (outbuf, inbuf);
        outbuf->bytesused = super->fmt->height * super->fmt->row_stride;
        cam_unit_produce_frame (super, outbuf, super->fmt);
        g_object_unref (outbuf);
        return;
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamConvertToRgb8 *self = CAM_CONVERT_TO_RGB8 (super);
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
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        cam_unit_add_output_format_full (super, infmt->pixelformat,
                infmt->name, infmt->width, infmt->height, 
                infmt->row_stride, infmt->max_data_size);
    } else {
        switch (infmt->pixelformat) {
            case CAM_PIXEL_FORMAT_I420:
            case CAM_PIXEL_FORMAT_GRAY:
            case CAM_PIXEL_FORMAT_YUYV:
            case CAM_PIXEL_FORMAT_BGRA:
            case CAM_PIXEL_FORMAT_BGR:
                self->worker = CAM_UNIT (cam_color_conversion_filter_new());
                break;
            case CAM_PIXEL_FORMAT_MJPEG:
                // use the Intel IPP library for JPEG decompression if it
                // is available
                if(cam_unit_manager_find_unit_description(self->manager, 
                            "ipp.jpeg_decompress")) {
                    self->worker = cam_unit_manager_create_unit_by_id(
                            self->manager, "ipp.jpeg_decompress");
                }

                // if not, then try the Framewave library.
                if(!self->worker && 
                        cam_unit_manager_find_unit_description(self->manager, 
                            "framewave.jpeg_decompress")) {
                    self->worker = cam_unit_manager_create_unit_by_id(
                            self->manager, "framewave.jpeg_decompress");
                }

                // Lastly, fall back to libjpeg
                if(!self->worker) {
                    self->worker = CAM_UNIT(cam_convert_jpeg_decompress_new());
                }
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
            if (wfmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
                CamUnitFormat *my_fmt = cam_unit_add_output_format_full (super,
                        wfmt->pixelformat, wfmt->name, wfmt->width, 
                        wfmt->height, wfmt->row_stride, wfmt->max_data_size);
                g_object_set_data (G_OBJECT (my_fmt), "convert_to_rgb8:wfmt", 
                        wfmt);

            } else if (CAM_IS_FAST_BAYER_FILTER (self->worker) &&
                    wfmt->pixelformat == CAM_PIXEL_FORMAT_BGRA) {
                // hack.  fast debayer filter only produces BGRA, so just do an
                // internal conversion to RGB later on
                CamUnitFormat *my_fmt = cam_unit_add_output_format_full (super,
                        CAM_PIXEL_FORMAT_RGB, wfmt->name, wfmt->width, 
                        wfmt->height, wfmt->width*3, wfmt->max_data_size);
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
