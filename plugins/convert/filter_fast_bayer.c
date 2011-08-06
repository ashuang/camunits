#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camunits/plugin.h"
#include "camunits/dbg.h"

#ifdef __APPLE__
#define MALLOC_ALIGNED(s) malloc(s)
#else
#include <malloc.h>
#define MALLOC_ALIGNED(s) memalign(16,s)
#endif

#ifdef __APPLE__
#define MALLOC_ALIGNED(s) malloc(s)
#else
#include <malloc.h>
#define MALLOC_ALIGNED(s) memalign(16,s)
#endif

#define err(args...) fprintf(stderr, args)

enum {
    OPTION_GBRG = 0,
    OPTION_GRBG,
    OPTION_BGGR,
    OPTION_RGGB
};

static CamPixelFormat _option_to_pfmt[] = {
    CAM_PIXEL_FORMAT_BAYER_GBRG,
    CAM_PIXEL_FORMAT_BAYER_GRBG,
    CAM_PIXEL_FORMAT_BAYER_BGGR,
    CAM_PIXEL_FORMAT_BAYER_RGGB
};

typedef struct _CamFastBayerFilter {
    CamUnit parent;
    CamUnitControl *bayer_tile_ctl;

    uint8_t * aligned_buffer;

    uint8_t * planes[4];
    int plane_stride;
} CamFastBayerFilter;

typedef struct _CamFastBayerFilterClass {
    CamUnitClass parent_class;
} CamFastBayerFilterClass;

static CamFastBayerFilter * cam_fast_bayer_filter_new (void);

GType cam_fast_bayer_filter_get_type (void);
CAM_PLUGIN_TYPE(CamFastBayerFilter, cam_fast_bayer_filter, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module);
void cam_plugin_initialize(GTypeModule * module)
{
    cam_fast_bayer_filter_register_type(module);
}

CamUnitDriver * cam_plugin_create(GTypeModule * module);
CamUnitDriver * cam_plugin_create(GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ( "convert", "fast_debayer",
            "Fast Debayer", 0,
            (CamUnitConstructor)cam_fast_bayer_filter_new, module);
}

// ============== CamFastBayerFilter ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static int cam_fast_bayer_filter_stream_init (CamUnit * super,
        const CamUnitFormat * fmt);
static int cam_fast_bayer_filter_stream_shutdown (CamUnit * super);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

static int
is_bayer_pixel_format(CamPixelFormat pfmt)
{
    return (pfmt == CAM_PIXEL_FORMAT_BAYER_GBRG ||
            pfmt == CAM_PIXEL_FORMAT_BAYER_GRBG ||
            pfmt == CAM_PIXEL_FORMAT_BAYER_BGGR ||
            pfmt == CAM_PIXEL_FORMAT_BAYER_RGGB);
}

static void
cam_fast_bayer_filter_init( CamFastBayerFilter *self )
{
    dbg(DBG_FILTER, "fast_bayer filter constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT( self );

    CamUnitControlEnumValue tiling_entries[] = {
        { OPTION_GBRG, "GBRG", 1 },
        { OPTION_GRBG, "GRBG", 1 },
        { OPTION_BGGR, "BGGR", 1 },
        { OPTION_RGGB, "RGGB", 1 },
        { 0, NULL, 0 }
    };

    self->bayer_tile_ctl = cam_unit_add_control_enum (super, "tiling", 
            "Tiling", OPTION_GBRG, 1, tiling_entries);

    for (int i = 0; i < 4; i++) {
        self->planes[i] = NULL;
    }

    self->aligned_buffer = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
cam_fast_bayer_filter_class_init( CamFastBayerFilterClass *klass )
{
    dbg(DBG_FILTER, "fast_bayer filter class initializer\n");
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init = cam_fast_bayer_filter_stream_init;
    klass->parent_class.stream_shutdown = cam_fast_bayer_filter_stream_shutdown;
}

static int
cam_fast_bayer_filter_stream_init (CamUnit * super, const CamUnitFormat * fmt)
{
    CamFastBayerFilter * self = (CamFastBayerFilter*)super;
    CamUnit * input = cam_unit_get_input(super);
    const CamUnitFormat * infmt = cam_unit_get_output_format(input);

    if(is_bayer_pixel_format(infmt->pixelformat)) {
        int tiling = OPTION_GBRG;
        switch (infmt->pixelformat) {
            case CAM_PIXEL_FORMAT_BAYER_GBRG:
                tiling = OPTION_GBRG;
                break;
            case CAM_PIXEL_FORMAT_BAYER_GRBG:
                tiling = OPTION_GRBG;
                break;
            case CAM_PIXEL_FORMAT_BAYER_BGGR:
                tiling = OPTION_BGGR;
                break;
            case CAM_PIXEL_FORMAT_BAYER_RGGB:
                tiling = OPTION_RGGB;
                break;
            default:
                break;
        }
        cam_unit_control_force_set_enum(self->bayer_tile_ctl, tiling);
    } else if(infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        // nothing?
    } else {
            return -1;
    }

    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    if (outfmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        int width = outfmt->width;
        int height = outfmt->height;
        self->plane_stride = ((width + 0xf)&(~0xf)) + 32;
        self->planes[0] = MALLOC_ALIGNED (self->plane_stride * (height + 4));
    }
    else {
        int width = outfmt->width / 2;
        int height = outfmt->height / 2;

        /* ensure stride is 16-byte aligned and add 32 extra bytes for the
         * border padding */
        self->plane_stride = ((width + 0xf)&(~0xf)) + 32;
        int i;
        for (i = 0; i < 4; i++)
            self->planes[i] = MALLOC_ALIGNED (self->plane_stride *
                    (height + 2));
    }

    return 0;
}

static int
cam_fast_bayer_filter_stream_shutdown (CamUnit * super)
{
    CamFastBayerFilter * self = (CamFastBayerFilter*) super;

    int i;
    for (i = 0; i < 4; i++) {
        free (self->planes[i]);
        self->planes[i] = NULL;
    }

    free(self->aligned_buffer);
    self->aligned_buffer = NULL;

    return 0;
}

CamFastBayerFilter * 
cam_fast_bayer_filter_new()
{
    int have_sse2 = cam_pixel_check_sse2();
    if (!have_sse2){
      err("Error: The fast-debayer pluger requires at least SSE2\n");
      return NULL;
    }
    return (CamFastBayerFilter*)
            g_object_new(cam_fast_bayer_filter_get_type(), NULL);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt)
{
    CamFastBayerFilter * self = (CamFastBayerFilter*) super;
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    int out_buf_size = outfmt->height * outfmt->row_stride;
    int in_buf_size = infmt->height * infmt->row_stride;
    CamFrameBuffer *outbuf = cam_framebuffer_new_alloc (out_buf_size);

    const uint8_t *in_data = inbuf->data;

    // if the input buffer is not 16-byte aligned, then make an aligned copy.
    if(!CAM_IS_ALIGNED16(inbuf->data)) {
        if(! self->aligned_buffer) {
            self->aligned_buffer = MALLOC_ALIGNED(in_buf_size);
        }
        memcpy(self->aligned_buffer, inbuf->data, inbuf->bytesused);
        in_data = self->aligned_buffer;
    }

    int tiling_option = cam_unit_control_get_enum(self->bayer_tile_ctl);
    CamPixelFormat tiling = _option_to_pfmt[tiling_option];

    if (outfmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        uint8_t * plane = self->planes[0] + 2*self->plane_stride + 16;
        int i;
        for (i = 0; i < outfmt->height; i++) {
            uint8_t * drow = plane + i*self->plane_stride;
            const uint8_t * srow = in_data + i*infmt->row_stride;
            memcpy (drow, srow, infmt->width);
            //memset (drow, 128, infmt->width);
        }
        cam_pixel_replicate_bayer_border_8u (plane, self->plane_stride,
                outfmt->width, outfmt->height);
        cam_pixel_bayer_interpolate_to_8u_gray (plane, self->plane_stride,
                outbuf->data, outfmt->row_stride, outfmt->width,
                outfmt->height, tiling);

        //uint8_t * d = outbuf->data + 8*outfmt->row_stride;
        //printf ("%d %d\n", d[0], d[1]);
    }
    else {
        uint8_t * planes[] = {
            self->planes[0] + self->plane_stride + 16,
            self->planes[1] + self->plane_stride + 16,
            self->planes[2] + self->plane_stride + 16,
            self->planes[3] + self->plane_stride + 16,
        };

        int p_width = outfmt->width / 2;
        int p_height = outfmt->height / 2;

        cam_pixel_split_bayer_planes_8u (planes, self->plane_stride,
                in_data, infmt->row_stride, p_width, p_height);
        int i;
        for (i = 0; i < 4; i++)
            cam_pixel_replicate_border_8u (planes[i], self->plane_stride,
                    p_width, p_height);

        cam_pixel_bayer_interpolate_to_8u_bgra (planes, self->plane_stride,
                outbuf->data, outfmt->row_stride, outfmt->width, outfmt->height,
                tiling);
    }

    cam_framebuffer_copy_metadata(outbuf, inbuf);
    outbuf->bytesused = out_buf_size;

    cam_unit_produce_frame (super, outbuf, outfmt);
    g_object_unref (outbuf);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (!infmt) return;

    if (! is_bayer_pixel_format(infmt->pixelformat) &&
          infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY) 
        return;

    CamPixelFormat outfmts[2] = {
        CAM_PIXEL_FORMAT_BGRA,
        CAM_PIXEL_FORMAT_GRAY
    };

    for (int i=0; i<2; i++) {
        CamPixelFormat out_pixelformat = outfmts[i];

        int stride = infmt->width * cam_pixel_format_bpp(out_pixelformat) / 8;

        /* Stride must be 128-byte aligned */
        stride = (stride + 0x7f)&(~0x7f);

        cam_unit_add_output_format (super, out_pixelformat,
                NULL, infmt->width, infmt->height, 
                stride);
    }
}
