#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#define MALLOC_ALIGNED(s) malloc(s)
#else
#include <malloc.h>
#define MALLOC_ALIGNED(s) memalign(16,s)
#endif

#include "filter_fast_bayer.h"
#include "dbg.h"

#define err(args...) fprintf(stderr, args)

CamUnitDriver *
cam_fast_bayer_filter_driver_new()
{
    return cam_unit_driver_new_stock( "convert:fast_debayer",
            "Fast Debayer", 0,
            (CamUnitConstructor)cam_fast_bayer_filter_new );
}

// ============== CamFastBayerFilter ===============
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static int cam_fast_bayer_filter_stream_init (CamUnit * super,
        const CamUnitFormat * fmt);
static int cam_fast_bayer_filter_stream_shutdown (CamUnit * super);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

G_DEFINE_TYPE (CamFastBayerFilter, cam_fast_bayer_filter, 
        CAM_TYPE_UNIT);

static void
cam_fast_bayer_filter_init( CamFastBayerFilter *self )
{
    dbg(DBG_FILTER, "fast_bayer filter constructor\n");
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT( self );

    const char * bayer_tiles[] = {
        "GBRG",
        "GRBG",
        "BGGR",
        "RGGB",
        NULL,
    };
    self->bayer_tile_ctl =
        cam_unit_add_control_enum( super, 1, "Tiling", 0, 1, bayer_tiles,
                NULL);

    int i;
    for (i = 0; i < 4; i++) {
        self->planes[i] = NULL;
    }

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
cam_fast_bayer_filter_class_init( CamFastBayerFilterClass *klass )
{
    dbg(DBG_FILTER, "fast_bayer filter class initializer\n");
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.stream_init =
        cam_fast_bayer_filter_stream_init;
    klass->parent_class.stream_shutdown =
        cam_fast_bayer_filter_stream_shutdown;
}

static int
cam_fast_bayer_filter_stream_init (CamUnit * super, const CamUnitFormat * fmt)
{
    if (CAM_UNIT_CLASS (cam_fast_bayer_filter_parent_class)->stream_init (super,
                fmt) < 0)
        return -1;

    CamFastBayerFilter * self = CAM_FAST_BAYER_FILTER (super);

    switch (super->input_unit->fmt->pixelformat) {
        case CAM_PIXEL_FORMAT_BAYER_GBRG:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 0);
            break;
        case CAM_PIXEL_FORMAT_BAYER_GRBG:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 1);
            break;
        case CAM_PIXEL_FORMAT_BAYER_BGGR:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 2);
            break;
        case CAM_PIXEL_FORMAT_BAYER_RGGB:
            cam_unit_control_force_set_enum (self->bayer_tile_ctl, 3);
            break;
        default:
            break;
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
    CamFastBayerFilter * self = CAM_FAST_BAYER_FILTER (super);

    int i;
    for (i = 0; i < 4; i++) {
        free (self->planes[i]);
        self->planes[i] = NULL;
    }

    /* chain up to parent, which handles some of the work */
    return CAM_UNIT_CLASS (
            cam_fast_bayer_filter_parent_class)->stream_shutdown (super);
}

CamFastBayerFilter * 
cam_fast_bayer_filter_new()
{
    return CAM_FAST_BAYER_FILTER(
            g_object_new(CAM_TYPE_FAST_BAYER_FILTER, NULL));
}

static CamPixelFormat
get_bayer_tile( CamFastBayerFilter *self )
{
    int tiling = cam_unit_control_get_enum( self->bayer_tile_ctl );
    switch( tiling ) {
        case 0:
            return CAM_PIXEL_FORMAT_BAYER_GBRG;
        case 1:
            return CAM_PIXEL_FORMAT_BAYER_GRBG;
        case 2:
            return CAM_PIXEL_FORMAT_BAYER_BGGR;
        case 3:
            return CAM_PIXEL_FORMAT_BAYER_RGGB;
        default:
            fprintf (stderr, "Warning: invalid tiling selected\n");
            return CAM_PIXEL_FORMAT_BAYER_BGGR;
    }
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt)
{
    CamFastBayerFilter * self = CAM_FAST_BAYER_FILTER (super);
    dbg(DBG_FILTER, "[%s] iterate\n", cam_unit_get_name(super));

    CamFrameBuffer *outbuf = 
        cam_framebuffer_new_alloc (super->fmt->max_data_size);

    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);

    if (outfmt->pixelformat == CAM_PIXEL_FORMAT_GRAY) {
        uint8_t * plane = self->planes[0] + 2*self->plane_stride + 16;
        int i;
        for (i = 0; i < outfmt->height; i++) {
            uint8_t * drow = plane + i*self->plane_stride;
            uint8_t * srow = inbuf->data + i*infmt->row_stride;
            memcpy (drow, srow, infmt->width);
            //memset (drow, 128, infmt->width);
        }
        cam_pixel_replicate_bayer_border_8u (plane, self->plane_stride,
                outfmt->width, outfmt->height);
        cam_pixel_bayer_interpolate_to_8u_gray (plane, self->plane_stride,
                outbuf->data, outfmt->row_stride, outfmt->width,
                outfmt->height, get_bayer_tile (self));

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
                inbuf->data, infmt->row_stride, p_width, p_height);
        int i;
        for (i = 0; i < 4; i++)
            cam_pixel_replicate_border_8u (planes[i], self->plane_stride,
                    p_width, p_height);

        cam_pixel_bayer_interpolate_to_8u_bgra (planes, self->plane_stride,
                outbuf->data, outfmt->row_stride, outfmt->width, outfmt->height,
                get_bayer_tile (self));
    }

    cam_framebuffer_copy_metadata(outbuf, inbuf);
    outbuf->bytesused = super->fmt->height * super->fmt->row_stride;

    cam_unit_produce_frame (super, outbuf, outfmt);
    g_object_unref (outbuf);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (!infmt) return;

    if (! (infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_RGGB ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_BGGR ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_GBRG ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_BAYER_GRBG ||
           infmt->pixelformat == CAM_PIXEL_FORMAT_GRAY)) return;

    CamPixelFormat outfmts[2] = {
        CAM_PIXEL_FORMAT_BGRA,
        CAM_PIXEL_FORMAT_GRAY
    };

    for (int i=0; i<2; i++) {
        CamPixelFormat out_pixelformat = outfmts[i];

        int stride = infmt->width * cam_pixel_format_bpp(out_pixelformat) / 8;

        /* Stride must be 128-byte aligned */
        stride = (stride + 0x7f)&(~0x7f);
        int max_data_size = infmt->height * stride;

        cam_unit_add_output_format_full (super, out_pixelformat,
                NULL, infmt->width, infmt->height, 
                stride, max_data_size);
    }
}
