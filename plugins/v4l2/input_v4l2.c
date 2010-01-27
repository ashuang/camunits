#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <errno.h>

#include <glib-object.h>

#include "camunits/plugin.h"
#include "camunits/dbg.h"

#define err(args...) fprintf(stderr, args)

#define V4L2_BASE   "/dev/video"

#define NUM_BUFFERS 5

typedef struct _CamV4L2Driver {
    CamUnitDriver parent;
} CamV4L2Driver;

typedef struct _CamV4L2DriverClass {
    CamUnitDriverClass parent_class;
} CamV4L2DriverClass;

typedef struct _CamV4L2 {
    CamUnit parent;

    /*< public >*/

    /*< private >*/
    char *dev_path;
    int fd;
    int num_buffers;
    uint8_t ** buffers;
    int buffer_length;
    int buffers_outstanding;

    int use_try_fmt;

    CamUnitControl *standard_ctl;
//    CamUnitControl *stream_ctl;
} CamV4L2;

typedef struct _CamV4L2Class {
    CamUnitClass parent_class;
} CamV4L2Class;

GType cam_v4l2_driver_get_type (void);
GType cam_v4l2_get_type (void);

CAM_PLUGIN_TYPE(CamV4L2Driver, cam_v4l2_driver, CAM_TYPE_UNIT_DRIVER);
CAM_PLUGIN_TYPE(CamV4L2, cam_v4l2, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize(GTypeModule * module);
void cam_plugin_initialize(GTypeModule * module)
{
    cam_v4l2_driver_register_type(module);
    cam_v4l2_register_type(module);
}

CamUnitDriver * cam_plugin_create(GTypeModule * module);
CamUnitDriver * cam_plugin_create(GTypeModule * module)
{
    return CAM_UNIT_DRIVER(g_object_new(cam_v4l2_driver_get_type(), NULL));
}

static CamV4L2 * cam_v4l2_new (const char *path);

static CamUnit * driver_create_unit (CamUnitDriver * super,
        const CamUnitDescription * udesc);
static void driver_finalize (GObject * obj);
static int driver_start (CamUnitDriver * super);
static int driver_stop (CamUnitDriver * super);

static void
fourcc_to_str(uint32_t fcc, char *result)
{
    result[0] = fcc & 0xff;
    result[1] = (fcc >> 8) & 0xff;
    result[2] = (fcc >> 16) & 0xff;
    result[3] = (fcc >> 24) & 0xff;
    result[4] = 0;
}

static void
cam_v4l2_driver_init (CamV4L2Driver * self)
{
    dbg (DBG_DRIVER, "v4l2 driver constructor\n");
    CamUnitDriver * super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "input", "v4l2");
}

static void
cam_v4l2_driver_class_init (CamV4L2DriverClass * klass)
{
    dbg (DBG_DRIVER, "v4l2 driver class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = driver_finalize;
    klass->parent_class.create_unit = driver_create_unit;
    klass->parent_class.start = driver_start;
    klass->parent_class.stop = driver_stop;
}

static void
driver_finalize (GObject * obj)
{
    dbg (DBG_DRIVER, "v4l2 driver finalize\n");
//    CamV4L2Driver * self = CAM_V4L2_DRIVER (obj);

    G_OBJECT_CLASS (cam_v4l2_driver_parent_class)->finalize (obj);
}

static int
open_v4l2_device (int num, struct v4l2_capability * cap)
{
    char dev[64];
    struct stat st;
    int fd;

    sprintf (dev, "%s%d", V4L2_BASE, num);

    if (stat (dev, &st) < 0)
        return -1;
    if (!S_ISCHR (st.st_mode))
        return -1;

    fd = open (dev, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        return -1;

    memset (cap, 0, sizeof (struct v4l2_capability));
    dbg (DBG_INPUT, "v4l2 driver opened %s\n", dev);
    if ( (ioctl (fd, VIDIOC_QUERYCAP, cap)) < 0) {
        if (errno == EINVAL) {
            dbg (DBG_INPUT, "%s is not a V4L2 device (maybe V4L1?)\n", dev);
        } else {
            fprintf (stderr, "Error (%d): VIDIOC_QUERYCAP failed on %s: %s\n",
                    errno, dev, strerror (errno));
        }
        close (fd);
        return -1;
    }

    if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        close (fd);
        return -1;
    }

    return fd;
}

static int
driver_start (CamUnitDriver * super)
{
//    CamV4L2Driver * self = CAM_V4L2_DRIVER (super);

    int j;

    for (j = 0; j < 10; j++) {
        char name[256];
        char unit_id[256];
        int fd;
        struct v4l2_capability cap;
        memset (&cap, 0, sizeof(cap));

        fd = open_v4l2_device (j, &cap);
        if (fd < 0)
            continue;

        snprintf (name, sizeof (name), "%s (V4L2)", cap.card);
        snprintf (unit_id, sizeof (unit_id), "%d", j);

        CamUnitDescription *udesc = 
            cam_unit_driver_add_unit_description (super, name, unit_id, 
                CAM_UNIT_EVENT_METHOD_FD);

        g_object_set_data (G_OBJECT(udesc), 
                "v4l2-driver-index", GINT_TO_POINTER (j));

        close(fd);
    }

    return 0;
}

static int
driver_stop (CamUnitDriver * super)
{
//    CamV4L2Driver * self = CAM_V4L2_DRIVER (super);

    return CAM_UNIT_DRIVER_CLASS (cam_v4l2_driver_parent_class)->stop (super);
}

static CamUnit *
driver_create_unit (CamUnitDriver * super, const CamUnitDescription * udesc)
{
    dbg (DBG_DRIVER, "v4l2 driver creating new unit\n");
    g_assert (cam_unit_description_get_driver(udesc) == super);

    int ndx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(udesc), 
                "v4l2-driver-index"));
    char path[256];
    snprintf (path, sizeof(path), "%s%d", V4L2_BASE, ndx);
    return CAM_UNIT (cam_v4l2_new (path));
}

static void
cam_v4l2_init (CamV4L2 * self)
{
    dbg (DBG_INPUT, "v4l2 constructor\n");

    self->dev_path = NULL;
    self->fd = -1;
    self->buffers = NULL;
    self->num_buffers = 0;
    self->buffer_length = 0;
    self->buffers_outstanding = 0;
    self->use_try_fmt = 1;
}

static void v4l2_finalize (GObject * obj);
static int v4l2_stream_init (CamUnit * super, const CamUnitFormat * format);
static int v4l2_stream_shutdown (CamUnit * super);
static gboolean v4l2_try_produce_frame (CamUnit * super);
static int v4l2_get_fileno (CamUnit * super);
static gboolean v4l2_try_set_control(CamUnit *super,
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);
static int add_all_controls (CamUnit * super);

static void
cam_v4l2_class_init (CamV4L2Class * klass)
{
    dbg (DBG_INPUT, "v4l2 class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = v4l2_finalize;

    klass->parent_class.stream_init = v4l2_stream_init;
    klass->parent_class.stream_shutdown = v4l2_stream_shutdown;
    klass->parent_class.try_produce_frame = v4l2_try_produce_frame;
    klass->parent_class.get_fileno = v4l2_get_fileno;
    klass->parent_class.try_set_control = v4l2_try_set_control;
}

static void
v4l2_finalize (GObject * obj)
{
    dbg (DBG_INPUT, "v4l2 finalize\n");
    CamUnit * super = CAM_UNIT (obj);

    if (cam_unit_is_streaming(super)) {
        dbg (DBG_INPUT, "forcibly shutting down v4l2 unit\n");
        v4l2_stream_shutdown (super);
    }
    CamV4L2 * self = (CamV4L2*) (super);
    if (self->fd >= 0) {
        close (self->fd);
        self->fd = -1;
    }
    GList *output_formats = cam_unit_get_output_formats(super);
    for (GList *fiter=output_formats; fiter; fiter=fiter->next) {
        CamUnitFormat *outfmt = CAM_UNIT_FORMAT (fiter->data);
        free (g_object_get_data (G_OBJECT (outfmt), "input_v4l2:v4l2_format"));
    }
    g_list_free(output_formats);
    if(self->dev_path)
        g_free(self->dev_path);

    G_OBJECT_CLASS (cam_v4l2_parent_class)->finalize (obj);
}

static int
open_camera_device(CamV4L2 *self)
{
    if(self->fd >= 0)
        close(self->fd);
    self->fd = open(self->dev_path, O_RDWR | O_NONBLOCK, 0);
    return self->fd;
}

static int
add_v4l2_format (CamV4L2 * self, uint32_t width, uint32_t height,
        CamPixelFormat cam_pixelformat, uint32_t v4l2_pixelformat)
{
    struct v4l2_format *fmt = calloc (1, sizeof (struct v4l2_format));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt->fmt.pix.width = width;
    fmt->fmt.pix.height = height;
    fmt->fmt.pix.pixelformat = v4l2_pixelformat;
    fmt->fmt.pix.field = V4L2_FIELD_ANY;
    fmt->fmt.pix.bytesperline = 0;

    // test out the video format.
    if (0 && self->use_try_fmt && ioctl (self->fd, VIDIOC_TRY_FMT, fmt) < 0) {
        if(errno == EINVAL) {
            perror ("ioctl");
            fprintf (stderr, 
                    "Error: VIDIOC_TRY_FMT failed (%s %dx%d)\n",
                    cam_pixel_format_nickname(cam_pixelformat), width, height);
            return -1;
        } else if (errno != EBUSY) {
            // Sometimes (esp on UVC), VIDIOC_TRY_FMT fails but the video
            // format is fine.  If we get an error that's not EINVAL or EBUSY,
            // then stop using VIDIOC_TRY_FMT because it's probably just a 
            // waste of time.
            self->use_try_fmt = 0;

            dbg(DBG_INPUT, "disabling use of VIDIOC_TRY_FMT\n");
        }
    } 

    if (fmt->fmt.pix.height*fmt->fmt.pix.bytesperline > 
            fmt->fmt.pix.sizeimage) {
        dbg (DBG_INPUT, "WARNING: v4l2 driver is reporting bogus row stride\n");
        fmt->fmt.pix.bytesperline = 
            fmt->fmt.pix.width * cam_pixel_format_bpp (cam_pixelformat) / 8;
    }

    CamUnitFormat *new_fmt = cam_unit_add_output_format (CAM_UNIT (self),
            cam_pixelformat, NULL, fmt->fmt.pix.width, fmt->fmt.pix.height,
            fmt->fmt.pix.bytesperline);
            //, fmt->fmt.pix.sizeimage);

    g_object_set_data (G_OBJECT (new_fmt), "input_v4l2:v4l2_format", fmt);
    return 0;
}

static void
add_control (CamV4L2 *self, struct v4l2_queryctrl *queryctrl)
{
    CamUnit *super = CAM_UNIT (self);
    CamUnitControl *newctl = NULL;
#ifdef V4L2_CTRL_FLAG_READ_ONLY
    int enabled = ! ((queryctrl->flags & V4L2_CTRL_FLAG_DISABLED) ||
                     (queryctrl->flags & V4L2_CTRL_FLAG_READ_ONLY));
#else 
    int enabled = ! queryctrl->flags & V4L2_CTRL_FLAG_DISABLED;
#endif

    char ctl_id[80];
    switch (queryctrl->id) {
        case V4L2_CID_BRIGHTNESS: sprintf (ctl_id, "brightness"); break;
        case V4L2_CID_CONTRAST: sprintf (ctl_id, "contrast"); break;
        case V4L2_CID_SATURATION: sprintf (ctl_id, "saturation"); break;
        case V4L2_CID_HUE: sprintf (ctl_id, "hue"); break;
        case V4L2_CID_AUDIO_VOLUME: sprintf (ctl_id, "audio-volume"); break;
        case V4L2_CID_AUDIO_BALANCE: sprintf (ctl_id, "audio-balance"); break;
        case V4L2_CID_AUDIO_BASS: sprintf (ctl_id, "audio-bass"); break;
        case V4L2_CID_AUDIO_TREBLE: sprintf (ctl_id, "treble"); break;
        case V4L2_CID_AUDIO_MUTE: sprintf (ctl_id, "audio-mute"); break;
        case V4L2_CID_AUDIO_LOUDNESS: sprintf (ctl_id, "audio-loudness"); break;
        case V4L2_CID_BLACK_LEVEL: sprintf (ctl_id, "black-level"); break;
        case V4L2_CID_AUTO_WHITE_BALANCE: 
           sprintf (ctl_id, "auto-white-balance"); break;
        case V4L2_CID_DO_WHITE_BALANCE: 
           sprintf (ctl_id, "do-white-balance"); break;
        case V4L2_CID_RED_BALANCE: 
           sprintf (ctl_id, "white-balance-red"); break;
        case V4L2_CID_BLUE_BALANCE: 
           sprintf (ctl_id, "white-balance-blue"); break;
        case V4L2_CID_GAMMA: sprintf (ctl_id, "gamma"); break;
        case V4L2_CID_EXPOSURE: sprintf (ctl_id, "exposure"); break;
        case V4L2_CID_AUTOGAIN: sprintf (ctl_id, "auto-gain"); break;
        case V4L2_CID_GAIN: sprintf (ctl_id, "gain"); break;
        case V4L2_CID_HFLIP: sprintf (ctl_id, "h-flip"); break;
        case V4L2_CID_VFLIP: sprintf (ctl_id, "v-flip"); break;
        case V4L2_CID_HCENTER: sprintf (ctl_id, "h-center"); break;
        case V4L2_CID_VCENTER: sprintf (ctl_id, "v-center"); break;
        default:
           snprintf (ctl_id, sizeof (ctl_id), "control-%d", queryctrl->id);
           break;
    }

    switch (queryctrl->type) {
        case V4L2_CTRL_TYPE_INTEGER:
            newctl = cam_unit_add_control_int (super, 
                    ctl_id, (char*) queryctrl->name, 
                    queryctrl->minimum, queryctrl->maximum, queryctrl->step,
                    queryctrl->default_value, enabled);
            break;
        case V4L2_CTRL_TYPE_BOOLEAN:
            newctl = cam_unit_add_control_boolean (super,
                    ctl_id, (char*) queryctrl->name,
                    queryctrl->default_value, enabled);
            break;
        case V4L2_CTRL_TYPE_MENU:
            {
                int noptions = queryctrl->maximum - queryctrl->minimum + 1;
                CamUnitControlEnumValue entries[noptions + 1];
                memset(entries, 0, 
                        (noptions+1)*sizeof(CamUnitControlEnumValue));
//                char **entries = (char**) calloc (noptions + 1, sizeof(char*));
//                int *entries_enabled = calloc (1, noptions * sizeof (int));

                struct v4l2_querymenu querymenu;
                memset (&querymenu, 0, sizeof (querymenu));
                querymenu.id = queryctrl->id;

                int i=0;
                for (querymenu.index = queryctrl->minimum;
                        querymenu.index <= queryctrl->maximum;
                        querymenu.index++) {
                    if (0 == ioctl (self->fd, VIDIOC_QUERYMENU, &querymenu)) {
                        entries[i].nickname = strdup ((char*)querymenu.name);
                        entries[i].value = i;
                        entries[i].enabled = 1;
                    } else {
                        perror ("VIDIOC_QUERYMENU");
                        break;
                    }
                    i++;
                }
                newctl = cam_unit_add_control_enum (super,
                        ctl_id, (char*) queryctrl->name, 
                        queryctrl->default_value,
                        enabled, entries);
                for(i=0; i<noptions; i++) {
                    free((char*)entries[i].nickname);
                }
            }
            break;
        case V4L2_CTRL_TYPE_BUTTON:
            newctl = cam_unit_add_control_boolean (super, 
                    ctl_id, (char*) queryctrl->name, 0, 1);
            cam_unit_control_set_ui_hints (newctl, CAM_UNIT_CONTROL_ONE_SHOT);
            break;
#ifdef V4L2_CTRL_TYPE_INTEGER64
        case V4L2_CTRL_TYPE_INTEGER64:
            err ("WARNING: unsupported int64 control (%s)\n",
                    queryctrl->name);
            break;
#endif
        default:
            break;
    }

    if (newctl) {
        g_object_set_data (G_OBJECT (newctl), "input_v4l2:queryctrl_id", 
                GINT_TO_POINTER (queryctrl->id));
    }
}

CamV4L2 *
cam_v4l2_new (const char *path)
{
    CamV4L2 * self = (CamV4L2*) (g_object_new (cam_v4l2_get_type(), NULL));
    self->dev_path = g_strdup(path);

    if(open_camera_device(self) < 0) 
        goto fail;

    struct v4l2_cropcap cropcap;
    memset (&cropcap, 0, sizeof (cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (self->fd, VIDIOC_CROPCAP, &cropcap) != 0) {
        fprintf (stderr, "Warning: %s does not support VIDIOC_CROPCAP\n", path);
    }

    struct v4l2_fmtdesc f;
    memset (&f, 0, sizeof (f));
    f.index = 0;
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int oldfindex = f.index;

    while (ioctl (self->fd, VIDIOC_ENUM_FMT, &f) == 0) {
        /* With some Logitech Quickcams the only way we know there are no more
         * formats is that the index gets modified under us. */
        if (f.index != oldfindex && oldfindex != 0)
            break;

        CamPixelFormat cam_pixelformat = f.pixelformat;
        if (f.pixelformat == 0x32435750) { // 'PWC2'
            cam_pixelformat = CAM_PIXEL_FORMAT_I420;
        }

        // HACK.  This seems to improve stability.
        open_camera_device(self);

        int can_enum_frames = 0;
#ifdef VIDIOC_ENUM_FRAMESIZES
        struct v4l2_frmsizeenum framesize;
        memset (&framesize, 0, sizeof(framesize));
        framesize.index = 0;
        framesize.pixel_format = f.pixelformat;
        while (ioctl (self->fd, VIDIOC_ENUM_FRAMESIZES, &framesize) == 0) {
            int width, height;
            can_enum_frames = 1;

            if (framesize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                    framesize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                width = framesize.stepwise.max_width;
                height = framesize.stepwise.max_height;
            } else {
                width = framesize.discrete.width;
                height = framesize.discrete.height;
            }

            add_v4l2_format (self, width, height, cam_pixelformat, 
                    f.pixelformat);

            framesize.index++;
        }
        char fourcc_str[5];
        fourcc_to_str (f.pixelformat, fourcc_str);
        dbg (DBG_INPUT, "v4l2: enumerated %d frame sizes for format %s\n", 
                framesize.index, fourcc_str);
#endif
        if (!can_enum_frames) {
            /* Just add a big format.  The try-set will automatically
             * determine the real resolution. */
            add_v4l2_format (self, 2000, 2000, cam_pixelformat, f.pixelformat);
        }
        
        f.index++;
        oldfindex = f.index;
    }
    dbg (DBG_INPUT, "v4l2: enumerated %d formats\n", f.index);

    struct v4l2_format curfmt;
    memset (&curfmt, 0, sizeof (curfmt));
    curfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (self->fd, VIDIOC_G_FMT, &curfmt) == 0) {
        dbg (DBG_INPUT, "v4l2: current format: %dx%d\n", 
                curfmt.fmt.pix.width, 
                curfmt.fmt.pix.height);
        if (ioctl (self->fd, VIDIOC_S_FMT, &curfmt) < 0) {
            err("V4L2:  Unable to set output format!\n");
        }
    }
    add_all_controls (CAM_UNIT (self));

    return self;
fail:
    g_object_unref (G_OBJECT (self));
    return NULL;
}

static void
_unmap_buffers (CamV4L2 *self)
{
    if (!self->buffers) return;

    int i;
    for (i = 0; i < self->num_buffers; i++) {
        munmap (self->buffers[i], self->buffer_length);
    }
    free (self->buffers);
    self->buffers = NULL;
}

static int
v4l2_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamV4L2 * self = (CamV4L2*) (super);
    dbg (DBG_INPUT, "Initializing v4l2 stream (pxlfmt %s %dx%d)\n",
            cam_pixel_format_nickname (format->pixelformat), 
            format->width, format->height);

    struct v4l2_format *fmt = g_object_get_data (G_OBJECT (format),
            "input_v4l2:v4l2_format");
    if (-1 == ioctl (self->fd, VIDIOC_S_FMT, fmt)) {
        perror ("VIDIOC_S_FMT");
        fprintf (stderr, "Error: VIDIOC_S_FMT failed\n");
        return -1;
    }

    // request kernel buffers
    struct v4l2_requestbuffers reqbuf;
    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.count = NUM_BUFFERS;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if ( -1 == ioctl (self->fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL) {
            err ("v4l2: mmap-streaming not supported\n");
        } else {
            perror ("VIDIOC_REQBUFS");
        }
        return -1;
    }

    self->num_buffers = reqbuf.count;
    self->buffers = malloc (self->num_buffers * sizeof (uint8_t *));

    // mmap buffers
    int i;
    for (i=0; i<self->num_buffers; i++) {
        struct v4l2_buffer buffer;
        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (-1 == ioctl (self->fd, VIDIOC_QUERYBUF, &buffer)) {
            perror ("VIDIOC_QUERYBUF");
            break;
        }

        self->buffers[i] = mmap (NULL, buffer.length,
                PROT_READ | PROT_WRITE, MAP_SHARED,
                self->fd, buffer.m.offset);
        dbg (DBG_INPUT, "v4l2 mapped %p (%d bytes)\n",
                self->buffers[i], buffer.length);

        if (-1 == ioctl (self->fd, VIDIOC_QBUF, &buffer)) {
            perror ("VIDIOC_QBUF");
            break;
        }

        self->buffer_length = buffer.length;
    }

    if (i<self->num_buffers) {
        for (int j=0; j<=i; j++)
            munmap (self->buffers[i], self->buffer_length);
        return -1;
    }

    self->buffers_outstanding = 0;

#if 0
    // special case for MJPEG
    if (format->pixelformat == CAM_PIXEL_FORMAT_MJPEG) {
        struct v4l2_jpegcompression jopt;
        memset (&jopt, 0, sizeof (jopt));
        if (0 == ioctl (self->fd, VIDIOC_G_JPEGCOMP, &jopt)) {
            jopt.jpeg_markers = V4L2_JPEG_MARKER_DHT |
                V4L2_JPEG_MARKER_DQT | 
                V4L2_JPEG_MARKER_DRI | 
                V4L2_JPEG_MARKER_COM | 
                V4L2_JPEG_MARKER_APP;
            if (-1 == ioctl (self->fd, VIDIOC_S_JPEGCOMP, &jopt)) {
                perror ("VIDIOC_S_JPEGCOMP");
            }
        } else {
            perror ("VIDIOC_G_JPEGCOMP");
        }
    }
#endif

    dbg (DBG_INPUT, "v4l2 mapped %d buffers of size %d\n", 
            self->num_buffers, self->buffer_length);

    int streamontype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl (self->fd, VIDIOC_STREAMON, &streamontype)) {
        perror ("VIDIOC_STREAMON");
        err ("v4l2: couldn't start streaming images\n");
        _unmap_buffers (self);
        return -1;
    }

    return 0;
}

static int
v4l2_stream_shutdown (CamUnit * super)
{
    CamV4L2 * self = (CamV4L2*) (super);

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl (self->fd, VIDIOC_STREAMOFF, &type)) {
        perror ("VIDIOC_STREAMOFF");
        err ("v4l2: couldn't stop streaming images\n");
        return -1;
    }

    _unmap_buffers (self);

    // release requested buffers
    struct v4l2_requestbuffers reqbuf;
    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.count = 0;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl (self->fd, VIDIOC_REQBUFS, &reqbuf)) {
        fprintf (stderr, "Warning: v4l2 driver does not handle REQBUFS "
                "for cleanup\n");
    }

    return 0;
}

static gboolean
v4l2_try_produce_frame (CamUnit * super)
{
    CamV4L2 * self = (CamV4L2*) (super);
    const CamUnitFormat * outfmt = cam_unit_get_output_format (super);

    /* If all buffers are already dequeued, V4L2 will keep waking us up
     * because it puts an error condition on its file descriptor.  Thus,
     * we bide our time and sleep a bit so we don't hose the CPU. */
    if (self->buffers_outstanding == self->num_buffers) {
        struct timespec st = { 0, 1000000 };
        nanosleep (&st, NULL);
        return FALSE;
    }

    struct v4l2_buffer buf;
    memset (&buf, 0, sizeof (buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl (self->fd, VIDIOC_DQBUF, &buf)) {
        fprintf (stderr, "Warning: DQBUF ioctl failed: %s\n", strerror (errno));

        /* Restart the stream from scratch */
        v4l2_stream_shutdown (super);
        v4l2_stream_init (super, outfmt);
        return FALSE;
    }

    // TODO don't malloc
    CamFrameBuffer * fbuf = cam_framebuffer_new (self->buffers[buf.index],
            self->buffer_length);
    fbuf->timestamp = buf.timestamp.tv_sec * 1000000 + buf.timestamp.tv_usec;
//    fbuf->bus_timestamp = buf.sequence;
    fbuf->bytesused = buf.bytesused;
    cam_unit_produce_frame (super, fbuf, outfmt);
    g_object_unref (fbuf);

    // release v4l2 mmap buffer
    if (-1 == ioctl (self->fd, VIDIOC_QBUF, &buf)) {
        fprintf (stderr, "Error: QBUF ioctl failed: %s\n", strerror (errno));
    }
    return TRUE;
}

static int
v4l2_get_fileno (CamUnit *super)
{
    return ((CamV4L2*) super)->fd;
}

#define MAX_INPUTS 32
#define MAX_STANDARDS 64
#define MAX_TUNERS 4

enum {
    CAM_V4L2_CNTL_INPUT=0,
    CAM_V4L2_CNTL_STD=1,
    CAM_V4L2_CNTL_TUNER0=2,
    CAM_V4L2_CNTL_TUNER1=3,
    CAM_V4L2_CNTL_TUNER2=4,
    CAM_V4L2_CNTL_TUNER3=5,
};

static int
update_video_standards (CamUnit * super, int modify)
{
    CamV4L2 * self = (CamV4L2*) (super);

    v4l2_std_id stdid = 0;
    ioctl (self->fd, VIDIOC_G_STD, &stdid);

    v4l2_std_id * stds = malloc (MAX_INPUTS * sizeof (v4l2_std_id));
    struct v4l2_standard std;
    CamUnitControlEnumValue entries[MAX_INPUTS + 1];
    memset(entries, 0, sizeof(entries));
//    char * std_descs[MAX_INPUTS];
    int i = 0;
    int cur_val = 0;
    std.index = i;
    while (ioctl (self->fd, VIDIOC_ENUMSTD, &std) == 0 && i < MAX_STANDARDS) {
        stds[i] = std.id;
        entries[i].nickname = strdup ((char *) std.name);
        entries[i].value = 1;
        entries[i].enabled = 1;
        if (std.id & stdid)
            cur_val = i;
        std.index = ++i;
    }
    if (i == 0) {
        cam_unit_control_set_enabled (self->standard_ctl, FALSE);
        return 0;
    }
    g_object_set_data (G_OBJECT (self->standard_ctl), "v4l2-stds", stds);
    if (modify)
        cam_unit_control_modify_enum (self->standard_ctl, cur_val, 1, entries);
    for(int j=0; j<i; j++) {
        free((char*)entries[j].nickname);
    }

    return 0;
}

static void
add_user_controls (CamV4L2 *self)
{
    struct v4l2_queryctrl queryctrl;

    // add all controls?
    memset(&queryctrl, 0, sizeof(queryctrl));
    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    int nctrl = 0;
    while (0 == ioctl (self->fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        nctrl ++;
        dbg (DBG_INPUT, "Control %s\n", queryctrl.name);

        if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
            queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }

        add_control (self, &queryctrl);
        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    // if the V4L2 driver does not support the V4L2_CTRL_FLAG_NEXT_CTRL
    // method of enumerating controls, then try enumerating them 
    // explicitly here.
    if(0 == nctrl && errno == EINVAL) {
        uint32_t id;
        for (id = V4L2_CID_BASE; id < V4L2_CID_LASTP1; id++) {
            memset (&queryctrl, 0, sizeof (queryctrl));
            queryctrl.id = id;
            if (0 == ioctl (self->fd, VIDIOC_QUERYCTRL, &queryctrl)) {
                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                    continue;

                dbg (DBG_INPUT, "Control %s\n", queryctrl.name);
                add_control (self, &queryctrl);
            } else {
                if (errno == EINVAL)
                    continue;
                perror ("VIDIOC_QUERYCTRL");
            }
        }

        for (id = V4L2_CID_PRIVATE_BASE; id<V4L2_CID_PRIVATE_BASE + 100; id++) {
            memset (&queryctrl, 0, sizeof (queryctrl));
            queryctrl.id = id;
            if (0 == ioctl (self->fd, VIDIOC_QUERYCTRL, &queryctrl)) {
                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                    continue;

                dbg (DBG_INPUT, "Private Control %s\n", queryctrl.name);
                add_control (self, &queryctrl);

            } else {
                if (errno == EINVAL)
                    break;
                perror ("VIDIOC_QUERYCTRL");
            }
        }
    }
}

static int
add_all_controls (CamUnit * super)
{
    CamV4L2 * self = (CamV4L2*) (super);
    int cur_val;

    struct v4l2_input input;
    CamUnitControlEnumValue input_entries[MAX_INPUTS + 1];
    memset(input_entries, 0, sizeof(input_entries));
    int i = 0;
    input.index = i;
    while (ioctl (self->fd, VIDIOC_ENUMINPUT, &input) == 0 &&
            i < MAX_INPUTS) {
        input_entries[i].nickname = strdup ((char *)input.name);
        input_entries[i].value = i;
        input_entries[i].enabled = 1;
        input.index = ++i;
    }
    if (i > 0 && ioctl (self->fd, VIDIOC_G_INPUT, &cur_val) == 0) {
        cam_unit_add_control_enum (super, "input", "Input", 
                cur_val, 1, input_entries);
    }
    int j;
    for (j = 0; j < i; j++) {
        free ((char*)input_entries[j].nickname);
    }

//    const char * stds[] = { NULL };
    CamUnitControlEnumValue standard_entries[] = {
        { 0, "NULL", 0 },
        { 0, NULL, 0 }
    };
    self->standard_ctl = cam_unit_add_control_enum (super, 
            "standard", "Standard", 0, 0, standard_entries);
    update_video_standards (super, 1);


    struct v4l2_tuner tuner;
    memset (&tuner, 0, sizeof (struct v4l2_tuner));
    i = 0;
    tuner.index = i;
    while (ioctl (self->fd, VIDIOC_G_TUNER, &tuner) == 0 &&
            i < MAX_TUNERS) {
        struct v4l2_frequency freq;
        freq.tuner = i;
        freq.type = tuner.type;
        if (tuner.rangehigh > 16000)
            tuner.rangehigh = 16000;
        if (ioctl (self->fd, VIDIOC_G_FREQUENCY, &freq) == 0) {
            char ctl_id[80];
            snprintf (ctl_id, sizeof (ctl_id), "tuner-%d", i);
            CamUnitControl * ctl = cam_unit_add_control_int (super,
                    ctl_id, (char *) tuner.name, 
                    tuner.rangelow, tuner.rangehigh, 1,
                    freq.frequency, 1);
            cam_unit_control_set_ui_hints (ctl, CAM_UNIT_CONTROL_SPINBUTTON);

            // set the tuner id, but add one so that we never set NULL
            g_object_set_data (G_OBJECT (ctl), "input_v4l2:tuner-id",
                    GINT_TO_POINTER (i + 1));
        }
        else {
            fprintf (stderr, "Warning: Can't get freq for V4L2 tuner %d\n", i);
        }
        tuner.index = ++i;
    }

    add_user_controls (self);

    return 0;
}

static gboolean
v4l2_try_set_control(CamUnit *super, const CamUnitControl *ctl,
        const GValue *proposed, GValue *actual) {
    CamV4L2 * self = (CamV4L2*) (super);
    const char *ctl_id = cam_unit_control_get_id(ctl);

    if (!strcmp (ctl_id, "input")) {
        int val = g_value_get_int (proposed);
        if (ioctl (self->fd, VIDIOC_S_INPUT, &val) < 0) {
            fprintf (stderr, "VIDIOC_S_INPUT failed: %s\n", strerror (errno));
            return FALSE;
        }
        g_value_set_int (actual, val);
        update_video_standards (super, 1);
        return TRUE;
    }
    if (ctl == self->standard_ctl) {
        int val = g_value_get_int (proposed);
        v4l2_std_id * stds = g_object_get_data (G_OBJECT (ctl),
                "v4l2-stds");
        if (!stds) return FALSE;
        v4l2_std_id std = stds[val];
        if (ioctl (self->fd, VIDIOC_S_STD, &std) < 0) {
            fprintf (stderr, "VIDIOC_S_STD failed: %s\n", strerror (errno));
            return FALSE;
        }
        g_value_set_int (actual, val);
        return TRUE;
    }
    if (!strncmp (ctl_id, "tuner-", strlen ("tuner-"))) {
        int tuner_id = GPOINTER_TO_INT (
            g_object_get_data (G_OBJECT (ctl), "input_v4l2:tuner-id")) - 1;
        if (tuner_id >= 0) {
            int val = g_value_get_int (proposed);
            struct v4l2_frequency freq;
            memset (&freq, 0, sizeof (struct v4l2_frequency));
            freq.tuner = tuner_id;
            freq.type = V4L2_TUNER_ANALOG_TV;
            freq.frequency = val;
            if (ioctl (self->fd, VIDIOC_S_FREQUENCY, &freq) < 0) {
                fprintf (stderr, "VIDIOC_S_FREQUENCY failed: %s\n", 
                        strerror (errno));
                return FALSE;
            }
            g_value_set_int (actual, val);
            return TRUE;
        }
    }

    // user control
    CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
    struct v4l2_control vctl;
    memset (&vctl, 0, sizeof (vctl));

    vctl.id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (ctl), 
                "input_v4l2:queryctrl_id"));
    switch (ctl_type) {
        case CAM_UNIT_CONTROL_TYPE_INT:
        case CAM_UNIT_CONTROL_TYPE_ENUM:
            vctl.value = g_value_get_int (proposed);
            break;
        case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
            vctl.value = g_value_get_boolean (proposed);
            break;
        default:
            err ("wtf???? %s:%d\n", __FILE__, __LINE__);
            break;
    }
    if (-1 == ioctl (self->fd, VIDIOC_S_CTRL, &vctl)) {
        dbg (DBG_INPUT, "V4L2 couldn't set control [%s] - %s\n", 
                cam_unit_control_get_name(ctl), strerror (errno));
        return FALSE;
    }
    
    // if the control is a push button, then ignore the value and return
    if (cam_unit_control_get_ui_hints (ctl) & CAM_UNIT_CONTROL_ONE_SHOT) {
        return FALSE;
    }

    // read back the actual value of the control
    if (-1 == ioctl (self->fd, VIDIOC_G_CTRL, &vctl)) {
        // readback failed.  just assume that the setting was successful and
        // return
        perror ("VIDIOC_G_CTRL");
        g_value_copy (proposed, actual);
        return TRUE;
    }

    switch (ctl_type) {
        case CAM_UNIT_CONTROL_TYPE_INT:
        case CAM_UNIT_CONTROL_TYPE_ENUM:
            g_value_set_int (actual, vctl.value);
            break;
        case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
            g_value_set_boolean (actual, vctl.value);
            break;
        default:
            err ("wtf???? %s:%d\n", __FILE__, __LINE__);
            break;
    }
    return TRUE;
}
