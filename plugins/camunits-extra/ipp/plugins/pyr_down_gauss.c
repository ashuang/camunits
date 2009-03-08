#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <camunits/plugin.h>
#include <ipp.h>
#include <ippi.h>

#define err(args...) fprintf(stderr, args)

typedef struct {
    CamUnit parent;
    uint8_t *pyrbuf;
    CamFrameBuffer *outbuf;
} CamippPyrDownGauss;

typedef struct {
    CamUnitClass parent_class;
} CamippPyrDownGaussClass;

static CamippPyrDownGauss * camipp_pyr_down_gauss_new(void);
static void _finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

GType camipp_pyr_down_gauss_get_type (void);
CAM_PLUGIN_TYPE (CamippPyrDownGauss, camipp_pyr_down_gauss, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camipp_pyr_down_gauss_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("ipp", "pyr-down-gauss",
            "Pyramid Down Gaussian", 0, 
            (CamUnitConstructor)camipp_pyr_down_gauss_new,
            module);
}

static void
camipp_pyr_down_gauss_class_init (CamippPyrDownGaussClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = _finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
}

static void
camipp_pyr_down_gauss_init (CamippPyrDownGauss *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
//    CamUnit *super = CAM_UNIT (self);
    self->pyrbuf = NULL;
    self->outbuf = NULL;
    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static void
_finalize (GObject *obj)
{
    // destructor.  release heap/freestore memory here
    CamippPyrDownGauss *self = (CamippPyrDownGauss*) (obj);
    if (self->pyrbuf) {
        free (self->pyrbuf);
    }
    if (self->outbuf) {
        g_object_unref (self->outbuf);
    }
    G_OBJECT_CLASS (camipp_pyr_down_gauss_parent_class)->finalize(obj);
}

static CamippPyrDownGauss * 
camipp_pyr_down_gauss_new()
{
    return (CamippPyrDownGauss*)(g_object_new(camipp_pyr_down_gauss_get_type(), NULL));
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamippPyrDownGauss *self = (CamippPyrDownGauss*) (super);
    cam_unit_remove_all_output_formats (super);

    if (! infmt || 
        (infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY && 
         infmt->pixelformat != CAM_PIXEL_FORMAT_RGB &&
         infmt->pixelformat != CAM_PIXEL_FORMAT_BGR)) return;

    int width = infmt->width / 2;
    int height = infmt->height / 2;
    int bpp = cam_pixel_format_bpp (infmt->pixelformat);
    int stride = width * bpp / 8;

    cam_unit_add_output_format (CAM_UNIT (self), infmt->pixelformat,
            NULL, width, height, stride);

    // allocate working buffer
    int bufsize = 0;
    ippiPyrDownGetBufSize_Gauss5x5 (width, ipp8u, bpp / 8, &bufsize);
    if (self->pyrbuf) free (self->pyrbuf);
    self->pyrbuf = (uint8_t*) malloc (bufsize);

    // allocate output buffer
    if (self->outbuf) {
        g_object_unref (self->outbuf);
    }
    self->outbuf = cam_framebuffer_new_alloc (height * stride);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamippPyrDownGauss *self = (CamippPyrDownGauss*)(super);

    const CamUnitFormat *outfmt = cam_unit_get_output_format (super);

    if (outfmt->width == infmt->width && outfmt->height == infmt->height) {
        // special case passthrough if no resize is actually needed
        cam_unit_produce_frame (super, inbuf, infmt);
        return;
    }

    IppiSize roi = { infmt->width , infmt->height };
    switch (infmt->pixelformat) {
        case CAM_PIXEL_FORMAT_RGB:
        case CAM_PIXEL_FORMAT_BGR:
            ippiPyrDown_Gauss5x5_8u_C3R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, outfmt->row_stride,
                    roi, self->pyrbuf);
            break;
        case CAM_PIXEL_FORMAT_GRAY:
            ippiPyrDown_Gauss5x5_8u_C1R (inbuf->data, infmt->row_stride,
                    self->outbuf->data, outfmt->row_stride,
                    roi, self->pyrbuf);
            break;
        default:
            break;
    }

    // copy the bytesused, timestamp, source_uid, etc. fields.
    cam_framebuffer_copy_metadata (self->outbuf, inbuf);
    self->outbuf->bytesused = outfmt->height * outfmt->row_stride;

    cam_unit_produce_frame (super, self->outbuf, outfmt);
    return;
}
