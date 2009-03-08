#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <linux/videodev.h>
#include <errno.h>

#include <camunits/dbg.h>
#include <camunits/pixels.h>
#include <camunits/plugin.h>
#include <camunits/unit.h>
#include <camunits/unit_driver.h>

#include "pwc-ioctl.h"

#define err(args...) fprintf(stderr, args)

#define V4L_BASE   "/dev/video"

typedef struct {
    CamUnitDriver parent;
} CamV4LDriver;

typedef struct {
    CamUnitDriverClass parent_class;
} CamV4LDriverClass;

typedef struct {
    CamUnit parent;

    CamUnitControl *brightness_ctl;
    CamUnitControl *hue_ctl;
    CamUnitControl *color_ctl;
    CamUnitControl *contrast_ctl;
    CamUnitControl *whiteness_ctl;
    GList *tuner_ctls;
    int fd;
    
    int is_pwc;
    CamUnitControl *pwc_wb_mode_ctl;
    CamUnitControl *pwc_wb_manual_red_ctl;
    CamUnitControl *pwc_wb_manual_blue_ctl;
} CamV4L;

typedef struct {
    CamUnitClass parent_class;
} CamV4LClass;

GType cam_v4l_driver_get_type (void);
GType cam_v4l_get_type (void);

static CamV4L * cam_v4l_new (int videonum);

//G_DEFINE_TYPE (CamV4LDriver, cam_v4l_driver, CAM_TYPE_UNIT_DRIVER);
CAM_PLUGIN_TYPE (CamV4LDriver, cam_v4l_driver, CAM_TYPE_UNIT_DRIVER);
CAM_PLUGIN_TYPE (CamV4L, cam_v4l, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    cam_v4l_driver_register_type (module);
    cam_v4l_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return CAM_UNIT_DRIVER (g_object_new (cam_v4l_driver_get_type(), NULL));
}

static inline int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) (((int64_t) tv.tv_sec) * 1000000 + tv.tv_usec);
}

static int
open_v4l_device (int num, struct video_capability * cap)
{
    char dev[64];
    struct stat st;
    int fd;

    sprintf (dev, "%s%d", V4L_BASE, num);

    if (stat (dev, &st) < 0)
        return -1;
    if (!S_ISCHR (st.st_mode))
        return -1;

    fd = open (dev, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        return -1;

    memset (cap, 0, sizeof (struct video_capability));
    dbg (DBG_INPUT, "v4l driver opened %s\n", dev);
    if (ioctl (fd, VIDIOCGCAP, cap) < 0) {
        if (errno == EINVAL) {
            dbg (DBG_INPUT, "%s is not a V4L device (maybe V4L2?)\n", dev);
        } else {
            fprintf (stderr, "Error: VIDIOC_QUERYCAP failed on %s: %s\n",
                    dev, strerror (errno));
        }
        close (fd);
        return -1;
    }

    if (!(cap->type & VID_TYPE_CAPTURE)) {
        close (fd);
        return -1;
    }

    return fd;
}

static CamPixelFormat 
videodev_palette_to_pixelformat(int palette)
{
    switch (palette) {
        case VIDEO_PALETTE_GREY:
            return CAM_PIXEL_FORMAT_GRAY;
        case VIDEO_PALETTE_RGB24:
            return CAM_PIXEL_FORMAT_RGB;
        case VIDEO_PALETTE_RGB32:
            return CAM_PIXEL_FORMAT_RGBA;
        case VIDEO_PALETTE_YUYV:
            return CAM_PIXEL_FORMAT_YUYV;
        case VIDEO_PALETTE_UYVY:
            return CAM_PIXEL_FORMAT_UYVY;
        case VIDEO_PALETTE_YUV411P:
            return CAM_PIXEL_FORMAT_YUV411P;
        case VIDEO_PALETTE_YUV420P:
            return CAM_PIXEL_FORMAT_I420;

        case VIDEO_PALETTE_YUV422:
        case VIDEO_PALETTE_YUV420:
        case VIDEO_PALETTE_YUV411:
        case VIDEO_PALETTE_RAW:
        case VIDEO_PALETTE_YUV422P:
        case VIDEO_PALETTE_RGB565:
        case VIDEO_PALETTE_YUV410P:
        case VIDEO_PALETTE_RGB555:
        case VIDEO_PALETTE_HI240:
        default:
            return CAM_PIXEL_FORMAT_INVALID;
    }

}

static CamUnit * driver_create_unit (CamUnitDriver * super,
        const CamUnitDescription * udesc);
static int driver_start (CamUnitDriver * super);

static void
cam_v4l_driver_init (CamV4LDriver * self)
{
    dbg (DBG_DRIVER, "v4l driver constructor\n");
    CamUnitDriver * super = CAM_UNIT_DRIVER (self);
    cam_unit_driver_set_name (super, "input", "v4l");
}

static void
cam_v4l_driver_class_init (CamV4LDriverClass * klass)
{
    dbg (DBG_DRIVER, "v4l driver class initializer\n");
    klass->parent_class.create_unit = driver_create_unit;
    klass->parent_class.start = driver_start;
}

static int
driver_start (CamUnitDriver * super)
{
    int j;

    for (j = 0; j < 10; j++) {
        char name[256];
        char unit_id[256];
        int fd;
        struct video_capability cap;

        fd = open_v4l_device (j, &cap);
        if (fd < 0)
            continue;

        snprintf (name, sizeof (name), "%s (V4L)", cap.name);
        snprintf (unit_id, sizeof (unit_id), "%d", j);

        CamUnitDescription *udesc = 
            cam_unit_driver_add_unit_description (super, name, unit_id, 
                CAM_UNIT_EVENT_METHOD_FD);

        g_object_set_data (G_OBJECT(udesc), 
                "v4l-driver-index", GINT_TO_POINTER (j));

        close(fd);
    }

    return 0;
}

static CamUnit *
driver_create_unit (CamUnitDriver * super, const CamUnitDescription * udesc)
{
    dbg (DBG_DRIVER, "v4l driver creating new unit\n");
    g_assert (cam_unit_description_get_driver(udesc) == super);

    int ndx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(udesc), 
                "v4l-driver-index"));
    return CAM_UNIT (cam_v4l_new (ndx));
}

static void
cam_v4l_init (CamV4L * self)
{
    dbg (DBG_INPUT, "v4l constructor\n");

    self->fd = -1;
    self->brightness_ctl = NULL;
    self->hue_ctl = NULL;
    self->color_ctl = NULL;
    self->whiteness_ctl = NULL;
    self->contrast_ctl = NULL;

    self->tuner_ctls = NULL;

    self->is_pwc = 0;
    self->pwc_wb_mode_ctl = NULL;
    self->pwc_wb_manual_red_ctl = NULL;
    self->pwc_wb_manual_blue_ctl = NULL;
}

static void v4l_finalize (GObject * obj);
static int v4l_stream_init (CamUnit * super, const CamUnitFormat * format);
static gboolean v4l_try_produce_frame (CamUnit * super);
static int v4l_get_fileno (CamUnit * super);
static gboolean v4l_try_set_control(CamUnit *super,
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual);
static void check_for_pwc (CamV4L *self);

static void
cam_v4l_class_init (CamV4LClass * klass)
{
    dbg (DBG_INPUT, "v4l class initializer\n");
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = v4l_finalize;

    klass->parent_class.stream_init = v4l_stream_init;
    klass->parent_class.try_produce_frame = v4l_try_produce_frame;
    klass->parent_class.get_fileno = v4l_get_fileno;
    klass->parent_class.try_set_control = v4l_try_set_control;
}

static void
v4l_finalize (GObject * obj)
{
    dbg (DBG_INPUT, "v4l finalize\n");
    CamUnit * super = CAM_UNIT (obj);

    CamV4L * self = (CamV4L*) (super);
    if (self->fd >= 0) {
        close (self->fd);
        self->fd = -1;
    }

    G_OBJECT_CLASS (cam_v4l_parent_class)->finalize (obj);
}

CamV4L *
cam_v4l_new (int videonum)
{
    CamV4L * self = (CamV4L*) (g_object_new (cam_v4l_get_type(), NULL));
    CamUnit *super = CAM_UNIT (self);
    int status;

    struct video_capability vcap;
    self->fd = open_v4l_device (videonum, &vcap);
    if (self->fd < 0) {
        perror ("open_v4l_device");
        goto fail;
    }

    struct video_picture vpic;
    memset (&vpic, 0, sizeof (vpic));
    status = ioctl (self->fd, VIDIOCGPICT, &vpic);
    if (status < 0) {
        err ("V4L: ioctl VIDIOCGPICT failed on /dev/video%d\n", videonum);
        goto fail;
    }

    dbg (DBG_INPUT, "V4L: %d %d - %d %d\n", vcap.minwidth, vcap.minheight,
            vcap.maxwidth, vcap.maxheight);

    CamPixelFormat pfmt = videodev_palette_to_pixelformat (vpic.palette);
    dbg (DBG_INPUT, "V4L: palette %d (%s)\n", vpic.palette,
            cam_pixel_format_nickname(pfmt));

    if (pfmt != CAM_PIXEL_FORMAT_INVALID) {
        int npixels = vcap.minwidth * vcap.minheight;
        cam_unit_add_output_format (CAM_UNIT (self), pfmt, 
                NULL, vcap.minwidth, vcap.minheight, vcap.minwidth);

        if (vcap.minwidth != vcap.maxwidth || 
            vcap.minheight != vcap.maxheight) {
            npixels = vcap.maxwidth * vcap.maxheight;
            cam_unit_add_output_format (CAM_UNIT (self), pfmt, 
                    NULL, vcap.maxwidth, vcap.maxheight, vcap.maxwidth);
        }
    }

    // add controls for brightness, hue, color, contrast, and whiteness
    // some may not be supported, but V4L doesn't provide the ability to detect
    // support.
    self->brightness_ctl = cam_unit_add_control_int (super, 
        "brightness", "Brightness", 0, 65535, 1,
        vpic.brightness, 1);
    self->hue_ctl = cam_unit_add_control_int (super, 
        "hue", "Hue", 0, 65535, 1,
        vpic.hue, 1);
    self->color_ctl = cam_unit_add_control_int (super, 
        "color", "Color", 0, 65535, 1,
        vpic.colour, 1);
    self->contrast_ctl = cam_unit_add_control_int (super, 
        "contrast", "Contrast", 0, 65535, 1,
        vpic.contrast, 1);
    self->whiteness_ctl = cam_unit_add_control_int (super, 
        "whiteness", "Whiteness", 0, 65535, 1,
        vpic.whiteness, 1);

    // if the device has a tuner, then add a channel control
    struct video_tuner vtune;
    memset (&vtune, 0, sizeof (vtune));
    vtune.tuner = 0;
    while (ioctl (self->fd, VIDIOCGTUNER, &vtune) == 0) {
        if (ioctl (self->fd, VIDIOCSTUNER, &vtune.tuner) < 0) {
            err("Warning: detected tuner %s, but could not activate\n",
                    vtune.name);
            vtune.tuner++;
            continue;
        }

        unsigned long freq = 0;
        if (ioctl (self->fd, VIDIOCGFREQ, &freq) < 0) {
            err ("Warning: detected tuner %s, but couldn't get current "
                    "frequency\n", vtune.name);
            vtune.tuner++;
            continue;
        }

        char ctl_id[40];
        snprintf (ctl_id, sizeof (ctl_id), "tuner-%d", vtune.tuner);
        CamUnitControl *ctl = cam_unit_add_control_int (super,
                ctl_id,
                vtune.name, vtune.rangelow, vtune.rangehigh, 
                1, freq, 1);

        self->tuner_ctls = g_list_append (self->tuner_ctls, ctl);

        g_object_set_data (G_OBJECT(ctl), "ctl-tuner-number",
                GINT_TO_POINTER (vtune.tuner));

        cam_unit_control_set_ui_hints (ctl, CAM_UNIT_CONTROL_SPINBUTTON);

        vtune.tuner++;
    }

    check_for_pwc (self);

    return self;
fail:
    g_object_unref (G_OBJECT (self));
    return NULL;
}

static void
check_for_pwc (CamV4L *self)
{
    CamUnit *super = CAM_UNIT (self);
    struct video_capability vcap;
    memset (&vcap, 0, sizeof (struct video_capability));
    ioctl (self->fd, VIDIOCGCAP, &vcap);

    struct pwc_probe probe;
    memset (&probe, 0, sizeof (probe));
    if (ioctl(self->fd, VIDIOCPWCPROBE, &probe) == 0) {
        if (! strcmp(vcap.name, probe.name)) {
            dbg (DBG_INPUT, "Detected a pwc camera (%s)\n", probe.name);

            self->is_pwc = 1;
            struct pwc_whitebalance wb;
            memset (&wb, 0, sizeof (wb));
            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) == 0) {
                dbg (DBG_INPUT, "adding white balance controls\n");

                CamUnitControlEnumValue wbmodes[] = { 
                    { PWC_WB_INDOOR,  "Indoor", 1 },
                    { PWC_WB_OUTDOOR, "Outdoor", 1 },
                    { PWC_WB_FL,      "Flourescent",1 },
                    { PWC_WB_MANUAL,  "Manual", 1 },
                    { PWC_WB_AUTO,    "Auto", 1 },
                    { 0, NULL, 1 },
                };
                self->pwc_wb_mode_ctl = cam_unit_add_control_enum (super,
                        "white-balance-mode", 
                        "White Balance", wb.mode, 1, wbmodes);
                int mwb = (wb.mode == PWC_WB_MANUAL);
                int red  = (mwb) ? wb.manual_red  : wb.read_red;
                int blue = (mwb) ? wb.manual_blue : wb.read_blue;
                self->pwc_wb_manual_red_ctl = cam_unit_add_control_int (super,
                        "white-balance-red", "WB Red",
                        0, 65535, 1, red, mwb);
                self->pwc_wb_manual_blue_ctl = cam_unit_add_control_int (super,
                        "white-balance-blue", "WB Blue",
                        0, 65535, 1, blue, mwb);
            }
        }
    }
}

static int
v4l_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamV4L * self = (CamV4L*) (super);
    dbg (DBG_INPUT, "Initializing v4l stream (pxlfmt 0x%x %dx%d)\n",
            format->pixelformat, format->width, format->height);

    struct video_window vwin;
    memset (&vwin, 0, sizeof (vwin));
    if (ioctl (self->fd, VIDIOCGWIN, &vwin) < 0) {
        perror ("ioctl");
        err ("V4L: VIDIOCGWIN failed in stream_init\n");
        return -1;
    }
    dbg (DBG_INPUT, "v4l: original window <%d, %d> <%dx%d>\n", 
            vwin.x, vwin.y, vwin.width, vwin.height);

    vwin.x = 0;
    vwin.y = 0;
    vwin.width = format->width;
    vwin.height = format->height;
    if (ioctl (self->fd, VIDIOCSWIN, &vwin) < 0) {
        perror ("ioctl");
        err ("V4L: VIDIOCWIN failed in stream_init\n");
        return -1;
    }

    if (ioctl (self->fd, VIDIOCGWIN, &vwin) < 0) {
        perror ("ioctl");
        err ("V4L: VIDIOCGWIN failed in stream_init\n");
        return -1;
    }
    dbg (DBG_INPUT, "v4l: new window <%d, %d> <%dx%d>\n", 
            vwin.x, vwin.y, vwin.width, vwin.height);

    return 0;
}

static gboolean
v4l_try_produce_frame (CamUnit * super)
{
    CamV4L * self = (CamV4L*)super;
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    int buf_sz = outfmt->height * outfmt->row_stride;
    CamFrameBuffer *buf = cam_framebuffer_new_alloc (buf_sz);

    int status = read (self->fd, buf->data, buf->length);
    if (status <= 0) {
        perror ("read");
        g_object_unref (buf);
        return FALSE;
    } 

    buf->bytesused = status;
    buf->timestamp = _timestamp_now();

    cam_unit_produce_frame (super, buf, outfmt);
    g_object_unref (buf);
    return TRUE;
}

static int
v4l_get_fileno (CamUnit *super)
{
    CamV4L *self = (CamV4L*)super;
    return self->fd;
}

static gboolean 
v4l_try_set_control(CamUnit *super,
        const CamUnitControl *ctl, const GValue *proposed, GValue *actual)
{
    CamV4L * self = (CamV4L*) (super);

    // pwc (i.e. Logitech quickcam 4000 and older) controls
    if (self->is_pwc) {
        if (ctl == self->pwc_wb_mode_ctl) {
            // white balance mode (indoor, manual, auto, etc.)
            int mode = g_value_get_int (proposed);

            struct pwc_whitebalance wb;
            memset (&wb, 0, sizeof (wb));
            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) < 0) { return FALSE; }
            wb.mode = mode;
            if (ioctl (self->fd, VIDIOCPWCSAWB, &wb) < 0) { return FALSE; }

            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) == 0) {
                g_value_set_int (actual, wb.mode);
            } else {
                g_value_copy (proposed, actual);
            }

            int mwb = (wb.mode == PWC_WB_MANUAL);
            cam_unit_control_set_enabled (self->pwc_wb_manual_red_ctl, mwb);
            cam_unit_control_set_enabled (self->pwc_wb_manual_blue_ctl, mwb);

            return TRUE;
        } else if (ctl == self->pwc_wb_manual_red_ctl) {
            // manual white balance - red
            struct pwc_whitebalance wb;
            memset (&wb, 0, sizeof (wb));
            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) < 0) { return FALSE; }
            wb.manual_red = g_value_get_int (proposed);
            if (ioctl (self->fd, VIDIOCPWCSAWB, &wb) < 0) { return FALSE; }
            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) == 0) {
                g_value_set_int (actual, wb.manual_red);
            } else {
                g_value_copy (proposed, actual);
            }
            return TRUE;
        } else if (ctl == self->pwc_wb_manual_blue_ctl) {
            // manual white balance - blue
            struct pwc_whitebalance wb;
            memset (&wb, 0, sizeof (wb));
            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) < 0) { return FALSE; }
            wb.manual_blue = g_value_get_int (proposed);
            if (ioctl (self->fd, VIDIOCPWCSAWB, &wb) < 0) { return FALSE; }
            if (ioctl (self->fd, VIDIOCPWCGAWB, &wb) == 0) {
                g_value_set_int (actual, wb.manual_blue);
            } else {
                g_value_copy (proposed, actual);
            }
            return TRUE;
        }
    }

    // tv tuner?
    GList *tuner_link = g_list_find (self->tuner_ctls, ctl);
    if (tuner_link) {
        int tuner_number = 
            GPOINTER_TO_INT (g_object_get_data(G_OBJECT(ctl), 
                        "ctl-tuner-number"));

        if (ioctl (self->fd, VIDIOCSTUNER, &tuner_number) < 0) {
            perror ("ioctl");
            err("Warning: could not activate tuner\n");
            return FALSE;
        }

        unsigned long freq = g_value_get_int (proposed);
        if (ioctl (self->fd, VIDIOCSFREQ, &freq) < 0) {
            perror ("ioctl");
            err("Warning: could not set tuner frequency\n");
            return FALSE;
        }

        if (ioctl (self->fd, VIDIOCGFREQ, &freq) < 0) {
            perror ("ioctl");
            err("Warning: set frequency succesfully, but unable to read "
                "results of new setting.\n");
            return TRUE;
        }

        g_value_set_int (actual, freq);
        return TRUE;

    } else {
        // generic V4L controls?

        struct video_picture vpic;
        memset (&vpic, 0, sizeof (vpic));
        if (ioctl (self->fd, VIDIOCGPICT, &vpic) < 0) {
            perror ("ioctl");
            err ("V4L: ioctl VIDIOCGPICT failed\n");
            return FALSE;
        }

        uint16_t *vp = NULL;

        if (ctl == self->brightness_ctl) {
            vp = &vpic.brightness;
        } else if (ctl == self->hue_ctl) {
            vp = &vpic.hue;
        } else if (ctl == self->color_ctl) {
            vp = &vpic.colour;
        } else if (ctl == self->contrast_ctl) {
            vp = &vpic.contrast;
        } else if (ctl == self->whiteness_ctl) {
            vp = &vpic.whiteness;
        }

        *vp = g_value_get_int (proposed);

        if (ioctl (self->fd, VIDIOCSPICT, &vpic) < 0) {
            perror ("ioctl");
            err ("V4L: ioctl VIDIOCSPICT failed\n");
            return FALSE;
        }

        if (ioctl (self->fd, VIDIOCGPICT, &vpic) < 0) {
            perror ("ioctl");
            err ("V4L: ioctl VIDIOCGPICT failed\n");
            g_value_copy (proposed, actual);
            return TRUE;
        }

        g_value_set_int (actual, *vp);
        return TRUE;
    }
}
