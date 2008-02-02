#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

#include "convert_jpeg_compress.h"
#include "pixels.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

static int _jpeg_compress_8u_gray (const uint8_t * src, int width, int height, 
        int stride, uint8_t * dest, int * destsize, int quality);
static int _jpeg_compress_8u_rgb (const uint8_t * src, int width, int height, 
        int stride, uint8_t * dest, int * destsize, int quality);
static int _jpeg_compress_8u_bgra (const uint8_t * src, int width, int height, 
        int stride, uint8_t * dest, int * destsize, int quality);

CamUnitDriver *
cam_convert_jpeg_compress_driver_new()
{
    return cam_unit_driver_new_stock ("convert", "jpeg_compress",
            "JPEG Compress", 0,
            (CamUnitConstructor)cam_convert_jpeg_compress_new);
}

// ============== CamConvertJpegCompress ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);

G_DEFINE_TYPE (CamConvertJpegCompress, cam_convert_jpeg_compress, 
        CAM_TYPE_UNIT);

static void
cam_convert_jpeg_compress_init (CamConvertJpegCompress *self)
{
    dbg(DBG_FILTER, "jpeg compress constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->quality_control = cam_unit_add_control_int (super, "quality", 
            "Quality", 1, 100, 1, 94, 1);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

static void
cam_convert_jpeg_compress_class_init (CamConvertJpegCompressClass *klass)
{
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
}

CamConvertJpegCompress * 
cam_convert_jpeg_compress_new()
{
    return CAM_CONVERT_JPEG_COMPRESS(
            g_object_new(CAM_TYPE_CONVERT_JPEG_COMPRESS, NULL));
}

static int 
_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamConvertJpegCompress *self = CAM_CONVERT_JPEG_COMPRESS (super);
    self->outbuf = cam_framebuffer_new_alloc (format->max_data_size);
    return 0;
}

static int 
_stream_shutdown (CamUnit * super)
{
    g_object_unref (CAM_CONVERT_JPEG_COMPRESS (super)->outbuf);
    CAM_CONVERT_JPEG_COMPRESS (super)->outbuf = NULL;
    return 0;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt)
{
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));
    CamConvertJpegCompress * self = CAM_CONVERT_JPEG_COMPRESS (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    int width = infmt->width;
    int height = infmt->height;
    int outsize = self->outbuf->length;
    int quality = cam_unit_control_get_int (self->quality_control);

    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB)
        _jpeg_compress_8u_rgb (inbuf->data, width, height, infmt->row_stride,
                self->outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA)
        _jpeg_compress_8u_bgra (inbuf->data, width, height, infmt->row_stride,
                self->outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        _jpeg_compress_8u_gray (inbuf->data, width, height, infmt->row_stride,
                self->outbuf->data, &outsize, quality);

    cam_framebuffer_copy_metadata(self->outbuf, inbuf);
    self->outbuf->bytesused = outsize;

    cam_unit_produce_frame (super, self->outbuf, outfmt);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (!infmt) return;

    if (! (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_RGB ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA)) return;

    int max_data_size = infmt->width * 3 * infmt->height;
    cam_unit_add_output_format_full (super, CAM_PIXEL_FORMAT_MJPEG,
            NULL, infmt->width, infmt->height,
            0, max_data_size);
}

static void
init_destination (j_compress_ptr cinfo)
{
    /* do nothing */
}

static boolean
empty_output_buffer (j_compress_ptr cinfo)
{
    fprintf (stderr, "Error: JPEG compressor ran out of buffer space\n");
    return TRUE;
}

static void
term_destination (j_compress_ptr cinfo)
{
    /* do nothing */
}

static int 
_jpeg_compress_8u_gray (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_destination_mgr jdest;
    int out_size = *destsize;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jdest.next_output_byte = dest;
    jdest.free_in_buffer = out_size;
    jdest.init_destination = init_destination;
    jdest.empty_output_buffer = empty_output_buffer;
    jdest.term_destination = term_destination;
    cinfo.dest = &jdest;

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);
    while (cinfo.next_scanline < height) {
        JSAMPROW row = (JSAMPROW)(src + cinfo.next_scanline * stride);
        jpeg_write_scanlines (&cinfo, &row, 1);
    }
    jpeg_finish_compress (&cinfo);
    *destsize = out_size - jdest.free_in_buffer;
    jpeg_destroy_compress (&cinfo);
    return 0;
}

static int 
_jpeg_compress_8u_rgb (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_destination_mgr jdest;
    int out_size = *destsize;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jdest.next_output_byte = dest;
    jdest.free_in_buffer = out_size;
    jdest.init_destination = init_destination;
    jdest.empty_output_buffer = empty_output_buffer;
    jdest.term_destination = term_destination;
    cinfo.dest = &jdest;

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);
    while (cinfo.next_scanline < height) {
        JSAMPROW row = (JSAMPROW)(src + cinfo.next_scanline * stride);
        jpeg_write_scanlines (&cinfo, &row, 1);
    }
    jpeg_finish_compress (&cinfo);
    *destsize = out_size - jdest.free_in_buffer;
    jpeg_destroy_compress (&cinfo);
    return 0;
}

static int 
_jpeg_compress_8u_bgra (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_destination_mgr jdest;
    int out_size = *destsize;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jdest.next_output_byte = dest;
    jdest.free_in_buffer = out_size;
    jdest.init_destination = init_destination;
    jdest.empty_output_buffer = empty_output_buffer;
    jdest.term_destination = term_destination;
    cinfo.dest = &jdest;

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);
    while (cinfo.next_scanline < height) {
        uint8_t buf[width*3];
        cam_pixel_convert_8u_bgra_to_8u_rgb (buf, 0, width, 1,
                src + cinfo.next_scanline * stride, 0);
        JSAMPROW row = (JSAMPROW) buf;
        jpeg_write_scanlines (&cinfo, &row, 1);
    }
    jpeg_finish_compress (&cinfo);
    *destsize = out_size - jdest.free_in_buffer;
    jpeg_destroy_compress (&cinfo);
    return 0;
}
