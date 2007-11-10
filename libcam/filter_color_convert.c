#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter_color_convert.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

CamUnitDriver *
cam_color_conversion_filter_driver_new()
{
    return cam_unit_driver_new_stock( "convert:colorspace",
            "Colorspace Conversion", 0,
            (CamUnitConstructor)cam_color_conversion_filter_new );
}

// ============== CamColorConversionFilter ===============
static int cam_color_conversion_filter_stream_init (CamUnit * super, 
        const CamUnitFormat * format);
static void cam_color_conversion_filter_finalize (GObject * obj);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);

typedef int (*cc_func_t)(CamColorConversionFilter *self, 
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf,
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf);

#define DECL_STANDARD_CONV(name, conversion_func) \
    static inline int name (CamColorConversionFilter *self, \
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf, \
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf) \
    { \
        return conversion_func (outbuf->data, outfmt->row_stride, \
            outfmt->width, outfmt->height, inbuf->data, infmt->row_stride); \
    }

DECL_STANDARD_CONV (gray_to_rgb, cam_pixel_convert_8u_gray_to_8u_RGB)
DECL_STANDARD_CONV (gray_to_rgba, cam_pixel_convert_8u_gray_to_8u_RGBA)
DECL_STANDARD_CONV (rgb_to_gray, cam_pixel_convert_8u_rgb_to_8u_gray)
DECL_STANDARD_CONV (rgb_to_bgr, cam_pixel_convert_8u_rgb_to_8u_bgr)
DECL_STANDARD_CONV (rgb_to_bgra, cam_pixel_convert_8u_rgb_to_8u_bgra)

DECL_STANDARD_CONV (yuv420p_to_rgb, cam_pixel_convert_8u_yuv420p_to_8u_rgb)
DECL_STANDARD_CONV (yuv420p_to_rgba, cam_pixel_convert_8u_yuv420p_to_8u_rgba)
DECL_STANDARD_CONV (yuv420p_to_bgr, cam_pixel_convert_8u_yuv420p_to_8u_bgr)
DECL_STANDARD_CONV (yuv420p_to_bgra, cam_pixel_convert_8u_yuv420p_to_8u_bgra)
DECL_STANDARD_CONV (yuv420p_to_gray, cam_pixel_convert_8u_yuv420p_to_8u_bgra)

DECL_STANDARD_CONV (yuv422_to_bgra, cam_pixel_convert_8u_yuv422_to_8u_bgra)
DECL_STANDARD_CONV (yuv422_to_gray, cam_pixel_convert_8u_yuv422_to_8u_gray)
DECL_STANDARD_CONV (bgra_to_rgb, cam_pixel_convert_8u_bgra_to_8u_rgb)
DECL_STANDARD_CONV (bgra_to_bgr, cam_pixel_convert_8u_bgra_to_8u_bgr)
#undef DECL_STANDARD_CONV

G_DEFINE_TYPE (CamColorConversionFilter, cam_color_conversion_filter, 
        CAM_TYPE_UNIT);

typedef struct _conv_info_t {
    CamPixelFormat inpfmt;
    CamPixelFormat outpfmt;
    cc_func_t func;
} conv_info_t;

static void
add_conv (CamColorConversionFilter *self,
        CamPixelFormat inpfmt, CamPixelFormat outpfmt, cc_func_t func)
{
    conv_info_t *ci = (conv_info_t*)malloc (sizeof(conv_info_t));
    ci->inpfmt = inpfmt;
    ci->outpfmt = outpfmt;
    ci->func = func;
    self->conversions = g_list_append (self->conversions, ci);
}

static void
cam_color_conversion_filter_init( CamColorConversionFilter *self )
{
    dbg(DBG_FILTER, "color_conv filter constructor\n");
    add_conv (self, CAM_PIXEL_FORMAT_GRAY, CAM_PIXEL_FORMAT_RGB,  gray_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_RGB,  CAM_PIXEL_FORMAT_GRAY, rgb_to_gray);
    add_conv (self, CAM_PIXEL_FORMAT_RGB,  CAM_PIXEL_FORMAT_BGRA, rgb_to_bgra);
    add_conv (self, CAM_PIXEL_FORMAT_RGB,  CAM_PIXEL_FORMAT_BGR, rgb_to_bgr);

    add_conv (self, CAM_PIXEL_FORMAT_I420, CAM_PIXEL_FORMAT_RGB,  yuv420p_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_I420, CAM_PIXEL_FORMAT_RGBA, yuv420p_to_rgba);
    add_conv (self, CAM_PIXEL_FORMAT_I420, CAM_PIXEL_FORMAT_BGR,  yuv420p_to_bgr);
    add_conv (self, CAM_PIXEL_FORMAT_I420, CAM_PIXEL_FORMAT_BGRA, yuv420p_to_bgra);
    add_conv (self, CAM_PIXEL_FORMAT_I420, CAM_PIXEL_FORMAT_GRAY, yuv420p_to_gray);
    add_conv (self, CAM_PIXEL_FORMAT_YV12, CAM_PIXEL_FORMAT_GRAY, yuv420p_to_gray);

    add_conv (self, CAM_PIXEL_FORMAT_YUYV, CAM_PIXEL_FORMAT_BGRA, yuv422_to_bgra);
    add_conv (self, CAM_PIXEL_FORMAT_YUYV, CAM_PIXEL_FORMAT_GRAY, yuv422_to_gray);

    add_conv (self, CAM_PIXEL_FORMAT_BGRA, CAM_PIXEL_FORMAT_RGB, bgra_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_BGRA, CAM_PIXEL_FORMAT_BGR, bgra_to_bgr);

    self->cc_func = NULL;

    self->output_pixelformat_override = CAM_PIXEL_FORMAT_INVALID;

    g_signal_connect( G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL );
}

static void
cam_color_conversion_filter_class_init( CamColorConversionFilterClass *klass )
{
    dbg(DBG_FILTER, "color_conversion filter class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_color_conversion_filter_finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = 
        cam_color_conversion_filter_stream_init;
}

CamColorConversionFilter * 
cam_color_conversion_filter_new()
{
    return CAM_COLOR_CONVERSION_FILTER(
            g_object_new(CAM_TYPE_COLOR_CONVERSION_FILTER, NULL));
}

static void
cam_color_conversion_filter_finalize (GObject * obj)
{
    dbg (DBG_INPUT, "color conversion finalize\n");
    CamColorConversionFilter *self = CAM_COLOR_CONVERSION_FILTER (obj);
    for (GList *citer=self->conversions; citer; citer=citer->next) {
        free (citer->data);
    }
    g_list_free (self->conversions);
    self->conversions = NULL;
    self->cc_func = 0;

    G_OBJECT_CLASS (cam_color_conversion_filter_parent_class)->finalize (obj);
}

static int
cam_color_conversion_filter_stream_init (CamUnit * super, 
        const CamUnitFormat * outfmt)
{
    CamColorConversionFilter * self = CAM_COLOR_CONVERSION_FILTER (super);
    dbg (DBG_INPUT, "Initializing color conversion filter\n");

    /* chain up to parent, which handles most of the error checking */
    if (CAM_UNIT_CLASS (
                cam_color_conversion_filter_parent_class)->stream_init (super,
                outfmt) < 0)
        return -1;

    const CamUnitFormat *infmt = cam_unit_get_output_format(super->input_unit);
    for (GList *citer=self->conversions; citer; citer=citer->next) {
        conv_info_t *ci = (conv_info_t*) citer->data;
        if (ci->inpfmt  == infmt->pixelformat &&
            ci->outpfmt == outfmt->pixelformat) {
            self->cc_func = ci->func;
            return 0;
        }
    }
    dbg (DBG_INPUT, 
            "ColorConversion couldn't find appropriate conversion function\n");
    cam_unit_set_status (super, CAM_UNIT_STATUS_IDLE);
    return -1;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamColorConversionFilter * self = CAM_COLOR_CONVERSION_FILTER (super);
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));

    if (!self->cc_func) return;

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);

    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    int status = self->cc_func (self, infmt, inbuf, outfmt, outbuf);

    if (0 == status) {
        cam_framebuffer_copy_metadata(outbuf, inbuf);
        outbuf->bytesused = super->fmt->height * super->fmt->row_stride;
        cam_unit_produce_frame (super, outbuf, outfmt);
    }

    g_object_unref (outbuf);
}

static void
update_output_formats (CamColorConversionFilter *self, 
        const CamUnitFormat *infmt)
{
    CamUnit *super = CAM_UNIT (self);
    cam_unit_remove_all_output_formats (super);

    if (!infmt) return;

    for (GList *citer=self->conversions; citer; citer=citer->next) {
        conv_info_t *ci = (conv_info_t*) citer->data;

        if (ci->inpfmt == infmt->pixelformat) {
           if ((self->output_pixelformat_override != CAM_PIXEL_FORMAT_INVALID) &&
                   (self->output_pixelformat_override != ci->outpfmt)) {
               dbg (DBG_FILTER, "color converter ignoring %s -> %s\n",
                       cam_pixel_format_str (ci->inpfmt), 
                       cam_pixel_format_str (ci->outpfmt));
           } else {
               int stride = 
                   infmt->width * cam_pixel_format_bpp(ci->outpfmt) / 8;
               int max_data_size = stride * infmt->height;

               cam_unit_add_output_format_full (super, ci->outpfmt,
                       NULL, infmt->width, infmt->height, 
                       stride, max_data_size);
           }
        }
    }
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    update_output_formats (CAM_COLOR_CONVERSION_FILTER (super), infmt);
}

void
cam_color_conversion_filter_set_output_format (CamColorConversionFilter *self,
        CamPixelFormat pixelformat)
{
    CamUnit *super = CAM_UNIT (self);
    self->output_pixelformat_override = pixelformat;

    CamUnitStatus original_status = cam_unit_get_status (super);
    switch (original_status) {
        case CAM_UNIT_STATUS_READY:
            cam_unit_stream_shutdown (super);
            break;
        case CAM_UNIT_STATUS_STREAMING:
            cam_unit_stream_off (super);
            cam_unit_stream_shutdown (super);
            break;
        default:
            break;
    }

    if (super->input_unit) {
        update_output_formats (self, 
                cam_unit_get_output_format (super->input_unit));
    }

    switch (original_status) {
        case CAM_UNIT_STATUS_READY:
            cam_unit_stream_init_any_format (super);
            break;
        case CAM_UNIT_STATUS_STREAMING:
            cam_unit_stream_init_any_format (super);
            cam_unit_stream_on (super);
            break;
        default:
            break;
    }
}
