#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include <GL/gl.h>

#include <camunits/plugin.h>

#include "klt/klt.h"
#include "color_util.h"

#define err(args...) fprintf(stderr, args)

typedef struct {
    CamUnit parent;
//    int max_ncorners;
//    CvPoint2D32f *corners;
//    int ncorners;

    KLT_TrackingContext tc;
    KLT_FeatureList fl;
    int *feature_ages;

    uint8_t *packed_img;
    uint8_t *prev_img;

    CamUnitControl *max_features_ctl;
//    CamUnitControl *quality_ctl;
    CamUnitControl *min_dist_ctl;
//    CamUnitControl *block_size_ctl;
//    CamUnitControl *use_harris_ctl;
//    CamUnitControl *harris_k_ctl;
    CamUnitControl *verbose_ctl;
} CamkltKLT;

typedef struct {
    CamUnitClass parent_class;
} CamkltKLTClass;

static CamkltKLT * camklt_klt_new(void);
static int _stream_init (CamUnit * super, const CamUnitFormat * format);
static int _stream_shutdown (CamUnit * super);
static int _gl_draw_gl (CamUnit *super);
static gboolean _try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);

GType camklt_klt_get_type (void);
CAM_PLUGIN_TYPE (CamkltKLT, camklt_klt, CAM_TYPE_UNIT);

/* These next two functions are required as entry points for the
 * plug-in API. */
void cam_plugin_initialize (GTypeModule * module);
void cam_plugin_initialize (GTypeModule * module)
{
    camklt_klt_register_type (module);
}

CamUnitDriver * cam_plugin_create (GTypeModule * module);
CamUnitDriver * cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("klt-stb", 
            "demo",
            "KLT tracker (Birchfield)", CAM_UNIT_RENDERS_GL, 
            (CamUnitConstructor)camklt_klt_new,
            module);
}

static void
camklt_klt_class_init (CamkltKLTClass *klass)
{
    // class initializer.  setup the class vtable here.
    klass->parent_class.stream_init = _stream_init;
    klass->parent_class.stream_shutdown = _stream_shutdown;
    klass->parent_class.draw_gl = _gl_draw_gl;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.try_set_control = _try_set_control;
}

static void
camklt_klt_init (CamkltKLT *self)
{
    // "private" constructor.  Initialize the unit with some reasonable
    // defaults here.
    CamUnit *super = CAM_UNIT (self);
//    self->quality_ctl = cam_unit_add_control_float (super, "quality",
//            "Quality", 0.0001, 1, 0.1, 0.3, 1);
    self->min_dist_ctl = cam_unit_add_control_int (super, "min-dist",
            "Min. Distance", 1, 100, 1, 10, 1);
//    self->block_size_ctl = cam_unit_add_control_int (super, "block-size",
//            "Block Size", 3, 15, 2, 3, 1);
//    self->use_harris_ctl = cam_unit_add_control_boolean (super, "use-harris",
//            "Use Harris", 1, 1);
//    self->harris_k_ctl = cam_unit_add_control_float (super, "harris-k",
//            "Harris K", 0, 0.5, 0.05, 0.04, 1);
    self->max_features_ctl = cam_unit_add_control_int (super, "max-features",
            "Max Features", 1, 2000, 1, 100, 1);
    self->verbose_ctl = cam_unit_add_control_boolean (super, "verbose",
            "Verbose", 0, 1);
    KLTSetVerbosity(cam_unit_control_get_boolean (self->verbose_ctl));

    self->tc = NULL;
    self->fl = NULL;
    self->packed_img = NULL;
    self->prev_img = NULL;
    self->feature_ages = NULL;

    g_signal_connect (G_OBJECT (self), "input-format-changed",
            G_CALLBACK (on_input_format_changed), self);
}

static CamkltKLT * 
camklt_klt_new()
{
    // "public" constructor
    return (CamkltKLT*)(g_object_new(camklt_klt_get_type(), NULL));
}

static int
_stream_init (CamUnit * super, const CamUnitFormat * format)
{
    CamkltKLT * self = (CamkltKLT*) (super);

    self->tc = KLTCreateTrackingContext ();

    self->tc->mindist = cam_unit_control_get_int (self->min_dist_ctl);

    self->packed_img = malloc (format->width * format->height);
    self->prev_img = malloc(format->width * format->height);
    //self->fl = KLTCreateFeatureList (100);

//    self->max_ncorners = format->width * format->height / 4;
//    self->corners = malloc (sizeof (CvPoint2D32f) * self->max_ncorners);
//    self->ncorners = 0;
    return 0;
}

static int
_stream_shutdown (CamUnit * super)
{
    CamkltKLT * self = (CamkltKLT*) (super);

    KLTFreeTrackingContext (self->tc);
    self->tc = NULL;
    if (self->fl) {
        KLTFreeFeatureList (self->fl);
        self->fl = NULL;
    }

    free (self->packed_img);
    self->packed_img = NULL;

    free (self->prev_img);
    self->prev_img = NULL;

    free (self->feature_ages);
    self->feature_ages = NULL;

//    free (self->corners);
    return 0;
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
    CamkltKLT *self = (CamkltKLT*)(super);

    // make a copy of the image data, both to remove pad bytes between rows,
    // and to keep a copy around for future frame processing
    uint8_t *img_data = (uint8_t*) inbuf->data;
    cam_pixel_copy_8u_generic (inbuf->data, infmt->row_stride,
            self->packed_img, infmt->width,
            0, 0, 0, 0, 
            infmt->width, infmt->height, 8);
    img_data = self->packed_img;

    if (!self->fl) {
        int max_features = cam_unit_control_get_int (self->max_features_ctl);
        self->fl = KLTCreateFeatureList (max_features);
        assert (!self->feature_ages);
        self->feature_ages = malloc (max_features * sizeof (int));
        KLTSelectGoodFeatures (self->tc, img_data, infmt->width, infmt->height,
                self->fl);
    } else {
        KLTTrackFeatures (self->tc, self->prev_img, img_data, 
                infmt->width, infmt->height, self->fl);

        for (int i=0; i<self->fl->nFeatures; i++) {
            if (self->fl->feature[i]->val >= 0) {
                self->feature_ages[i] ++;
            } else {
                self->feature_ages[i] = 0;
            }
        }

        KLTReplaceLostFeatures (self->tc, img_data, 
                infmt->width, infmt->height, self->fl);
    }

    // swap pointers
    uint8_t *tmp = self->prev_img;
    self->prev_img = self->packed_img;
    self->packed_img = tmp;

    cam_unit_produce_frame (super, inbuf, infmt);

    return;
}

static int 
_gl_draw_gl (CamUnit *super)
{
    CamkltKLT *self = (CamkltKLT*) (super);
    const CamUnitFormat *outfmt = cam_unit_get_output_format(super);
    if (!outfmt) return 0;
    if (!self->fl) return 0;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, outfmt->width, outfmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    glPointSize (4.0);

    double COLOR_AGE_MAX = 20;

//    glColor3f (0, 1, 0);
    glBegin (GL_POINTS);
    for (int i=0; i<self->fl->nFeatures; i++) {
        if (self->fl->feature[i]->val >= 0) {
            double a = self->feature_ages[i] / COLOR_AGE_MAX;
            glColor3fv (color_util_jet (a));
            glVertex2f (self->fl->feature[i]->x, self->fl->feature[i]->y);
        }
    }
    glEnd ();
    return 0;
}

static gboolean
_try_set_control (CamUnit *super, const CamUnitControl *ctl, 
        const GValue *proposed, GValue *actual)
{
    CamkltKLT *self = (CamkltKLT*) (super);
    if (ctl == self->max_features_ctl) {
        int old_max = cam_unit_control_get_int (self->max_features_ctl);
        int new_max = g_value_get_int (proposed);
        if (old_max != new_max && self->fl) {
            KLTFreeFeatureList (self->fl);
            self->fl = NULL;
            free (self->feature_ages);
            self->feature_ages = NULL;
        }
        g_value_copy (proposed, actual);
        return TRUE;
    } else if (ctl == self->min_dist_ctl) {
        if (self->tc) {
            self->tc->mindist = g_value_get_int (proposed);
            if (self->fl) {
                KLTFreeFeatureList (self->fl);
                self->fl = NULL;
            }
        }
        g_value_copy (proposed, actual);
        return TRUE;
    } else if (ctl == self->verbose_ctl) {
        KLTSetVerbosity(g_value_get_boolean (proposed));
        g_value_copy (proposed, actual);
        return TRUE;
    }
//    if (ctl == self->block_size_ctl) {
//        int requested = g_value_get_int (proposed);
//        if (requested < 3 || (requested % 2 == 0)) {
//            return FALSE;
//        }
//        g_value_copy (proposed, actual);
//        return TRUE;
//    }

    g_value_copy (proposed, actual);
    return TRUE;
}
