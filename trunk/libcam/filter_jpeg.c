#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter_jpeg.h"
#include "dbg.h"

#include "plugin.h"

#define err(args...) fprintf(stderr, args)

#ifndef FILTER_JPEG_NAME
#define FILTER_JPEG_NAME "convert:jpeg"
#endif

#ifndef FILTER_JPEG_LABEL
#define FILTER_JPEG_LABEL "JPEG"
#endif

static int jpeg_decompress_to_8u_rgb (const uint8_t * src, int src_size,
        uint8_t * dest, int width, int height, int stride);
static int jpeg_compress_8u_gray (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality);
static int jpeg_compress_8u_rgb (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality);
static int jpeg_compress_8u_bgra (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality);

CamUnitDriver *
cam_filter_jpeg_driver_new()
{
    return cam_unit_driver_new_stock (FILTER_JPEG_NAME,
            FILTER_JPEG_LABEL, 0,
            (CamUnitConstructor)cam_filter_jpeg_new);
}

// ============== CamFilterJpeg ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

#ifndef PLUGIN_UNIT
G_DEFINE_TYPE (CamFilterJpeg, cam_filter_jpeg, CAM_TYPE_UNIT);
#else
CAM_PLUGIN_TYPE (CamFilterJpeg, cam_filter_jpeg, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void
cam_plugin_initialize (GTypeModule * module)
{
    cam_filter_jpeg_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full (FILTER_JPEG_NAME,
            FILTER_JPEG_LABEL, 0,
            (CamUnitConstructor)cam_filter_jpeg_new, module);
}
#endif


static void
cam_filter_jpeg_init (CamFilterJpeg *self)
{
    dbg(DBG_FILTER, "jpeg filter constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->quality_control = cam_unit_add_control_int (super, "quality", 
            "Quality", 1, 100, 1, 94, 1);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

static void
cam_filter_jpeg_class_init (CamFilterJpegClass *klass)
{
    dbg(DBG_FILTER, "jpeg filter class initializer\n");
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

CamFilterJpeg * 
cam_filter_jpeg_new()
{
    return CAM_FILTER_JPEG(
            g_object_new(CAM_TYPE_FILTER_JPEG, NULL));
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt)
{
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);;

    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    int width = infmt->width;
    int height = infmt->height;
    int outsize = outbuf->length;
    CamFilterJpeg * self = CAM_FILTER_JPEG (super);
    int quality = cam_unit_control_get_int (self->quality_control);
    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB)
        jpeg_compress_8u_rgb (inbuf->data, width, height, infmt->row_stride,
                outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA)
        jpeg_compress_8u_bgra (inbuf->data, width, height, infmt->row_stride,
                outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        jpeg_compress_8u_gray (inbuf->data, width, height, infmt->row_stride,
                outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_MJPEG) {
        jpeg_decompress_to_8u_rgb (inbuf->data, inbuf->bytesused,
                outbuf->data, infmt->width, infmt->height, outfmt->row_stride);
        outsize = outfmt->row_stride * infmt->height;
    }

    cam_framebuffer_copy_metadata(outbuf, inbuf);
    outbuf->bytesused = outsize;

    cam_unit_produce_frame (super, outbuf, outfmt);
    g_object_unref (outbuf);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (!infmt) return;

    if (! (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_RGB ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_MJPEG)) return;

    if (infmt->pixelformat == CAM_PIXEL_FORMAT_MJPEG) {
        int stride = infmt->width * 3;
        int max_data_size = stride * infmt->height;
        cam_unit_add_output_format_full (super, CAM_PIXEL_FORMAT_RGB,
                NULL, infmt->width, infmt->height, 
                stride, max_data_size);
    } else {
        int max_data_size = infmt->width * 3 * infmt->height;
        cam_unit_add_output_format_full (super, CAM_PIXEL_FORMAT_MJPEG,
                NULL, infmt->width, infmt->height,
                0, max_data_size);
    }
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>

#include "pixels.h"

static void
init_source (j_decompress_ptr cinfo)
{
}

static boolean
fill_input_buffer (j_decompress_ptr cinfo)
{
//    fprintf (stderr, "Error: JPEG decompressor ran out of buffer space\n");
    return TRUE;
}

static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
    cinfo->src->next_input_byte += num_bytes;
    cinfo->src->bytes_in_buffer -= num_bytes;
}

static void
term_source (j_decompress_ptr cinfo)
{
}

int
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

static int jpeg_compress_8u_gray (const uint8_t * src, int width, int height, int stride,
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

static int jpeg_compress_8u_rgb (const uint8_t * src, int width, int height, int stride,
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

static int jpeg_compress_8u_bgra (const uint8_t * src, int width, int height, int stride,
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
