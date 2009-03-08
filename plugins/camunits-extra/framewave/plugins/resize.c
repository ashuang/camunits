#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include <camunits/plugin.h>
#include <fwImage.h>

#define MAX_WIDTH (2 << 15)
#define MAX_HEIGHT (2 << 15)
#define MIN_WIDTH 1
#define MIN_HEIGHT 1

typedef struct {
    CamUnit parent;

    int size_requested;
    CamUnitControl *width_ctl;
    CamUnitControl *height_ctl;
    CamUnitControl *lock_aspect_ctl;
} CamfwResize;

typedef struct {
    CamUnitClass parent_class;
} CamfwResizeClass;

static CamfwResize * camfw_resize_new(void);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static gboolean _try_set_control (CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);

GType camfw_resize_get_type (void);
CAM_PLUGIN_TYPE (CamfwResize, camfw_resize, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camfw_resize_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("framewave", "resize",
            "Resize", 0, (CamUnitConstructor)camfw_resize_new, module);
}

static void
camfw_resize_class_init (CamfwResizeClass *klass)
{
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camfw_resize_init (CamfwResize *self)
{
    // constructor.  Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);
    self->lock_aspect_ctl = cam_unit_add_control_boolean (super, 
            "lock-aspect", "Lock Aspect", 1, 1);
    self->width_ctl = cam_unit_add_control_int (super, "width", "Width", 
            1, MAX_WIDTH, 1, 1, 1);
    self->height_ctl = cam_unit_add_control_int (super, "height", "Height", 
            1, MAX_HEIGHT, 1, 1, 1);
    self->size_requested = 0;

    // suggest that UI widgets for this filter display the width and height
    // controls as spin buttons
    cam_unit_control_set_ui_hints (self->width_ctl, 
            CAM_UNIT_CONTROL_SPINBUTTON);
    cam_unit_control_set_ui_hints (self->height_ctl, 
            CAM_UNIT_CONTROL_SPINBUTTON);
    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamfwResize * 
camfw_resize_new()
{
    return (CamfwResize*)(g_object_new(camfw_resize_get_type(), NULL));
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    const CamUnitFormat *outfmt = cam_unit_get_output_format (super);

    if (outfmt->width == infmt->width && outfmt->height == infmt->height) {
        // special case passthrough if no resize is actually needed
        cam_unit_produce_frame (super, inbuf, infmt);
        return;
    }

    int buf_sz = outfmt->height * outfmt->row_stride;
    CamFrameBuffer *outbuf = cam_framebuffer_new_alloc(buf_sz);

    FwiSize srcSize = { infmt->width, infmt->height };
    FwiRect srcRoi = { 0, 0, infmt->width, infmt->height };
    FwiSize dstRoiSize = { outfmt->width, outfmt->height };
    
    double sf_x = (double) outfmt->width / infmt->width;
    double sf_y = (double) outfmt->height / infmt->height;

    int interp_method = FWI_INTER_LINEAR;

    switch (infmt->pixelformat) {
        case CAM_PIXEL_FORMAT_GRAY:
            fwiResize_8u_C1R (inbuf->data, srcSize, infmt->row_stride,
                    srcRoi, outbuf->data, outfmt->row_stride, dstRoiSize,
                    sf_x, sf_y, interp_method);
            break;
        case CAM_PIXEL_FORMAT_RGB:
        case CAM_PIXEL_FORMAT_BGR:
            fwiResize_8u_C3R (inbuf->data, srcSize, infmt->row_stride,
                    srcRoi, outbuf->data, outfmt->row_stride, dstRoiSize,
                    sf_x, sf_y, interp_method);
            break;
        case CAM_PIXEL_FORMAT_BGRA:
            fwiResize_8u_C4R (inbuf->data, srcSize, infmt->row_stride,
                    srcRoi, outbuf->data, outfmt->row_stride, dstRoiSize,
                    sf_x, sf_y, interp_method);
            break;
        default:
            break;
    }

    // copy the bytesused, timestamp, source_uid, etc. fields.
    cam_framebuffer_copy_metadata (outbuf, inbuf);
    outbuf->bytesused = outfmt->row_stride * outfmt->height;

    cam_unit_produce_frame (super, outbuf, outfmt);
    g_object_unref (outbuf);
    return;
}

static void
update_output_format (CamfwResize *self, int width, int height,
        const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (CAM_UNIT (self));
    if (!infmt) return;
    if (infmt->pixelformat != CAM_PIXEL_FORMAT_RGB &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_BGR &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY &&
        infmt->pixelformat != CAM_PIXEL_FORMAT_BGRA) return;
    if (!width || !height) return;

    int bpp = cam_pixel_format_bpp (infmt->pixelformat);
    int stride = width * bpp / 8;

    cam_unit_add_output_format (CAM_UNIT (self), infmt->pixelformat,
            NULL, width, height, stride);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamfwResize *self = (CamfwResize*) (super);
    
    if (infmt) {
        int new_width, new_height;

        if (self->size_requested) {
            new_width = cam_unit_control_get_int (self->width_ctl);
            new_height = cam_unit_control_get_int (self->height_ctl);
        } else {
            new_width = infmt->width;
            new_height = infmt->height;
        }

        update_output_format (self, new_width, new_height, infmt);
        cam_unit_control_force_set_int (self->width_ctl, new_width);
        cam_unit_control_force_set_int (self->height_ctl, new_height);
        cam_unit_control_set_enabled (self->width_ctl, TRUE);
        cam_unit_control_set_enabled (self->height_ctl, TRUE);
    } else {
        update_output_format (self, 0, 0, infmt);
    }
}

static gboolean 
_try_set_control (CamUnit *super, 
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual)
{
    CamfwResize *self = (CamfwResize*) (super);

    int old_width = cam_unit_control_get_int (self->width_ctl);
    int old_height = cam_unit_control_get_int (self->height_ctl);

    int new_width = old_width;
    int new_height = old_height;

    double in_aspect = (double) old_width / old_height;

    if (ctl == self->width_ctl) {
        new_width = g_value_get_int (proposed);
        if (new_width > MAX_WIDTH) new_width = MAX_WIDTH;
        if (new_width < MIN_WIDTH) new_width = MIN_WIDTH;

        // if the aspect ratio is locked, then maybe need to adjust both 
        // width and height
        if (cam_unit_control_get_boolean (self->lock_aspect_ctl)) { 
            new_height = (int) round (new_width / in_aspect);

            // but still impose size limits
            if (new_height > MAX_HEIGHT) {
                new_height = MAX_HEIGHT;
                new_width = (int) round (new_height * in_aspect);
            } else if (new_height < MIN_HEIGHT) {
                new_height = MIN_HEIGHT;
                new_width = (int) round (new_height * in_aspect);
            }

            cam_unit_control_force_set_int (self->height_ctl, new_height);
        }
        self->size_requested = 1;
        g_value_set_int (actual, new_width);
    } else if (ctl == self->height_ctl) {
        new_height = g_value_get_int (proposed);
        if (new_height > MAX_HEIGHT) new_height = MAX_HEIGHT;
        if (new_height < MIN_HEIGHT) new_height = MIN_HEIGHT;

        // if the aspect ratio is locked, then maybe need to adjust both 
        // width and height
        if (cam_unit_control_get_boolean (self->lock_aspect_ctl)) { 
            new_width = (int) round (new_height * in_aspect);

            // but still impose size limits
            if (new_width > MAX_WIDTH) {
                new_width = MAX_WIDTH;
                new_height = (int) round (new_width / in_aspect);
            } else if (new_width < MIN_WIDTH) {
                new_width = MIN_WIDTH;
                new_height = (int) round (new_width / in_aspect);
            }

            cam_unit_control_force_set_int (self->width_ctl, new_width);
        }
        self->size_requested = 1;
        g_value_set_int (actual, new_height);
    } else if (ctl == self->lock_aspect_ctl) {
        g_value_copy (proposed, actual);
        new_height = (int) round (new_width / in_aspect);
        cam_unit_control_force_set_int (self->height_ctl, new_height);
    }

    CamUnit *input = cam_unit_get_input(super);
    if (input) {
        const CamUnitFormat *infmt = cam_unit_get_output_format (input);
        if ((old_width != new_width || old_height != new_height) && infmt) {
            cam_unit_stream_shutdown (super);

            update_output_format (self, new_width, new_height, infmt);

            cam_unit_stream_init (super, NULL);
        }
    }

    return TRUE;
}
