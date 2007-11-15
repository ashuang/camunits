#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dc1394/control.h>
#include <dc1394/vendor/avt.h>
//#include <dc1394/dc1394_vendor_prosilica.h>
#include <math.h>

#include <libcam/dbg.h>
#include <libcam/plugin.h>
#include <libcam/pixels.h>

#include "input_ladybug2.h"


#define FEATURE_HDR 0xf000
#define FEATURE_LUT 0xf001

#define NUM_BUFFERS 2

#define LADYBUG2_PACKAGE "input.ladybug2"

#define err(args...) fprintf (stderr, args)

#define s2s(status) ((status)==DC1394_SUCCESS?"OK":"FAILURE")

static CamUnit * driver_create_unit (CamUnitDriver * super,
        const CamUnitDescription * udesc);
static void driver_finalize (GObject * obj);
static int driver_start (CamUnitDriver * super);
static int driver_stop (CamUnitDriver * super);
//static int add_all_camera_controls (CamUnit * super);

#if 0
G_DEFINE_TYPE (LB2Ladybug2, lb2_ladybug2, CAM_TYPE_UNIT);
G_DEFINE_TYPE (LB2Ladybug2Driver, lb2_ladybug2_driver, CAM_TYPE_UNIT_DRIVER);
#else
CAM_PLUGIN_TYPE (LB2Ladybug2, lb2_ladybug2, CAM_TYPE_UNIT);
CAM_PLUGIN_TYPE (LB2Ladybug2Driver, lb2_ladybug2_driver, 
        CAM_TYPE_UNIT_DRIVER);

void cam_plugin_initialize (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module);

void
cam_plugin_initialize (GTypeModule * module)
{
    printf ("initializing plugin\n");
    lb2_ladybug2_driver_register_type (module);
    lb2_ladybug2_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    LB2Ladybug2Driver *self = 
        LB2_LADYBUG2_DRIVER (
                g_object_new (LB2_LADYBUG2_DRIVER_TYPE, NULL));
    return CAM_UNIT_DRIVER (self);
}

#endif

static void
lb2_ladybug2_driver_init (LB2Ladybug2Driver * self)
{
    dbg (DBG_DRIVER, "Ladybug2 driver constructor\n");
    CamUnitDriver * super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_package (super, LADYBUG2_PACKAGE);

    self->cameras = NULL;
    self->num_cameras = 0;
}

static void
lb2_ladybug2_driver_class_init (LB2Ladybug2DriverClass * klass)
{
    dbg (DBG_DRIVER, "Ladybug2 driver class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = driver_finalize;
    klass->parent_class.create_unit = driver_create_unit;
    klass->parent_class.start = driver_start;
    klass->parent_class.stop = driver_stop;
}

static void
driver_finalize (GObject * obj)
{
    dbg (DBG_DRIVER, "Ladybug2 driver finalize\n");
    LB2Ladybug2Driver * self = LB2_LADYBUG2_DRIVER (obj);

    if (self->num_cameras)
        err ("Warning: Ladybug2 driver finalized before stopping\n");

    G_OBJECT_CLASS (lb2_ladybug2_driver_parent_class)->finalize (obj);
}

CamUnitDriver *
lb2_ladybug2_driver_new ()
{
    return CAM_UNIT_DRIVER (g_object_new (LB2_LADYBUG2_DRIVER_TYPE, NULL));
}

static int
driver_start (CamUnitDriver * super)
{
    LB2Ladybug2Driver * self = LB2_LADYBUG2_DRIVER (super);

    if (dc1394_find_cameras (&self->cameras, &self->num_cameras)
            != DC1394_SUCCESS)
        return -1;

    int i;
    for (i = 0; i < self->num_cameras; i++) {
        char name[256], id[32];

        snprintf (name, sizeof (name), "%s %s",
                self->cameras[i]->vendor,
                self->cameras[i]->model);
        snprintf (id, sizeof (id), "%s:%d", LADYBUG2_PACKAGE, i);
        cam_unit_driver_add_unit_description (super, name, id, 
                CAM_UNIT_EVENT_METHOD_FD);
    }
    
    return 0;
}

static int
driver_stop (CamUnitDriver * super)
{
    LB2Ladybug2Driver * self = LB2_LADYBUG2_DRIVER (super);

    int i;
    for (i = 0; i < self->num_cameras; i++) {
        dc1394_free_camera (self->cameras[i]);
    }
    free (self->cameras);

    self->num_cameras = 0;
    self->cameras = NULL;

    return CAM_UNIT_DRIVER_CLASS (
            lb2_ladybug2_driver_parent_class)->stop (super);

}

static CamUnit *
driver_create_unit (CamUnitDriver * super, const CamUnitDescription * udesc)
{
    LB2Ladybug2Driver * self = LB2_LADYBUG2_DRIVER (super);

    dbg (DBG_DRIVER, "Ladybug2 driver creating new unit\n");

    char ** words = g_strsplit (udesc->unit_id, ":", 2);
    g_assert (!strcmp (words[0], LADYBUG2_PACKAGE));

    char * end = NULL;
    int idx = strtoul (words[1], &end, 10);
    g_assert (words[1] != end && *end == '\0');
    g_strfreev (words);

    if (idx >= self->num_cameras) {
        fprintf (stderr, "Error: invalid unit id %s\n", udesc->unit_id);
        return NULL;
    }

    LB2Ladybug2 * result = lb2_ladybug2_new (self->cameras[idx]);

    return CAM_UNIT (result);
}

static void
lb2_ladybug2_init (LB2Ladybug2 * self)
{
    dbg (DBG_INPUT, "Ladybug2 constructor\n");
}

static void dc1394_finalize (GObject * obj);
static int dc1394_stream_init (CamUnit * super, const CamUnitFormat * format);
static int dc1394_stream_shutdown (CamUnit * super);
static int dc1394_stream_on (CamUnit * super);
static int dc1394_stream_off (CamUnit * super);
static void dc1394_try_produce_frame (CamUnit * super);
static int dc1394_get_fileno (CamUnit * super);
//static gboolean dc1394_try_set_control(CamUnit *super,
//        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);

static void
lb2_ladybug2_class_init (LB2Ladybug2Class * klass)
{
    dbg (DBG_INPUT, "Ladybug2 class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = dc1394_finalize;

    klass->parent_class.stream_init = dc1394_stream_init;
    klass->parent_class.stream_shutdown = dc1394_stream_shutdown;
    klass->parent_class.stream_on = dc1394_stream_on;
    klass->parent_class.stream_off = dc1394_stream_off;
    klass->parent_class.try_produce_frame = dc1394_try_produce_frame;
    klass->parent_class.get_fileno = dc1394_get_fileno;
//    klass->parent_class.try_set_control = dc1394_try_set_control;
}

static void
dc1394_finalize (GObject * obj)
{
    dbg (DBG_INPUT, "Ladybug2 finalize\n");
    CamUnit * super = CAM_UNIT (obj);

    if (super->status != CAM_UNIT_STATUS_IDLE) {
        dbg (DBG_INPUT, "forcibly shutting down Ladybug2 unit\n");
        dc1394_stream_shutdown (super);
    }

    G_OBJECT_CLASS (lb2_ladybug2_parent_class)->finalize (obj);
}

LB2Ladybug2 *
lb2_ladybug2_new (dc1394camera_t * cam)
{
    LB2Ladybug2 * self =
        LB2_LADYBUG2 (g_object_new (LB2_LADYBUG2_TYPE, NULL));

    self->cam = cam;

//    dc1394format7modeset_t info;

    if (dc1394_video_set_mode (cam, DC1394_VIDEO_MODE_FORMAT7_0) !=
            DC1394_SUCCESS)
        goto fail;
    cam_unit_add_output_format_full( CAM_UNIT(self),
            CAM_PIXEL_FORMAT_BAYER_BGGR,
            "1024x7608 BGGR",
            1024, 4608, 1024, 1024 * 4608 );

    return self;

fail:
    g_object_unref (G_OBJECT (self));
    return NULL;
}

static int
dc1394_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    LB2Ladybug2 * self = LB2_LADYBUG2 (super);
    dbg (DBG_INPUT, "Initializing Ladybug2 stream (pxlfmt 0x%x %dx%d)\n",
            format->pixelformat, format->width, format->height);

    /* chain up to parent, which handles most of the error checking */
    if (CAM_UNIT_CLASS (lb2_ladybug2_parent_class)->stream_init (super,
                format) < 0)
        return -1;

    /* The parent set our status to READY, so undo that until it's
     * really true. */
    cam_unit_set_status (super, CAM_UNIT_STATUS_IDLE);

#if 1
    // initialize this camera
    dc1394camera_t *cam = self->cam;
    int status;
    status = dc1394_reset_camera( cam );
    DC1394_ERR_RTN(status, "resetting camera");

    // set 1394b mode
    uint32_t opmode ;
    status = dc1394_video_get_operation_mode( cam, &opmode );
    DC1394_ERR_RTN(status, "retrieving operation mode");
    dbg(DBG_INPUT, "get operation mode (%s): %u\n", s2s(status), opmode);

    opmode = DC1394_OPERATION_MODE_LEGACY;
    dbg(DBG_INPUT, "setting operation mode...\n");
    status = dc1394_video_set_operation_mode( cam, opmode );
    DC1394_ERR_RTN(status, "retrieving operation mode");

    status = dc1394_video_get_operation_mode( cam, &opmode );
    DC1394_ERR_RTN(status, "retrieving operation mode");
    dbg(DBG_INPUT, "get operation mode (%s): %u\n", s2s(status), opmode);

    // get camera channel and speed setting
    dc1394speed_t speed;
    status = dc1394_video_get_iso_speed(cam, &speed);
    DC1394_ERR_RTN(status, "getting ISO speed");
    dbg(DBG_INPUT, "get speed: %u\n", speed);

    speed = DC1394_ISO_SPEED_400;
    dbg(DBG_INPUT, "setting speed...\n");
    status = dc1394_video_set_iso_speed(cam, speed);
    DC1394_ERR_RTN(status, "setting speed");

    status = dc1394_video_get_iso_speed(cam, &speed);
    DC1394_ERR_RTN(status, "getting ISO speed");
    dbg(DBG_INPUT, "get speed: %u\n", speed);

    // get and set framerate
    uint32_t framerate;
    status = dc1394_video_get_framerate(cam, &framerate);
    DC1394_ERR_RTN(status, "getting framerate");
    dbg(DBG_INPUT, "get framerate: %u\n", framerate);

    framerate = DC1394_FRAMERATE_15;
    dbg(DBG_INPUT, "setting framerate...\n");
    status = dc1394_video_set_framerate(cam, framerate);
    DC1394_ERR_RTN(status, "setting framerate");

    status = dc1394_video_get_framerate(cam, &framerate);
    DC1394_ERR_RTN(status, "getting framerate");
    dbg(DBG_INPUT, "get framerate: %u\n", framerate);

    // check the image size for the capture
    uint32_t mode = DC1394_VIDEO_MODE_FORMAT7_0;
    uint32_t lb2_width = 1024, lb2_height = 768 * 6;
    status = dc1394_format7_set_image_size(cam, mode, lb2_width, lb2_height);
    DC1394_ERR_RTN(status, "setting image size");

    status = dc1394_format7_get_image_size(cam, mode, &lb2_width, &lb2_height);
    DC1394_ERR_RTN(status, "querying image size");
    dbg(DBG_INPUT, "capture image width: %d height: %d\n", lb2_width, lb2_height);

    // set video mode
    dbg(DBG_INPUT, "setting video mode...\n");
    status = dc1394_video_set_mode(cam, mode);
    DC1394_ERR_RTN(status, "setting video mode\n");

    // bytes per packet
    uint32_t bpp;
    status = dc1394_format7_get_recommended_byte_per_packet(cam, mode, &bpp);
    DC1394_ERR_RTN(status, "getting recommended bytes per packet\n");
    dbg(DBG_INPUT, "recommended bytes per packet: %d\n", bpp);
    status = dc1394_format7_set_byte_per_packet(cam, mode, 4096);
    DC1394_ERR_RTN(status, "setting bytes per packet\n");
    status = dc1394_format7_get_byte_per_packet(cam, mode, &bpp);
    DC1394_ERR_RTN(status, "getting bytes per packet\n");
    dbg(DBG_INPUT, "bytes per packet: %d\n", bpp);
//    idd->packet_size = 4096;

    uint32_t ppf;
    status = dc1394_format7_get_packet_per_frame(cam, mode, &ppf);
    DC1394_ERR_RTN(status, "getting packets per frame\n");
    dbg(DBG_INPUT, "packets per frame: %d\n", ppf);

    // check the image size in bytes
    uint64_t totalbytes;
    status = dc1394_format7_get_total_bytes(cam, mode, &totalbytes);
    DC1394_ERR_RTN(status, "querying bytes per frame");
    dbg(DBG_INPUT, "bytes per frame: %"PRId64"\n", totalbytes);

//    device->pixelformat = pixelformat;
//    device->width = lb2_width;
//    device->height = lb2_height;
//    device->stride = totalbytes / device->height;
//    device->fps_numer = 7;
//    device->fps_denom = 1;
//    device->true_fps = 0.0;
//    idd->last_frame = -1;
//    idd->last_cycle = -1;
#endif

    if (dc1394_capture_setup (self->cam, NUM_BUFFERS, 
                DC1394_CAPTURE_FLAGS_DEFAULT)
            != DC1394_SUCCESS)
        goto fail;

    self->fd = dc1394_capture_get_fileno (self->cam);
    dbg(DBG_INPUT, "dc1394 capture fileno: %d\n", self->fd);

    cam_unit_set_status (super, CAM_UNIT_STATUS_READY);
    return 0;

fail:
    return -1;
}

static int
dc1394_stream_shutdown (CamUnit * super)
{
    LB2Ladybug2 * self = LB2_LADYBUG2 (super);

    if (super->status != CAM_UNIT_STATUS_READY &&
            super->status != CAM_UNIT_STATUS_STREAMING) {
        err ("Ladybug2: cannot shut down an IDLE unit\n");
        return -1;
    }

    if (super->status == CAM_UNIT_STATUS_STREAMING) {
        if (dc1394_stream_off (super) < 0)
            return -1;
    }

    dbg (DBG_INPUT, "Shutting down Ladybug2 stream\n");

    dc1394_capture_stop (self->cam);

    /* chain up to parent, which handles some of the work */
    return CAM_UNIT_CLASS (lb2_ladybug2_parent_class)->stream_shutdown (super);
}

static int
dc1394_stream_on (CamUnit * super)
{
    LB2Ladybug2 * self = LB2_LADYBUG2 (super);

    /* chain up to parent, which handles most of the error checking */
    if (CAM_UNIT_CLASS (lb2_ladybug2_parent_class)->stream_on (super) < 0)
        return -1;

    dbg (DBG_INPUT, "Ladybug2 stream on\n");

    /* The parent set our status to READY, so undo that until it's
     * really true. */
    cam_unit_set_status (super, CAM_UNIT_STATUS_READY);

    if (dc1394_video_set_transmission (self->cam, DC1394_ON) !=
            DC1394_SUCCESS)
        return -1;

    cam_unit_set_status (super, CAM_UNIT_STATUS_STREAMING);
    return 0;
}

static int
dc1394_stream_off (CamUnit * super)
{
    LB2Ladybug2 * self = LB2_LADYBUG2 (super);

    dbg (DBG_INPUT, "Ladybug2 stream off\n");

    if (super->status != CAM_UNIT_STATUS_STREAMING) {
        err ("Unit: unit must be STREAMING to stop streaming\n");
        return -1;
    }

    dc1394_video_set_transmission (self->cam, DC1394_OFF);

    return CAM_UNIT_CLASS (lb2_ladybug2_parent_class)->stream_off (super);
}

static void
dc1394_try_produce_frame (CamUnit * super)
{
    LB2Ladybug2 * self = LB2_LADYBUG2 (super);

    dbg (DBG_INPUT, "Ladybug2 stream iterate\n");

    if (super->status != CAM_UNIT_STATUS_STREAMING)
        return;

    dc1394video_frame_t * frame;
    if (dc1394_capture_dequeue (self->cam, DC1394_CAPTURE_POLICY_WAIT, &frame)
            != DC1394_SUCCESS) {
        err ("Ladybug2 dequeue failed\n");
        return;
    }

    CamFrameBuffer * buf =
        cam_framebuffer_new (frame->image, frame->image_bytes);
    
    buf->data = frame->image;
    buf->length = frame->image_bytes;
    buf->bytesused = frame->image_bytes;
    buf->timestamp = frame->timestamp;
    buf->source_uid = self->cam->euid_64;

    cam_unit_produce_frame (super, buf, super->fmt);

    dc1394_capture_enqueue (self->cam, frame);
    g_object_unref (buf);

    return;
}

static int
dc1394_get_fileno (CamUnit * super)
{
    LB2Ladybug2 * self = LB2_LADYBUG2 (super);

    if (super->status != CAM_UNIT_STATUS_IDLE)
        return self->fd;
    else
        return -1;
}
