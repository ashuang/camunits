#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>

#include "filter_jpeg.h"
#include "pixels.h"
#include "dbg.h"

#include "plugin.h"

#define err(args...) fprintf(stderr, args)

#ifndef FILTER_JPEG_LABEL
#define FILTER_JPEG_LABEL "JPEG"
#endif

static int _jpeg_decompress_to_8u_rgb (const uint8_t * src, int src_size,
        uint8_t * dest, int width, int height, int stride);
static int _jpeg_compress_8u_gray (const uint8_t * src, int width, int height, 
        int stride, uint8_t * dest, int * destsize, int quality);
static int _jpeg_compress_8u_rgb (const uint8_t * src, int width, int height, 
        int stride, uint8_t * dest, int * destsize, int quality);
static int _jpeg_compress_8u_bgra (const uint8_t * src, int width, int height, 
        int stride, uint8_t * dest, int * destsize, int quality);
static void _jpeg_std_huff_tables (j_decompress_ptr cinfo);

CamUnitDriver *
cam_filter_jpeg_driver_new()
{
    return cam_unit_driver_new_stock ("convert", "jpeg",
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
    return cam_unit_driver_new_stock_full ("convert", "jpeg",
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
        _jpeg_compress_8u_rgb (inbuf->data, width, height, infmt->row_stride,
                outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_BGRA)
        _jpeg_compress_8u_bgra (inbuf->data, width, height, infmt->row_stride,
                outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)
        _jpeg_compress_8u_gray (inbuf->data, width, height, infmt->row_stride,
                outbuf->data, &outsize, quality);
    else if (infmt->pixelformat == CAM_PIXEL_FORMAT_MJPEG) {
        _jpeg_decompress_to_8u_rgb (inbuf->data, inbuf->bytesused,
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

static int
_jpeg_decompress_to_8u_rgb (const uint8_t * src, int src_size,
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

    if (! (cinfo.dc_huff_tbl_ptrs[0] || cinfo.dc_huff_tbl_ptrs[1] ||
           cinfo.ac_huff_tbl_ptrs[0] || cinfo.ac_huff_tbl_ptrs[1])) {
        _jpeg_std_huff_tables(&cinfo);
    }

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


// ============= BEGIN code taken from libjpeg ===============

/*
 * Huffman table setup routines
 */

static void
add_huff_table (j_decompress_ptr cinfo,
		JHUFF_TBL **htblptr, const uint8_t *bits, const uint8_t *val)
/* Define a Huffman table */
{
    int nsymbols, len;

    if (*htblptr == NULL)
        *htblptr = jpeg_alloc_huff_table((j_common_ptr) cinfo);

    /* Copy the number-of-symbols-of-each-code-length counts */
    memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));

    /* Validate the counts.  We do this here mainly so we can copy the right
     * number of symbols from the val[] array, without risking marching off
     * the end of memory.  jchuff.c will do a more thorough test later.
     */
    nsymbols = 0;
    for (len = 1; len <= 16; len++)
        nsymbols += bits[len];
    if (nsymbols < 1 || nsymbols > 256)
        ERREXIT(cinfo, JERR_BAD_HUFF_TABLE);

    memcpy((*htblptr)->huffval, val, nsymbols * sizeof(uint8_t));

    /* Initialize sent_table FALSE so table will be written to JPEG file. */
    (*htblptr)->sent_table = FALSE;
}


static void
_jpeg_std_huff_tables (j_decompress_ptr cinfo)
/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
{
    static const uint8_t bits_dc_luminance[17] =
    { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t val_dc_luminance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    static const uint8_t bits_dc_chrominance[17] =
    { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    static const uint8_t val_dc_chrominance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    static const uint8_t bits_ac_luminance[17] =
    { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
    static const uint8_t val_ac_luminance[] =
    { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
        0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
        0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa };

    static const uint8_t bits_ac_chrominance[17] =
    { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
    static const uint8_t val_ac_chrominance[] =
    { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
        0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
        0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa };

    add_huff_table(cinfo, &cinfo->dc_huff_tbl_ptrs[0],
            bits_dc_luminance, val_dc_luminance);
    add_huff_table(cinfo, &cinfo->ac_huff_tbl_ptrs[0],
            bits_ac_luminance, val_ac_luminance);
    add_huff_table(cinfo, &cinfo->dc_huff_tbl_ptrs[1],
            bits_dc_chrominance, val_dc_chrominance);
    add_huff_table(cinfo, &cinfo->ac_huff_tbl_ptrs[1],
            bits_ac_chrominance, val_ac_chrominance);
}

// ============= END code taken from libjpeg =============
