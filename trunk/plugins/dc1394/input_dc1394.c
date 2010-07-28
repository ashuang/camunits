#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <math.h>

#include <glib-object.h>

#include <dc1394/control.h>
#include <dc1394/vendor/avt.h>

#include <camunits/dbg.h>
#include <camunits/pixels.h>
#include <camunits/plugin.h>
#include <camunits/unit.h>
#include <camunits/unit_driver.h>
#include "input_dc1394.h"

#define NUM_BUFFERS 10

#define VENDOR_ID_POINT_GREY 0xb09d

#define err(args...) fprintf (stderr, args)

/*
 * CamDC1394Driver
 */

typedef struct _CamDC1394Driver CamDC1394Driver;
typedef struct _CamDC1394DriverClass CamDC1394DriverClass;

// boilerplate
#define CAM_DC1394_DRIVER_TYPE  cam_dc1394_driver_get_type()
#define CAM_DC1394_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_DC1394_DRIVER_TYPE, CamDC1394Driver))
#define CAM_DC1394_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_DC1394_DRIVER_TYPE, CamDC1394DriverClass ))
#define CAM_IS_DC1394_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_DC1394_DRIVER_TYPE ))
#define CAM_IS_DC1394_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_DC1394_DRIVER_TYPE))
#define CAM_DC1394_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_DC1394_DRIVER_TYPE, CamDC1394DriverClass))

struct _CamDC1394Driver {
    CamUnitDriver parent;

    dc1394_t * dc1394;
    dc1394camera_list_t * list;
};

struct _CamDC1394DriverClass {
    CamUnitDriverClass parent_class;
};

GType cam_dc1394_driver_get_type (void);

/**
 * Constructor
 */
CamUnitDriver * cam_dc1394_driver_new(void);


// =========================================================================

/*
 * CamDC1394
 */

typedef struct _CamDC1394 CamDC1394;
typedef struct _CamDC1394Class CamDC1394Class;

// boilerplate
#define CAM_DC1394_TYPE  cam_dc1394_get_type()
#define CAM_DC1394(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_DC1394_TYPE, CamDC1394))
#define CAM_DC1394_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_DC1394_TYPE, CamDC1394Class ))
#define CAM_IS_DC1394(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_DC1394_TYPE ))
#define CAM_IS_DC1394_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_DC1394_TYPE))
#define CAM_DC1394_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_DC1394_TYPE, CamDC1394Class))

struct _CamDC1394 {
    CamUnit parent;

    dc1394camera_t * cam;
    unsigned int packet_size;
    int fd;
    int num_buffers;
};

struct _CamDC1394Class {
    CamUnitClass parent_class;
};

GType cam_dc1394_get_type (void);

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

/** 
 * cam_dc1394_new:
 *
 * Constructor.  don't call this function manually.  Instead, let the
 * CamDC1394Driver call it.
 */
CamDC1394 * cam_dc1394_new( dc1394camera_t * camera );

static CamUnit * driver_create_unit (CamUnitDriver * super,
        const CamUnitDescription * udesc);
static int driver_start (CamUnitDriver * super);
static int driver_stop (CamUnitDriver * super);
static int add_all_camera_controls (CamUnit * super);

CAM_PLUGIN_TYPE (CamDC1394Driver, cam_dc1394_driver, CAM_TYPE_UNIT_DRIVER);

static void
cam_dc1394_driver_init (CamDC1394Driver * self)
{
    dbg (DBG_DRIVER, "dc1394 driver constructor\n");
    CamUnitDriver * super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "input", "dc1394");
}

static void
cam_dc1394_driver_class_init (CamDC1394DriverClass * klass)
{
    dbg (DBG_DRIVER, "dc1394 driver class initializer\n");
    klass->parent_class.create_unit = driver_create_unit;
    klass->parent_class.start = driver_start;
    klass->parent_class.stop = driver_stop;
}

static int
driver_start (CamUnitDriver * super)
{
    CamDC1394Driver * self = CAM_DC1394_DRIVER (super);

    self->dc1394 = dc1394_new ();
    if (!self->dc1394)
        return -1;

    if (dc1394_camera_enumerate (self->dc1394, &self->list) < 0) {
        dc1394_free (self->dc1394);
        return -1;
    }

    int i;
    for (i = 0; i < self->list->num; i++) {
        char name[256], id[32];
        dc1394camera_t * camera = dc1394_camera_new (self->dc1394,
                self->list->ids[i].guid);

        snprintf (name, sizeof (name), "%s %s",
                camera->vendor,
                camera->model);
        dc1394_camera_free (camera);

        int64_t uid = self->list->ids[i].guid;
        snprintf (id, sizeof (id), "%"PRIx64, uid);

        CamUnitDescription * udesc = 
            cam_unit_driver_add_unit_description (super, name, id, 
                CAM_UNIT_EVENT_METHOD_FD);

        g_object_set_data (G_OBJECT(udesc), "dc1394-driver-index",
                GINT_TO_POINTER(i));
    }
    
    return 0;
}

static int
driver_stop (CamUnitDriver * super)
{
    CamDC1394Driver * self = CAM_DC1394_DRIVER (super);

    dc1394_camera_free_list (self->list);
    dc1394_free (self->dc1394);

    return CAM_UNIT_DRIVER_CLASS (cam_dc1394_driver_parent_class)->stop (super);
}

static CamUnit *
driver_create_unit (CamUnitDriver * super, const CamUnitDescription * udesc)
{
    CamDC1394Driver * self = CAM_DC1394_DRIVER (super);

    dbg (DBG_DRIVER, "dc1394 driver creating new unit\n");

    g_assert (cam_unit_description_get_driver(udesc) == super);

    int idx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(udesc), 
                "dc1394-driver-index"));

    if (idx >= self->list->num) {
        fprintf (stderr, "Error: invalid unit id %s\n", 
                cam_unit_description_get_unit_id(udesc));
        return NULL;
    }

    dc1394camera_t * camera = dc1394_camera_new (self->dc1394,
            self->list->ids[idx].guid);
    if (!camera)
        return NULL;
    CamDC1394 * result = cam_dc1394_new (camera);

    return CAM_UNIT (result);
}

CAM_PLUGIN_TYPE (CamDC1394, cam_dc1394, CAM_TYPE_UNIT);

void
cam_plugin_initialize (GTypeModule * module)
{
    cam_dc1394_driver_register_type (module);
    cam_dc1394_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return CAM_UNIT_DRIVER (g_object_new (CAM_DC1394_DRIVER_TYPE, NULL));
}

#define CAM_DC1394_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_DC1394_TYPE, CamDC1394Private))
typedef struct _CamDC1394Private CamDC1394Private;

struct _CamDC1394Private {
    int embedded_timestamp;
    int raw1394_fd;
};

static void
cam_dc1394_init (CamDC1394 * self)
{
    CamDC1394Private * priv = CAM_DC1394_GET_PRIVATE (self);
    dbg (DBG_INPUT, "dc1394 constructor\n");

    priv->embedded_timestamp = 0;
    self->num_buffers = NUM_BUFFERS;
}

static void dc1394_finalize (GObject * obj);
static int dc1394_stream_init (CamUnit * super, const CamUnitFormat * format);
static int dc1394_stream_shutdown (CamUnit * super);
static gboolean dc1394_try_produce_frame (CamUnit * super);
static int dc1394_get_fileno (CamUnit * super);
static gboolean dc1394_try_set_control(CamUnit *super,
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);

static void
cam_dc1394_class_init (CamDC1394Class * klass)
{
    dbg (DBG_INPUT, "dc1394 class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = dc1394_finalize;

    klass->parent_class.stream_init = dc1394_stream_init;
    klass->parent_class.stream_shutdown = dc1394_stream_shutdown;
    klass->parent_class.try_produce_frame = dc1394_try_produce_frame;
    klass->parent_class.get_fileno = dc1394_get_fileno;
    klass->parent_class.try_set_control = dc1394_try_set_control;

    g_type_class_add_private (gobject_class, sizeof (CamDC1394Private));
}

static void
dc1394_finalize (GObject * obj)
{
    dbg (DBG_INPUT, "dc1394 finalize\n");
    CamUnit * super = CAM_UNIT (obj);
    CamDC1394 * self = CAM_DC1394 (super);

    if (cam_unit_is_streaming(super)) {
        dbg (DBG_INPUT, "forcibly shutting down dc1394 unit\n");
        dc1394_stream_shutdown (super);
    }
    if(self->cam)
        dc1394_camera_free (self->cam);

    G_OBJECT_CLASS (cam_dc1394_parent_class)->finalize (obj);
}

static CamPixelFormat
dc1394_pixel_format (dc1394color_coding_t color,
        dc1394color_filter_t filter)
{
    switch (color) {
        case DC1394_COLOR_CODING_MONO8:
            return CAM_PIXEL_FORMAT_GRAY;
        case DC1394_COLOR_CODING_RAW8:
            switch (filter) {
                case DC1394_COLOR_FILTER_RGGB:
                    return CAM_PIXEL_FORMAT_BAYER_RGGB;
                case DC1394_COLOR_FILTER_GBRG:
                    return CAM_PIXEL_FORMAT_BAYER_GBRG;
                case DC1394_COLOR_FILTER_GRBG:
                    return CAM_PIXEL_FORMAT_BAYER_GRBG;
                case DC1394_COLOR_FILTER_BGGR:
                    return CAM_PIXEL_FORMAT_BAYER_BGGR;
                default:
                    return CAM_PIXEL_FORMAT_GRAY;
            }
        case DC1394_COLOR_CODING_YUV411:
            return CAM_PIXEL_FORMAT_IYU1;
        case DC1394_COLOR_CODING_YUV422:
            return CAM_PIXEL_FORMAT_UYVY;
        case DC1394_COLOR_CODING_YUV444:
            return CAM_PIXEL_FORMAT_IYU2;
        case DC1394_COLOR_CODING_RGB8:
            return CAM_PIXEL_FORMAT_RGB;
        case DC1394_COLOR_CODING_MONO16:
            return CAM_PIXEL_FORMAT_BE_GRAY16;
        case DC1394_COLOR_CODING_RGB16:
            return CAM_PIXEL_FORMAT_BE_RGB16;
        case DC1394_COLOR_CODING_MONO16S:
            return CAM_PIXEL_FORMAT_BE_SIGNED_GRAY16;
        case DC1394_COLOR_CODING_RGB16S:
            return CAM_PIXEL_FORMAT_BE_SIGNED_RGB16;
        case DC1394_COLOR_CODING_RAW16:
            return CAM_PIXEL_FORMAT_BE_GRAY16;
    }
    return 0;
}

static int
setup_embedded_timestamps (CamDC1394 * self)
{
    uint32_t value;
    if (dc1394_get_adv_control_register (self->cam, 0x2F8, &value) !=
            DC1394_SUCCESS)
        return -1;

    if (!(value & 0x80000000))
        return -1;

    value |= 0x1;

    if (dc1394_set_adv_control_register (self->cam, 0x2F8, value) !=
            DC1394_SUCCESS)
        return -1;

    CamDC1394Private * priv = CAM_DC1394_GET_PRIVATE (self);
    priv->embedded_timestamp = 1;
    printf ("Enabled embedded timestamps for Point Grey camera\n");
    return 0;
}

CamDC1394 *
cam_dc1394_new (dc1394camera_t * cam)
{
    CamDC1394 * self =
        CAM_DC1394 (g_object_new (CAM_DC1394_TYPE, NULL));

    self->cam = cam;

    printf ("Found camera with UID 0x%"PRIx64"\n", cam->guid);

    dc1394format7modeset_t info;

    if (dc1394_format7_get_modeset (cam, &info) != DC1394_SUCCESS)
        goto fail;
    
    int i;
    for (i = 0; i < DC1394_VIDEO_MODE_FORMAT7_NUM; i++) {
        char name[256];
        dc1394format7mode_t * mode = info.mode + i;
        if (!info.mode[i].present)
            continue;

        int j;
        for (j = 0; j < mode->color_codings.num; j++) {
            CamPixelFormat pix =
                dc1394_pixel_format (mode->color_codings.codings[j],
                        mode->color_filter);
            if (0 == pix)
                continue;
            sprintf (name, "%dx%d %s (F7-%d)", mode->max_size_x, 
                    mode->max_size_y, cam_pixel_format_nickname (pix), i);

            int stride = mode->max_size_x * cam_pixel_format_bpp(pix) / 8;

            CamUnitFormat * fmt = 
                cam_unit_add_output_format (CAM_UNIT (self), pix,
                    name, mode->max_size_x, mode->max_size_y, 
                    stride);

            g_object_set_data (G_OBJECT (fmt), "input_dc1394-mode",
                    GINT_TO_POINTER (DC1394_VIDEO_MODE_FORMAT7_0 + i));
            g_object_set_data (G_OBJECT (fmt), "input_dc1394-format7-mode",
                    GINT_TO_POINTER (i));
            g_object_set_data (G_OBJECT (fmt), "input_dc1394-color-coding",
                    GINT_TO_POINTER (j));
        }
    }

    dc1394video_modes_t modes;
    dc1394_video_get_supported_modes (cam, &modes);
    for (i = 0; i < modes.num; i++) {
        dc1394video_mode_t mode = modes.modes[i];
        if (dc1394_is_video_mode_scalable (mode))
            continue;
        dc1394color_coding_t color_coding;
        dc1394_get_color_coding_from_video_mode (cam, mode, &color_coding);
        CamPixelFormat pix = dc1394_pixel_format (color_coding, 0);

        uint32_t width, height, stride;
        dc1394_get_image_size_from_video_mode (cam, mode, &width, &height);
        stride = width * cam_pixel_format_bpp (pix) / 8;

        char name[256];
        sprintf (name, "%dx%d %s", width, height, 
                cam_pixel_format_nickname (pix));

        CamUnitFormat * fmt =
            cam_unit_add_output_format (CAM_UNIT (self), pix, name,
                    width, height, stride);

        g_object_set_data (G_OBJECT (fmt), "input_dc1394-mode",
                GINT_TO_POINTER (mode));
    }

    add_all_camera_controls (CAM_UNIT (self));

    if (self->cam->vendor_id == VENDOR_ID_POINT_GREY)
        setup_embedded_timestamps (self);

    return self;

fail:
    g_object_unref (G_OBJECT (self));
    return NULL;
}

static int
dc1394_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamDC1394 * self = CAM_DC1394 (super);
    dbg (DBG_INPUT, "Initializing DC1394 stream (pxlfmt 0x%x %dx%d)\n",
            format->pixelformat, format->width, format->height);

    dc1394video_mode_t vidmode = GPOINTER_TO_INT (g_object_get_data (
                G_OBJECT (format), "input_dc1394-mode"));
    dc1394_video_set_mode (self->cam, vidmode);
    dc1394_video_set_iso_speed (self->cam, DC1394_ISO_SPEED_400);

    if (dc1394_is_video_mode_scalable (vidmode)) {
        dc1394format7modeset_t info;
        dc1394_format7_get_modeset (self->cam, &info);

        int i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (format), 
                    "input_dc1394-format7-mode"));
        int j = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (format),
                    "input_dc1394-color-coding"));
        dc1394format7mode_t * mode = info.mode + i;
        dc1394color_coding_t color_coding = mode->color_codings.codings[j];

        if (format->pixelformat != dc1394_pixel_format (color_coding,
                    mode->color_filter) ||
                format->width != mode->max_size_x ||
                format->height != mode->max_size_y)
            goto fail;

        int width, height;
        dc1394_format7_set_image_size (self->cam, vidmode,
                format->width, format->height);
        dc1394_format7_set_image_position (self->cam, vidmode, 0, 0);
        width = format->width;
        height = format->height;

        dc1394_format7_set_color_coding (self->cam, vidmode, color_coding);

        uint32_t psize_unit, psize_max;
        dc1394_format7_get_packet_parameters (self->cam, vidmode, &psize_unit,
                &psize_max);

        CamUnitControl * ctl = cam_unit_find_control (super, "packet-size");
        int desired_packet = cam_unit_control_get_int (ctl);

        desired_packet = (desired_packet / psize_unit) * psize_unit;
        if (desired_packet > psize_max)
            desired_packet = psize_max;
        if (desired_packet < psize_unit)
            desired_packet = psize_unit;

        self->packet_size = desired_packet;

        cam_unit_control_force_set_int (ctl, desired_packet);

#if 0
        dc1394_format7_get_recommended_packet_size (self->cam,
                vidmode, &self->packet_size);
        dbg (DBG_INPUT, "DC1394: Using device-recommended packet size of %d\n",
                self->packet_size);
#endif
        if (self->packet_size == 0)
            self->packet_size = 4096;

        dc1394_format7_set_packet_size (self->cam, vidmode, self->packet_size);
        uint64_t bytes_per_frame;
        dc1394_format7_get_total_bytes (self->cam, vidmode, &bytes_per_frame);

        if (bytes_per_frame * self->num_buffers > 25000000) {
            printf ("Reducing dc1394 buffers from %d to ", self->num_buffers);
            self->num_buffers = 25000000 / bytes_per_frame;
            printf ("%d\n", self->num_buffers);
        }
    } else {
        // use the highest supported framerate
        // should make this a proper control...
        dc1394framerates_t framerates;
        dc1394_video_get_supported_framerates(self->cam,vidmode,&framerates);
        dc1394framerate_t framerate=framerates.framerates[framerates.num-1];
        dc1394_video_set_framerate(self->cam, framerate);
    }

//  FIXME

    /* Using libdc1394 for iso streaming */
    if (dc1394_capture_setup (self->cam, self->num_buffers,
                DC1394_CAPTURE_FLAGS_DEFAULT) != DC1394_SUCCESS)
        goto fail;

    unsigned int bandwidth_usage;
    if(dc1394_video_get_bandwidth_usage(self->cam, &bandwidth_usage) == 
            DC1394_SUCCESS) {
        dbg(DBG_INPUT, "Required bandwidth: %d\n", bandwidth_usage);
    } else {
        dbg(DBG_INPUT, "Unable to query bandiwdth usage.\n");
    }

    if (dc1394_video_set_transmission (self->cam, DC1394_ON) !=
            DC1394_SUCCESS)
        goto fail;

    self->fd = dc1394_capture_get_fileno (self->cam);

    return 0;

fail:
    fprintf (stderr, "Error: failed to initialize dc1394 stream\n");
    fprintf (stderr, "\nIF YOU HAVE HAD A CAMERA FAIL TO EXIT CLEANLY OR\n");
    fprintf (stderr, " THE BANDWIDTH HAS BEEN OVER SUBSCRIBED TRY (to reset):\n");
    fprintf (stderr, "dc1394_reset_bus\n\n");
    return -1;
}

static int
dc1394_stream_shutdown (CamUnit * super)
{
    CamDC1394 * self = CAM_DC1394 (super);

    dbg (DBG_INPUT, "Shutting down DC1394 stream\n");

    dc1394_video_set_transmission (self->cam, DC1394_OFF);

    dc1394_capture_stop (self->cam);

    /* chain up to parent, which handles some of the work */
    return 0;
}

#define CYCLE_TIMER_TO_USEC(cycle,secmask) (\
        (((uint32_t)cycle >> 25) & secmask) * 1000000 + \
        (((uint32_t)cycle & 0x01fff000) >> 12) * 125 + \
        ((uint32_t)cycle & 0x00000fff) * 125 / 3072)

#define CYCLE_TIMER_MAX_USEC(secmask)  ((secmask+1)*1000000)

enum {
    TS_SHORT,
    TS_LONG
};

#ifndef raw1394_cycle_timer
struct raw1394_cycle_timer {
	/* contents of Isochronous Cycle Timer register,
	   as in OHCI 1.1 clause 5.13 (also with non-OHCI hosts) */
	uint32_t cycle_timer;

	/* local time in microseconds since Epoch,
	   simultaneously read with cycle timer */
	uint64_t local_time;
};
#define RAW1394_IOC_GET_CYCLE_TIMER		\
	_IOR ('#', 0x30, struct raw1394_cycle_timer)
#endif

static gboolean
dc1394_try_produce_frame (CamUnit * super)
{
    CamDC1394 * self = CAM_DC1394 (super);
    dbg (DBG_INPUT, "DC1394 stream iterate\n");

    if (! cam_unit_is_streaming(super)) return FALSE;

    dc1394video_frame_t * frame;
    if (dc1394_capture_dequeue (self->cam, DC1394_CAPTURE_POLICY_WAIT, &frame)
            != DC1394_SUCCESS) {
        err ("DC1394 dequeue failed\n");
        return FALSE;
    }
    while (frame->frames_behind > 0) {
        dc1394_capture_enqueue (self->cam, frame);
        if (dc1394_capture_dequeue (self->cam, DC1394_CAPTURE_POLICY_WAIT,
                    &frame) != DC1394_SUCCESS) {
            err ("DC1394 dequeue failed\n");
            return FALSE;
        }
    }

    CamFrameBuffer *buf = cam_framebuffer_new(frame->image, frame->image_bytes);

    if (frame->frames_behind >= self->num_buffers-2)
        fprintf (stderr, "Warning: video1394 buffer contains %d frames, "
                "probably dropped frames...\n",
                frame->frames_behind);
    
    buf->bytesused = frame->image_bytes;
    buf->timestamp = frame->timestamp;

    char str[20];
    sprintf (str, "0x%016"PRIx64, self->cam->guid);
    cam_framebuffer_metadata_set (buf, "Source GUID", (uint8_t *) str,
            strlen (str));

    cam_unit_produce_frame (super, buf, cam_unit_get_output_format(super));

    dc1394_capture_enqueue (self->cam, frame);

    g_object_unref (buf);

//    int ts_type = TS_SHORT;
// TODO David - FIXME
//    int i;
//    uint32_t cycle_usec_now;
//    uint64_t systime = 0;
//    int sec_mask = 0;
//    uint32_t cyctime;
//    for (i = 0; i < g_queue_get_length (super->outgoing_q); i++) {
//        CamFrameBuffer * b = g_queue_peek_nth (super->outgoing_q, i);
//        if (b->timestamp != 0 ||
//                (b->bus_timestamp == 0 && !priv->embedded_timestamp))
//            continue;
//
//        if (priv->embedded_timestamp) {
//            ts_type = TS_LONG;
//            b->bus_timestamp = (b->data[0] << 24) |
//                (b->data[1] << 16) | (b->data[2] << 8) |
//                b->data[3];
//            /* bottom 4 bits of cycle offset will be a frame count */
//            b->bus_timestamp &= 0xfffffff0;
//        }
//
//        if (systime == 0) {
//            if (dc1394_read_cycle_timer (self->cam, &cyctime,
//                        &systime) != DC1394_SUCCESS)
//                break;
//            sec_mask = (ts_type == TS_SHORT) ? 0x7 : 0x7f;
//            cycle_usec_now = CYCLE_TIMER_TO_USEC (cyctime, sec_mask);
//        }
//
//        int usec_diff = cycle_usec_now -
//            CYCLE_TIMER_TO_USEC (b->bus_timestamp, sec_mask);
//        if (usec_diff < 0)
//            usec_diff += CYCLE_TIMER_MAX_USEC (sec_mask);
//
//        b->timestamp = systime - usec_diff;
//
//#if 0
//        printf ("%.3f %.3f bus %x nowbus %x d %d\n",
//                (double) systime / 1000000.0,
//                (double) b->timestamp / 1000000.0,
//                b->bus_timestamp, cyctime, usec_diff / 1000);
//#endif
//    }

    return TRUE;
}

static int
dc1394_get_fileno (CamUnit * super)
{
    CamDC1394 * self = CAM_DC1394 (super);

    if (cam_unit_is_streaming(super))
        return self->fd;
    else
        return -1;
}

static const char * feature_ids[] = {
    "brightness",
    "exposure",
    "sharpness",
    "white-balance",
    "hue",
    "saturation",
    "gamma",
    "shutter",
    "gain",
    "iris",
    "focus",
    "temperature",
    "trigger",
    "trigger-delay",
    "white-shading",
    "frame-rate",
    "zoom",
    "pan",
    "tilt",
    "optical-filter",
    "capture-size",
    "capture-quality",
};

static const char * feature_desc[] = {
    "Brightness",
    "Exposure",
    "Sharpness",
    "White Bal.",
    "Hue",
    "Saturation",
    "Gamma",
    "Shutter",
    "Gain",
    "Iris",
    "Focus",
    "Temperature",
    "Trigger",
    "Trig. Delay",
    "White Shading",
    "Frame Rate",
    "Zoom",
    "Pan",
    "Tilt",
    "Optical Filter",
    "Capture Size",
    "Capture Qual.",
};

#define NUM_FLOAT_STEPS 100

static int
add_all_camera_controls (CamUnit * super)
{
    CamDC1394 * self = CAM_DC1394 (super);
    dc1394featureset_t features;
    CamUnitControl * ctl;

    dc1394_feature_get_all (self->cam, &features);

    int i, reread = 0;
    for (i = 0; i < DC1394_FEATURE_NUM; i++) {
        dc1394feature_info_t * f = features.feature + i;
        if (f->available && f->absolute_capable && !f->abs_control) {
            fprintf (stderr, "Enabling absolute control of \"%s\"\n",
                    feature_desc[i]);
            dc1394_feature_set_absolute_control (self->cam, f->id, DC1394_ON);
            reread = 1;
        }
    }
    if (reread)
        dc1394_feature_get_all (self->cam, &features);

    cam_unit_add_control_int (super, "packet-size",
            "Packet Size", 1, 4192, 1, 4192, 1);

    for (i = 0; i < DC1394_FEATURE_NUM; i++) {
        dc1394feature_info_t * f = features.feature + i;

        if (!f->available)
            continue;
#if 0
        fprintf (stderr, "%s, one-push %d abs %d read %d on_off %d auto %d manual %d\n",
                dc1394_feature_desc[i], f->one_push, f->absolute_capable, f->readout_capable,
                f->on_off_capable, f->auto_capable, f->manual_capable);
        fprintf (stderr, "  on %d polar %d auto_active %d min %d max %d value %d\n",
                f->is_on, f->polarity_capable, f->auto_active, f->min, f->max, f->value);
        fprintf (stderr, "  value %f max %f min %f\n", f->abs_value, f->abs_max, f->abs_min);
#endif

#if 0
        if (f->one_push)
            fprintf (stderr, "Warning: One-push available on control \"%s\"\n",
                    feature_desc[i]);
#endif
        int manual_capable = 0;
        int auto_capable = 0;
        for (int j = 0; j < f->modes.num; j++) {
            if (f->modes.modes[j] == DC1394_FEATURE_MODE_MANUAL)
                manual_capable = 1;
            if (f->modes.modes[j] == DC1394_FEATURE_MODE_AUTO)
                auto_capable = 1;
        }

        if (f->id == DC1394_FEATURE_TRIGGER) {
            CamUnitControlEnumValue triggers[] = {
                { 0, "Off", 0 },
                { 1, "Start integration (Mode 0)", 0 },
                { 2, "Bulb shutter (Mode 1)", 0 },
                { 3, "Integrate to Nth (Mode 2)", 0 },
                { 4, "Every Nth frame (Mode 3)", 0 },
                { 5, "Mult. exposures (Mode 4)", 0 },
                { 6, "Mult. bulb exposures (Mode 5)", 0 },
                { 7, "Vendor-specific (Mode 14)", 0 },
                { 8, "Vendor-specific (Mode 15)", 0 },
                { 0, NULL, 0 },
            };
            int cur_val = 0;
            for (int j = 0; j < f->trigger_modes.num; j++) {
                int ind = f->trigger_modes.modes[j] - DC1394_TRIGGER_MODE_0 + 1;
                triggers[ind].enabled = 1;
                if (f->trigger_mode == f->trigger_modes.modes[j])
                    cur_val = ind;
            }
            if (f->on_off_capable) {
                triggers[0].enabled = 1;
                if (f->is_on == DC1394_OFF)
                    cur_val = 0;
            }
            cam_unit_add_control_enum (super, "trigger", "Trigger", 
                    cur_val, 1, triggers);

            int aux_enabled = 0;
            if (cur_val > 0)
                aux_enabled = 1;

            /* Add trigger polarity selection */
            if (f->polarity_capable)
                cam_unit_add_control_boolean (super,
                        "trigger-polarity", "Polarity",
                        f->trigger_polarity, aux_enabled);

            /* Add trigger source selection */
            CamUnitControlEnumValue trig_sources[] = {
                { 0, "Trigger Source 0", 0 },
                { 1, "Trigger Source 1", 0 },
                { 2, "Trigger Source 2", 0 },
                { 3, "Trigger Source 3", 0 },
                { 4, "Software Trigger", 0 },
                { 0, NULL, 0 },
            };

            for (int j = 0; j < f->trigger_sources.num; j++) {
                int source = f->trigger_sources.sources[j];
                int ind = source - DC1394_TRIGGER_SOURCE_MIN;
                trig_sources[ind].enabled = 1;
                if (f->trigger_source == source)
                    cur_val = ind;
            }
            cam_unit_add_control_enum (super, "trigger-source",
                    "Source", cur_val, aux_enabled, trig_sources);

            /* Add one-shot software trigger */
            if (trig_sources[CAM_DC1394_TRIGGER_SOURCE_SOFTWARE].enabled) {
                CamUnitControl * ctl = cam_unit_add_control_boolean (super,
                        "trigger-now", "Trigger",
                        0, aux_enabled);
                cam_unit_control_set_ui_hints(ctl, CAM_UNIT_CONTROL_ONE_SHOT);
            }

            continue;
        }

        if (!f->on_off_capable && !auto_capable && !manual_capable) {
            fprintf (stderr, "Warning: Control \"%s\" has neither auto, "
                    "manual, or off mode\n", feature_desc[i]);
            continue;
        }

        if (f->on_off_capable && !auto_capable && !manual_capable) {
            fprintf (stderr, "Warning: Control \"%s\" has neither auto "
                    "nor manual mode\n", feature_desc[i]);
            continue;
        }

        if (!f->on_off_capable && auto_capable && !manual_capable) {
            fprintf (stderr, "Warning: Control \"%s\" has only auto mode\n",
                    feature_desc[i]);
            continue;
        }

        if (!(!f->on_off_capable && !auto_capable && manual_capable)) {
            CamUnitControlEnumValue entries[] = {
                { CAM_DC1394_MENU_OFF, "Off", f->on_off_capable }, 
                { CAM_DC1394_MENU_AUTO, "Auto", auto_capable }, 
                { CAM_DC1394_MENU_MANUAL, "Manual", manual_capable },
                { 0, NULL, 0 }
            };
            int cur_val = CAM_DC1394_MENU_OFF;
            if (f->is_on && f->current_mode == DC1394_FEATURE_MODE_AUTO)
                cur_val = CAM_DC1394_MENU_AUTO;
            else if (f->is_on)
                cur_val = CAM_DC1394_MENU_MANUAL;

            char *ctl_id = g_strdup_printf ("%s-mode", feature_ids[i]);

            ctl = cam_unit_add_control_enum (super, 
                    ctl_id,
                    (char *) feature_desc[i], cur_val, 1,
                    entries);
            g_object_set_data (G_OBJECT (ctl), "dc1394-control-id",
                    GINT_TO_POINTER (f->id));
            free (ctl_id);
        }

        int enabled = (f->is_on &&
                f->current_mode != DC1394_FEATURE_MODE_AUTO) ||
            (!f->on_off_capable && !auto_capable && manual_capable);

        if (!f->readout_capable && manual_capable) {
            fprintf (stderr, "Control \"%s\" is not readout capable but can "
                    "still be set\n", feature_desc[i]);
        }

        if (f->id == DC1394_FEATURE_WHITE_BALANCE) {
            ctl = cam_unit_add_control_int (super, "white-balance-red",
                    "W.B. Red", f->min, f->max, 1, f->RV_value,
                    enabled);
            g_object_set_data (G_OBJECT (ctl), "dc1394-control-id",
                    GINT_TO_POINTER (f->id));
            ctl = cam_unit_add_control_int (super, "white-balance-blue",
                    "W.B. Blue", f->min, f->max, 1, f->BU_value,
                    enabled);
            g_object_set_data (G_OBJECT (ctl), "dc1394-control-id",
                    GINT_TO_POINTER (f->id));
            continue;
        }

        if (f->absolute_capable && f->abs_control) {
            if (f->abs_max <= f->abs_min) {
                fprintf (stderr, 
                        "Disabling control \"%s\" because min >= max\n",
                        feature_desc[i]);
                cam_unit_add_control_float (super, feature_ids[i],
                        (char *) feature_desc[i], 0, 1,
                        1, 0, 0);
                continue;
            }
            ctl = cam_unit_add_control_float (super, feature_ids[i],
                    (char *) feature_desc[i], f->abs_min,
                    f->abs_max, (f->abs_max - f->abs_min) / NUM_FLOAT_STEPS,
                    f->abs_value, enabled);
            g_object_set_data (G_OBJECT (ctl), "dc1394-control-id",
                    GINT_TO_POINTER (f->id));
        }
        else {
            if (f->max <= f->min) {
                fprintf (stderr, 
                        "Disabling control \"%s\" because min >= max\n",
                        feature_desc[i]);
                cam_unit_add_control_int (super, feature_ids[i],
                        (char *) feature_desc[i], 0, 1,
                        1, 0, 0);
                continue;
            }

            ctl = cam_unit_add_control_int (super, feature_ids[i],
                    (char *) feature_desc[i], f->min, f->max,
                    1, f->value, enabled);
            g_object_set_data (G_OBJECT (ctl), "dc1394-control-id",
                    GINT_TO_POINTER (f->id));
        }
    }

    return 0;
}

static gboolean
dc1394_try_set_control(CamUnit *super, const CamUnitControl *ctl,
        const GValue *proposed, GValue *actual)
{
    CamDC1394 * self = CAM_DC1394 (super);
    dc1394feature_info_t f;
    int val = 0;

    const char *ctl_id = cam_unit_control_get_id(ctl);
    if (!strcmp (ctl_id, "packet-size")) {
        g_value_copy (proposed, actual);

        // re-initialize unit 
        if(cam_unit_is_streaming(super)) {
            cam_unit_control_force_set_val(ctl, proposed);
            const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
            cam_unit_stream_shutdown(super);
            cam_unit_stream_init(super, outfmt);
        }
        return TRUE;
    }

    if (G_VALUE_TYPE (proposed) == G_TYPE_INT)
        val = g_value_get_int (proposed);

    if (!strcmp (ctl_id, "trigger-polarity")) {
        dc1394_external_trigger_set_polarity (self->cam,
                g_value_get_boolean (proposed));
        dc1394trigger_polarity_t newpol;
        dc1394_external_trigger_get_polarity (self->cam, &newpol);
        g_value_set_boolean (actual, newpol);
        return TRUE;
    }

    if (!strcmp (ctl_id, "trigger-source")) {
        dc1394_external_trigger_set_source (self->cam,
                val + DC1394_TRIGGER_SOURCE_MIN);
        dc1394trigger_source_t source;
        dc1394_external_trigger_get_source (self->cam, &source);
        g_value_set_int (actual, source - DC1394_TRIGGER_SOURCE_MIN);
        return TRUE;
    }

    if (!strcmp (ctl_id, "trigger-now")) {
        dc1394_software_trigger_set_power (self->cam,
                g_value_get_boolean (proposed));
        dc1394switch_t power;
        dc1394_software_trigger_get_power (self->cam,
                &power);
        g_value_set_boolean (actual, power);
        return TRUE;
    }

    if (!strcmp (ctl_id, "trigger")) {
        if (val == 0)
            dc1394_external_trigger_set_power (self->cam, DC1394_OFF);
        else {
            dc1394_external_trigger_set_power (self->cam, DC1394_ON);
            dc1394_external_trigger_set_mode (self->cam, val - 1 +
                    DC1394_TRIGGER_MODE_0);
        }
        f.id = DC1394_FEATURE_TRIGGER;
        dc1394_feature_get (self->cam, &f);
        if (f.is_on)
            g_value_set_int (actual,
                    f.trigger_mode - DC1394_TRIGGER_MODE_0 + 1);
        else
            g_value_set_int (actual, 0);

        /* Enable or disable other trigger-related controls accordingly */
        CamUnitControl * ctl2;
        ctl2 = cam_unit_find_control (super, "trigger-polarity");
        if (ctl2)
            cam_unit_control_set_enabled (ctl2, f.is_on);

        ctl2 = cam_unit_find_control (super, "trigger-source");
        if (ctl2)
            cam_unit_control_set_enabled (ctl2, f.is_on);

        ctl2 = cam_unit_find_control (super, "trigger-now");
        if (ctl2)
            cam_unit_control_set_enabled (ctl2, f.is_on);
        return TRUE;
    }

    f.id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (ctl),
                "dc1394-control-id"));

    char ctlid[64];
    strncpy (ctlid, ctl_id, 64);
    char * suffix = strstr (ctlid, "-mode");
    if (suffix) {
        if (val == 0)
            dc1394_feature_set_power (self->cam, f.id, DC1394_OFF);
        else {
            dc1394_feature_set_power (self->cam, f.id, DC1394_ON);
            dc1394_feature_set_mode (self->cam, f.id, (val == 1) ?
                    DC1394_FEATURE_MODE_AUTO : DC1394_FEATURE_MODE_MANUAL);
        }
        dc1394_feature_get (self->cam, &f);
        if (!f.is_on)
            g_value_set_int (actual, CAM_DC1394_MENU_OFF);
        else if (f.current_mode == DC1394_FEATURE_MODE_AUTO)
            g_value_set_int (actual, CAM_DC1394_MENU_AUTO);
        else
            g_value_set_int (actual, CAM_DC1394_MENU_MANUAL);

        *suffix = '\0';
        if (!strcmp (ctlid, "white-balance")) {
            CamUnitControl * ctl2 = cam_unit_find_control (super,
                    "white-balance-red");
            cam_unit_control_modify_int (ctl2, f.min, f.max, 1,
                    f.is_on && f.current_mode != DC1394_FEATURE_MODE_AUTO);
            cam_unit_control_force_set_int (ctl2, f.RV_value);
            CamUnitControl * ctl3 = cam_unit_find_control (super,
                    "white-balance-blue");
            cam_unit_control_modify_int (ctl3, f.min, f.max, 1,
                    f.is_on && f.current_mode != DC1394_FEATURE_MODE_AUTO);
            cam_unit_control_force_set_int (ctl3, f.BU_value);
            return TRUE;
        }

        CamUnitControl * ctl2 = cam_unit_find_control (super, ctlid);
        if (!ctl2) return TRUE;
        CamUnitControlType ctl2_type = cam_unit_control_get_control_type(ctl2);

        if (ctl2_type == CAM_UNIT_CONTROL_TYPE_INT) {
            cam_unit_control_modify_int (ctl2, f.min, f.max, 1,
                    f.is_on && f.current_mode != DC1394_FEATURE_MODE_AUTO);
            cam_unit_control_force_set_int (ctl2, f.value);
        }
        else {
            cam_unit_control_modify_float (ctl2, f.abs_min, f.abs_max,
                    (f.abs_max - f.abs_min) / NUM_FLOAT_STEPS,
                    f.is_on && f.current_mode != DC1394_FEATURE_MODE_AUTO);
            cam_unit_control_force_set_float (ctl2, f.abs_value);
        }
        return TRUE;
    }

    if (f.id == DC1394_FEATURE_WHITE_BALANCE) {
        dc1394_feature_get (self->cam, &f);
        if (strstr (ctl_id, "blue"))
            dc1394_feature_whitebalance_set_value (self->cam,
                    val, f.RV_value);
        else
            dc1394_feature_whitebalance_set_value (self->cam,
                    f.BU_value, val);
        dc1394_feature_get (self->cam, &f);
        if (strstr (ctl_id, "blue"))
            g_value_set_int (actual, f.BU_value);
        else
            g_value_set_int (actual, f.RV_value);
        return TRUE;
    }

    if (G_VALUE_TYPE (proposed) == G_TYPE_FLOAT) {
        float fval = g_value_get_float (proposed);
        dc1394_feature_set_absolute_value (self->cam, f.id, fval);
        dc1394_feature_get (self->cam, &f);
        if (f.readout_capable)
            g_value_set_float (actual, f.abs_value);
        else
            g_value_copy (proposed, actual);
    }
    else {
        dc1394_feature_set_value (self->cam, f.id, val);
        dc1394_feature_get (self->cam, &f);
        if (f.readout_capable)
            g_value_set_int (actual, f.value);
        else
            g_value_copy (proposed, actual);
    }
    return TRUE;
}
