#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "jpeglib.h"
#include "jerror.h"

#include <camunits/plugin.h>

#define err(args...) fprintf(stderr, args)

typedef struct {
    CamUnit parent;
    
    /*< private >*/
    CamUnitControl * quality_control;
    CamFrameBuffer * outbuf;
} CamippJpegCompress;

typedef struct {
    CamUnitClass parent_class;
} CamippJpegCompressClass;

/** 
 * Constructor.
 * 
 * Don't call this function manually.  Instead, use the unit driver
 */
CamippJpegCompress * camipp_jpeg_compress_new (void);
static int _jpegipp_compress_8u_gray (const uint8_t * src, int width, 
        int height, int stride, uint8_t * dest, int * destsize, int quality);
static int _jpegipp_compress_8u_rgb (const uint8_t * src, int width, 
        int height, int stride, uint8_t * dest, int * destsize, int quality);
static int _jpegipp_compress_8u_bgra (const uint8_t * src, int width, 
        int height, int stride, uint8_t * dest, int * destsize, int quality);

// ============== CamippJpegCompress ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);

GType camipp_jpeg_compress_get_type (void);
CAM_PLUGIN_TYPE (CamippJpegCompress, camipp_jpeg_compress, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camipp_jpeg_compress_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("ipp", "jpeg_compress",
            "JPEG Compress", 0, 
            (CamUnitConstructor)camipp_jpeg_compress_new, module);
}

static void
camipp_jpeg_compress_init (CamippJpegCompress *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->quality_control = cam_unit_add_control_int (super, "quality", 
            "Quality", 1, 100, 1, 94, 1);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);
}

static void
camipp_jpeg_compress_class_init (CamippJpegCompressClass *klass)
{
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
}

CamippJpegCompress * 
camipp_jpeg_compress_new()
{
    return (CamippJpegCompress*)
            g_object_new(camipp_jpeg_compress_get_type(), NULL);
}

static int 
_stream_init (CamUnit * super, const CamUnitFormat * fmt)
{
    CamippJpegCompress *self = (CamippJpegCompress*) (super);
    self->outbuf = cam_framebuffer_new_alloc(fmt->width * fmt->height * 4);
    return 0;
}

static int 
_stream_shutdown (CamUnit * super)
{
CamippJpegCompress* self = (CamippJpegCompress*) super;
    g_object_unref (self->outbuf);
    self->outbuf = NULL;
    return 0;
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt)
{
    CamippJpegCompress * self = (CamippJpegCompress*) (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    int width = infmt->width;
    int height = infmt->height;
    int outsize = self->outbuf->length;
    int quality = cam_unit_control_get_int (self->quality_control);

    if (infmt->pixelformat == CAM_PIXEL_FORMAT_RGB)
        _jpegipp_compress_8u_rgb (inbuf->data, width, height, 
                infmt->row_stride, self->outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA)
        _jpegipp_compress_8u_bgra (inbuf->data, width, height, 
                infmt->row_stride, self->outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        _jpegipp_compress_8u_gray (inbuf->data, width, height, 
                infmt->row_stride, self->outbuf->data, &outsize, quality);

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

    cam_unit_add_output_format (super, CAM_PIXEL_FORMAT_MJPEG,
            NULL, infmt->width, infmt->height, 0);
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
_jpegipp_compress_8u_gray (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality)
{
    struct jpegipp_compress_struct cinfo;
    struct jpegipp_error_mgr jerr;
    struct jpegipp_destination_mgr jdest;
    int out_size = *destsize;

    cinfo.err = jpegipp_std_error (&jerr);
    jpegipp_create_compress (&cinfo);
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
    jpegipp_set_defaults (&cinfo);
    jpegipp_set_quality (&cinfo, quality, TRUE);

    jpegipp_start_compress (&cinfo, TRUE);
    while (cinfo.next_scanline < height) {
        JSAMPROW row = (JSAMPROW)(src + cinfo.next_scanline * stride);
        jpegipp_write_scanlines (&cinfo, &row, 1);
    }
    jpegipp_finish_compress (&cinfo);
    *destsize = out_size - jdest.free_in_buffer;
    jpegipp_destroy_compress (&cinfo);
    return 0;
}

static int 
_jpegipp_compress_8u_rgb (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality)
{
    struct jpegipp_compress_struct cinfo;
    struct jpegipp_error_mgr jerr;
    struct jpegipp_destination_mgr jdest;
    int out_size = *destsize;

    cinfo.err = jpegipp_std_error (&jerr);
    jpegipp_create_compress (&cinfo);
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
    jpegipp_set_defaults (&cinfo);
    jpegipp_set_quality (&cinfo, quality, TRUE);

    jpegipp_start_compress (&cinfo, TRUE);
    while (cinfo.next_scanline < height) {
        JSAMPROW row = (JSAMPROW)(src + cinfo.next_scanline * stride);
        jpegipp_write_scanlines (&cinfo, &row, 1);
    }
    jpegipp_finish_compress (&cinfo);
    *destsize = out_size - jdest.free_in_buffer;
    jpegipp_destroy_compress (&cinfo);
    return 0;
}

static int 
_jpegipp_compress_8u_bgra (const uint8_t * src, int width, int height, int stride,
        uint8_t * dest, int * destsize, int quality)
{
    struct jpegipp_compress_struct cinfo;
    struct jpegipp_error_mgr jerr;
    struct jpegipp_destination_mgr jdest;
    int out_size = *destsize;

    cinfo.err = jpegipp_std_error (&jerr);
    jpegipp_create_compress (&cinfo);
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
    jpegipp_set_defaults (&cinfo);
    jpegipp_set_quality (&cinfo, quality, TRUE);

    jpegipp_start_compress (&cinfo, TRUE);
    while (cinfo.next_scanline < height) {
        uint8_t buf[width*3];
        cam_pixel_convert_8u_bgra_to_8u_rgb (buf, 0, width, 1,
                src + cinfo.next_scanline * stride, 0);
        JSAMPROW row = (JSAMPROW) buf;
        jpegipp_write_scanlines (&cinfo, &row, 1);
    }
    jpegipp_finish_compress (&cinfo);
    *destsize = out_size - jdest.free_in_buffer;
    jpegipp_destroy_compress (&cinfo);
    return 0;
}
