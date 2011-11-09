#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <GL/gl.h>

#include <camunits/plugin.h>
#include <opencv/cv.h>

#define err(args...) fprintf(stderr, args)

typedef struct {
    CamUnit parent;
    CvMemStorage *storage;
    CvHaarClassifierCascade *cascade;
    CvSeq *objects;

    CamUnitControl *cascade_ctl;
    CamUnitControl *scale_factor_ctl;
    CamUnitControl *min_neighbors_ctl;
    CamUnitControl *canny_pruning_ctl;
    CamUnitControl *min_window_width_ctl;
    CamUnitControl *min_window_height_ctl;
    CamUnitControl *max_window_width_ctl;
    CamUnitControl *max_window_height_ctl;
} CamcvHCC;

typedef struct {
    CamUnitClass parent_class;
} CamcvHCCClass;

static CamcvHCC * camcv_hcc_new(void);
static int _gl_draw_gl (CamUnit *super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static void _finalize (GObject *obj);

GType camcv_hcc_get_type (void);
CAM_PLUGIN_TYPE (CamcvHCC, camcv_hcc, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camcv_hcc_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("opencv.demo", 
            "object-detection-haar-cascade",
            "Object Detection (Haar Cascade)", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)camcv_hcc_new,
            module);
}

static void
camcv_hcc_class_init (CamcvHCCClass *klass)
{
    // class initializer.  setup the class vtable here.
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;

    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = _finalize;
}

static void
camcv_hcc_init (CamcvHCC *self)
{
    // "private" constructor.  Initialize the unit with some reasonable
    // defaults here.
    CamUnit *super = CAM_UNIT (self);
    self->cascade_ctl = cam_unit_add_control_string(super, "cascade-file",
            "Cascade File", "", 1);
    cam_unit_control_set_ui_hints(self->cascade_ctl, CAM_UNIT_CONTROL_FILENAME);
    self->scale_factor_ctl = cam_unit_add_control_float(super, "scale-factor",
            "Scale Factor", 1.05, 3, 0.05, 1.1, 1);
    cam_unit_control_set_ui_hints(self->scale_factor_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    self->min_neighbors_ctl = cam_unit_add_control_int(super, "min-neighbors",
            "Min Neighbors", 0, 1000, 1, 3, 1);
    cam_unit_control_set_ui_hints(self->min_neighbors_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    self->canny_pruning_ctl = cam_unit_add_control_boolean(super, "canny-pruning",
            "Canny Edge Pruning", 0, 1);
    self->min_window_width_ctl = cam_unit_add_control_int(super, "min-window-width",
            "Min Window Width", 0, 1000, 1, 0, 1);
    cam_unit_control_set_ui_hints(self->min_window_width_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    self->min_window_height_ctl = cam_unit_add_control_int(super, "min-window-height",
            "Min Window Height", 0, 1000, 1, 0, 1);
    cam_unit_control_set_ui_hints(self->min_window_height_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    self->max_window_width_ctl = cam_unit_add_control_int(super, "max-window-width",
            "Max Window Width", 0, 1000, 1, 1000, 1);
    cam_unit_control_set_ui_hints(self->max_window_width_ctl, CAM_UNIT_CONTROL_SPINBUTTON);
    self->max_window_height_ctl = cam_unit_add_control_int(super, "max-window-height",
            "Max Window Height", 0, 1000, 1, 1000, 1);
    cam_unit_control_set_ui_hints(self->max_window_height_ctl, CAM_UNIT_CONTROL_SPINBUTTON);

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);

    self->storage = cvCreateMemStorage(0);
    self->cascade = NULL;
    self->objects = NULL;
}

static CamcvHCC * 
camcv_hcc_new()
{
    // "public" constructor
    return (CamcvHCC*)(g_object_new(camcv_hcc_get_type(), NULL));
}

static void
_finalize (GObject *obj)
{
    CamcvHCC * self = (CamcvHCC*)obj;
    if(self->cascade) {
        cvReleaseHaarClassifierCascade(&self->cascade);
        self->cascade = NULL;
    }
    if(self->storage) {
        cvReleaseMemStorage(&self->storage);
        self->storage = NULL;
    }
    self->objects = NULL;

    G_OBJECT_CLASS (camcv_hcc_parent_class)->finalize (obj);
}

static void
_load_cascade(CamcvHCC *self, const char *filename)
{
    if(self->cascade) {
        cvReleaseHaarClassifierCascade(&self->cascade);
        self->cascade = NULL;
    }
    self->cascade = (CvHaarClassifierCascade*)cvLoad(filename, 0, 0, 0);
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    cam_unit_remove_all_output_formats (super);

    if (! infmt || infmt->pixelformat != CAM_PIXEL_FORMAT_GRAY) return;

    cam_unit_add_output_format (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride);
}

static void 
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamcvHCC *self = (CamcvHCC*)(super);

    CvSize img_size = { infmt->width, infmt->height };
    IplImage *cvimg = cvCreateImage (img_size, IPL_DEPTH_8U, 1);

    for (int r=0; r<infmt->height; r++) {
        memcpy (cvimg->imageData + r * cvimg->widthStep, 
                inbuf->data + r * infmt->row_stride,
                infmt->width);
    }

    double scale_factor = cam_unit_control_get_float(self->scale_factor_ctl);
    int min_neighbors = cam_unit_control_get_int(self->min_neighbors_ctl);
    int canny_pruning = cam_unit_control_get_boolean(self->canny_pruning_ctl);
    int flags = canny_pruning ? CV_HAAR_DO_CANNY_PRUNING : 0;
    CvSize min_window_size = {
        cam_unit_control_get_int(self->min_window_width_ctl), 
        cam_unit_control_get_int(self->min_window_height_ctl)
    };

    if(self->cascade) {
#if (CV_MAJOR_VERSION >= 2 && CV_MINOR_VERSION > 1)
        CvSize max_window_size = {
            cam_unit_control_get_int(self->max_window_width_ctl),
            cam_unit_control_get_int(self->max_window_height_ctl)
        };
        self->objects = cvHaarDetectObjects(cvimg, self->cascade, self->storage,
                scale_factor, min_neighbors, flags, min_window_size, max_window_size);
#else
        self->objects = cvHaarDetectObjects(cvimg, self->cascade, self->storage,
                scale_factor, min_neighbors, flags, min_window_size);
#endif
    }

    cam_unit_produce_frame (super, inbuf, infmt);

    cvReleaseImage (&cvimg);
}

static int 
_gl_draw_gl (CamUnit *super)
{
    CamcvHCC *self = (CamcvHCC*) (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (!outfmt) return 0;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, outfmt->width, outfmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    if(self->objects) {
        glColor3f(0, 1, 0);
        for(int i=0; i<self->objects->total; i++) {
            CvRect *r = (CvRect*) cvGetSeqElem(self->objects, i);
            glBegin(GL_LINE_LOOP);
            glVertex2d(r->x, r->y);
            glVertex2d(r->x + r->width, r->y);
            glVertex2d(r->x + r->width, r->y + r->height);
            glVertex2d(r->x, r->y + r->height);
            glEnd();
        }
    }

    return 0;
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamcvHCC *self = (CamcvHCC*) (super);
    if (ctl == self->cascade_ctl) {
        const char *fname = g_value_get_string (proposed);
        _load_cascade(self, fname);
        if(! self->cascade) {
            return FALSE;
        }
    }

    g_value_copy (proposed, actual);
    return TRUE;
}
