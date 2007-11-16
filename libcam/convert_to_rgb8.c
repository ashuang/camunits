#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>
#include <jerror.h>

#include "convert_to_rgb8.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

CamUnitDriver *
cam_convert_to_rgb8_driver_new()
{
    return cam_unit_driver_new_stock ("convert:to_rgb8",
            "Convert to 8-bit RGB", 0,
            (CamUnitConstructor)cam_convert_to_rgb8_new);
}

// ============== CamConvertToRgb8 ===============
static int cam_convert_to_rgb8_stream_init (CamUnit * super, 
        const CamUnitFormat * format);
static void cam_convert_to_rgb8_finalize (GObject * obj);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);

typedef int (*cc_func_t)(CamConvertToRgb8 *self, 
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf,
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf);

#define DECL_STANDARD_CONV(name, conversion_func) \
    static inline int name (CamConvertToRgb8 *self, \
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf, \
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf) \
    { \
        return conversion_func (outbuf->data, outfmt->row_stride, \
            outfmt->width, outfmt->height, inbuf->data, infmt->row_stride); \
    }

DECL_STANDARD_CONV (gray_to_rgb, cam_pixel_convert_8u_gray_to_8u_RGB)
DECL_STANDARD_CONV (yuv420p_to_rgb, cam_pixel_convert_8u_yuv420p_to_8u_rgb)
DECL_STANDARD_CONV (yuv422_to_rgb, cam_pixel_convert_8u_yuv422_to_8u_rgb)
DECL_STANDARD_CONV (bgra_to_rgb, cam_pixel_convert_8u_bgra_to_8u_rgb)
#undef DECL_STANDARD_CONV

static int mjpeg_to_rgb (CamConvertToRgb8 *self, 
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf,
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf);

G_DEFINE_TYPE (CamConvertToRgb8, cam_convert_to_rgb8, 
        CAM_TYPE_UNIT);

typedef struct _conv_info_t {
    CamPixelFormat inpfmt;
    cc_func_t func;
} conv_info_t;

static void
add_conv (CamConvertToRgb8 *self,
        CamPixelFormat inpfmt, cc_func_t func)
{
    conv_info_t *ci = (conv_info_t*)malloc (sizeof(conv_info_t));
    ci->inpfmt = inpfmt;
    ci->func = func;
    self->conversions = g_list_append (self->conversions, ci);
}

static void
cam_convert_to_rgb8_init( CamConvertToRgb8 *self )
{
    dbg(DBG_FILTER, "color_conv filter constructor\n");
    add_conv (self, CAM_PIXEL_FORMAT_GRAY, gray_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_I420, yuv420p_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_YUYV, yuv422_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_BGRA, bgra_to_rgb);
    add_conv (self, CAM_PIXEL_FORMAT_MJPEG, mjpeg_to_rgb);

    self->cc_func = NULL;

    g_signal_connect( G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL );
}

static void
cam_convert_to_rgb8_class_init( CamConvertToRgb8Class *klass )
{
    dbg(DBG_FILTER, "color_conversion filter class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_convert_to_rgb8_finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = 
        cam_convert_to_rgb8_stream_init;
}

CamConvertToRgb8 * 
cam_convert_to_rgb8_new()
{
    return CAM_CONVERT_TO_RGB8(
            g_object_new(CAM_TYPE_CONVERT_TO_RGB8, NULL));
}

static void
cam_convert_to_rgb8_finalize (GObject * obj)
{
    dbg (DBG_INPUT, "color conversion finalize\n");
    CamConvertToRgb8 *self = CAM_CONVERT_TO_RGB8 (obj);
    for (GList *citer=self->conversions; citer; citer=citer->next) {
        free (citer->data);
    }
    g_list_free (self->conversions);
    self->conversions = NULL;
    self->cc_func = 0;

    G_OBJECT_CLASS (cam_convert_to_rgb8_parent_class)->finalize (obj);
}

static int
cam_convert_to_rgb8_stream_init (CamUnit * super, 
        const CamUnitFormat * outfmt)
{
    CamConvertToRgb8 * self = CAM_CONVERT_TO_RGB8 (super);
    dbg (DBG_INPUT, "Initializing color conversion filter\n");

    /* chain up to parent, which handles most of the error checking */
    if (CAM_UNIT_CLASS (
                cam_convert_to_rgb8_parent_class)->stream_init (super,
                outfmt) < 0)
        return -1;

    const CamUnitFormat *infmt = cam_unit_get_output_format(super->input_unit);
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        self->cc_func = NULL;
        return 0;
    }

    for (GList *citer=self->conversions; citer; citer=citer->next) {
        conv_info_t *ci = (conv_info_t*) citer->data;
        if (ci->inpfmt  == infmt->pixelformat) {
            self->cc_func = ci->func;
            return 0;
        }
    }
    cam_unit_set_status (super, CAM_UNIT_STATUS_IDLE);
    return -1;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamConvertToRgb8 * self = CAM_CONVERT_TO_RGB8 (super);
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));

    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        cam_unit_produce_frame (super, inbuf, infmt);
    }

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
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamConvertToRgb8 *self = CAM_CONVERT_TO_RGB8 (super);
    cam_unit_remove_all_output_formats (super);
    if (!infmt) return;

    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB) {
        cam_unit_add_output_format_full (super, infmt->pixelformat,
                infmt->name, infmt->width, infmt->height, infmt->row_stride,
                infmt->max_data_size);
        return;
    }
    for (GList *citer=self->conversions; citer; citer=citer->next) {
        conv_info_t *ci = (conv_info_t*) citer->data;

        if (ci->inpfmt == infmt->pixelformat) {
            int stride = infmt->width * 3;
            int max_data_size = stride * infmt->height;

            cam_unit_add_output_format_full (super, CAM_PIXEL_FORMAT_RGB,
                    NULL, infmt->width, infmt->height, 
                    stride, max_data_size);
            return;
        }
    }
}

// conversions

static void init_source (j_decompress_ptr cinfo) { }

static boolean fill_input_buffer (j_decompress_ptr cinfo) {
//    fprintf (stderr, "Error: JPEG decompressor ran out of buffer space\n");
    return TRUE;
}

static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
    cinfo->src->next_input_byte += num_bytes;
    cinfo->src->bytes_in_buffer -= num_bytes;
}

static void term_source (j_decompress_ptr cinfo) { }

static int
jpeg_decompress_to_8u_rgb (const uint8_t * src, int src_size,
        uint8_t * dest, int width, int height, int stride)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_source_mgr jsrc;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);

    jsrc.next_input_byte = src;
    jsrc.bytes_in_buffer = src_size;
    jsrc.init_source = init_source;
    jsrc.fill_input_buffer = fill_input_buffer;
    jsrc.skip_input_data = skip_input_data;
    jsrc.resync_to_restart = jpeg_resync_to_restart;
    jsrc.term_source = term_source;
    cinfo.src = &jsrc;

    jpeg_read_header (&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress (&cinfo);

    if (cinfo.output_height != height || cinfo.output_width != width) {
        fprintf (stderr, "Error: Buffer was %dx%d but JPEG image is %dx%d\n",
                width, height, cinfo.output_width, cinfo.output_height);
        jpeg_destroy_decompress (&cinfo);
        return -1;
    }

    while (cinfo.output_scanline < height) {
        uint8_t * row = dest + cinfo.output_scanline * stride;
        jpeg_read_scanlines (&cinfo, &row, 1);
    }
    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);
    return 0;
}

static int 
mjpeg_to_rgb (CamConvertToRgb8 *self, 
        const CamUnitFormat *infmt, const CamFrameBuffer *inbuf,
        const CamUnitFormat *outfmt, CamFrameBuffer *outbuf)
{
    return jpeg_decompress_to_8u_rgb (inbuf->data, inbuf->bytesused,
            outbuf->data, outfmt->width, outfmt->height, outfmt->row_stride);
}
