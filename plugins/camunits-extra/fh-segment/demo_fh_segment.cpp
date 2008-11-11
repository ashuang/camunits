#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <camunits/cam.h>
#include <camunits/plugin.h>
#include <camunits/gl_texture.h>

#include "segment/segment-image.h"
#include "segment/pnmfile.h"

#include "glib_util.h"

extern "C" {

/**
 * CamFHSegmenter
 *
 * CamUnit implementation of the Felzenswalb and Huttenlocher image segmenter
 */

typedef struct _CamFHSegmenter CamFHSegmenter;
typedef struct _CamFHSegmenterClass CamFHSegmenterClass;

// boilerplate
#define CAM_TYPE_FHSEGMENTER  cam_fhsegmenter_get_type()
#define CAM_FHSEGMENTER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_FHSEGMENTER, CamFHSegmenter))
#define CAM_FHSEGMENTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_FHSEGMENTER, CamFHSegmenterClass ))
#define IS_CAM_FHSEGMENTER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_FHSEGMENTER ))
#define IS_CAM_FHSEGMENTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_FHSEGMENTER))
#define CAM_FHSEGMENTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_FHSEGMENTER, CamFHSegmenterClass))

struct _CamFHSegmenter {
    CamUnit parent;
    CamUnitControl *sigma_ctl;
    CamUnitControl *k_ctl;
    CamUnitControl *min_size_ctl;
    CamUnitControl *gl_alpha_ctl;
    CamUnitControl *frame_2_frame_associate_ctl;

    disjoint_set_forest *last_segmentation;

    CamGLTexture * frame_tex;
    uint8_t * gl_buf;
    rgb * random_colors;

    GHashTable *last_segments;
};

struct _CamFHSegmenterClass {
    CamUnitClass parent_class;
};

GType cam_fhsegmenter_get_type (void);

CamFHSegmenter * cam_fhsegmenter_new(void);

typedef struct _fhsegment_t fhsegment_t;

struct _fhsegment_t {
    int id;
    rgb color;
    fhsegment_t *prev_seg;
    fhsegment_t *next_seg;
    GHashTable *vote_bins;
};

typedef struct {
    int nvotes;
    fhsegment_t *old_seg;
    fhsegment_t *new_seg;
} fhseg_assoc_vote_bin_t;

fhseg_assoc_vote_bin_t * fhseg_assoc_vote_bin_new (fhsegment_t *old_seg, 
        fhsegment_t *new_seg) {
    fhseg_assoc_vote_bin_t *self = 
        (fhseg_assoc_vote_bin_t*) g_slice_new (fhseg_assoc_vote_bin_t);
    assert (old_seg && new_seg);
    self->old_seg = old_seg;
    self->new_seg = new_seg;
    self->nvotes = 0;
    return self;
}

void fhseg_assoc_vote_bin_destroy (fhseg_assoc_vote_bin_t *self) {
    g_slice_free (fhseg_assoc_vote_bin_t, self);
}

inline bool operator < (const fhseg_assoc_vote_bin_t &a, 
        const fhseg_assoc_vote_bin_t &b) {
    return a.nvotes < b.nvotes;
}

static int fhseg_assoc_vote_bin_compare (const void *ap, const void *bp) {
    const fhseg_assoc_vote_bin_t *a = *(const fhseg_assoc_vote_bin_t**) ap;
    const fhseg_assoc_vote_bin_t *b = *(const fhseg_assoc_vote_bin_t**) bp;
    if (a->nvotes < b->nvotes) return -1;
    if (a->nvotes > b->nvotes) return 1;
    return 0;
}

static fhsegment_t* 
fhsegment_new (int id) {
    fhsegment_t *self = (fhsegment_t*) g_slice_new (fhsegment_t);
    self->id = id;
    self->color.r = self->color.g = self->color.b = 0;
    self->prev_seg = NULL;
    self->next_seg = NULL;
    self->vote_bins = g_hash_table_new_full (g_direct_hash, g_direct_equal,
            NULL, (GDestroyNotify) fhseg_assoc_vote_bin_destroy);
    return self;
}

static void 
fhsegment_destroy (fhsegment_t *self) {
    g_hash_table_destroy (self->vote_bins);
    g_slice_free (fhsegment_t, self);
}

static void 
fhsegment_add_assoc_vote (fhsegment_t *self, fhsegment_t *old_seg) {
    fhseg_assoc_vote_bin_t *bin = (fhseg_assoc_vote_bin_t*)
        g_hash_table_lookup (self->vote_bins, old_seg);
    if (! bin) {
        bin = fhseg_assoc_vote_bin_new (old_seg, self);
        g_hash_table_insert (self->vote_bins, old_seg, bin);
    } 
    bin->nvotes++;
}

// =========

/* Boilerplate */
CAM_PLUGIN_TYPE (CamFHSegmenter, cam_fhsegmenter, CAM_TYPE_UNIT);

void
cam_plugin_initialize (GTypeModule * module)
{
    cam_fhsegmenter_register_type (module);
}

CamUnitDriver *
cam_plugin_create (GTypeModule * module)
{
    return cam_unit_driver_new_stock_full ("demo",
            "fh-segment",
            "Felzenswalb/Huttenlocher segmenter", 
            CAM_UNIT_RENDERS_GL, (CamUnitConstructor)cam_fhsegmenter_new,
            module);
}

// ============== CamFHSegmenter ===============
static void cam_fhsegmenter_finalize (GObject *obj);
static void on_input_frame_ready (CamUnit * super, const CamFrameBuffer *inbuf,
        const CamUnitFormat *infmt);
static void on_input_format_changed (CamUnit *super, 
        const CamUnitFormat *infmt);
static int draw_gl_init (CamUnit *super);
static int draw_gl (CamUnit *super);
static int draw_gl_shutdown (CamUnit *super);

// class initializer
static void
cam_fhsegmenter_class_init (CamFHSegmenterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_fhsegmenter_finalize;
    klass->parent_class.on_input_frame_ready = on_input_frame_ready;
    klass->parent_class.draw_gl_init = draw_gl_init;
    klass->parent_class.draw_gl = draw_gl;
    klass->parent_class.draw_gl_shutdown = draw_gl_shutdown;
}

CamFHSegmenter * 
cam_fhsegmenter_new()
{
    return CAM_FHSEGMENTER (g_object_new(CAM_TYPE_FHSEGMENTER, NULL));
}

static void
cam_fhsegmenter_init (CamFHSegmenter *self)
{
    // Initialize the unit with some reasonable defaults here.
    CamUnit *super = CAM_UNIT (self);

    self->sigma_ctl = cam_unit_add_control_float (super, 
            "sigma", "Sigma", 0, 1, 0.05, 0.5, 1);
    self->k_ctl = cam_unit_add_control_float (super, 
            "k", "k", 1, 1000, 50, 200, 1);
    self->min_size_ctl = cam_unit_add_control_int (super, 
            "min-component-size", "Min component size", 1, 1000, 1, 20, 1);
    self->gl_alpha_ctl = cam_unit_add_control_float (super, 
            "alpha", "OpenGL alpha", 0, 1, 0.1, 0.5, 1);
    self->frame_2_frame_associate_ctl = cam_unit_add_control_boolean (super,
            "track", "Track", 1, 1);
    g_signal_connect (G_OBJECT(self), "input-format-changed",
            G_CALLBACK(on_input_format_changed), NULL);

    self->last_segmentation = NULL;
    self->frame_tex = NULL;
    self->gl_buf = NULL;
    self->random_colors = NULL;
    self->last_segments = NULL;
}

static void
cam_fhsegmenter_finalize (GObject *obj)
{
    CamFHSegmenter *self = CAM_FHSEGMENTER (obj);

    if (self->last_segmentation) delete self->last_segmentation;
    if (self->gl_buf) delete [] self->gl_buf;
    if (self->random_colors) delete [] self->random_colors;

    if (self->last_segments) {
        g_hash_table_destroy (self->last_segments);
    }

    G_OBJECT_CLASS (cam_fhsegmenter_parent_class)->finalize(obj);
}

static void
on_input_frame_ready (CamUnit *super, const CamFrameBuffer *inbuf, 
        const CamUnitFormat *infmt)
{
    CamFHSegmenter *self = CAM_FHSEGMENTER(super);

	image<rgb> *im = new image<rgb>(infmt->width, infmt->height);
	for (int row=0; row<infmt->height; row++) {
		memcpy (imPtr(im,0,row), inbuf->data + row*infmt->row_stride, 
				infmt->width * 3);
	}

	double sigma = cam_unit_control_get_float (self->sigma_ctl);
	double k = cam_unit_control_get_float (self->k_ctl);
	int min_size = cam_unit_control_get_int (self->min_size_ctl);
    assert (min_size > 0 && k > 0 && sigma > 0);

	disjoint_set_forest *new_seg = segment_image(im, sigma, k, min_size);

    int associate = 
        cam_unit_control_get_boolean (self->frame_2_frame_associate_ctl);

    if (self->frame_tex) {

        int width = infmt->width;
        int height = infmt->height;

        // for each pixel, find its cluster from the previous segmentation.
        // Then find the cluster of the new segmentation and cast a vote in the
        // new cluster to associate with the old cluster.
        GHashTable *new_seg_table = 
            g_hash_table_new_full (g_int_hash, g_int_equal, 
                    NULL, (GDestroyNotify) fhsegment_destroy);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // pull out id of old and new segment
                int new_id = new_seg->find (y * width + x);

                // find or create the new segment
                fhsegment_t *new_seg = (fhsegment_t*)
                    g_hash_table_lookup (new_seg_table, &new_id);
                if (!new_seg) {
                    new_seg = fhsegment_new (new_id);
                    g_hash_table_insert (new_seg_table, &new_seg->id, new_seg);
                }

                if (self->last_segments && associate) {
                    int old_id = self->last_segmentation->find (y * width + x);
                    fhsegment_t *old_seg = (fhsegment_t*) 
                        g_hash_table_lookup (self->last_segments, &old_id);
                    assert (old_seg);
                    fhsegment_add_assoc_vote (new_seg, old_seg);
                }
            }
        }

        // now sort the votes
        if (self->last_segments && associate) {
            // allocate a pointer array for the vote bins and reserve a ton of
            // space for it.  It's okay if there are more vote bins than this, 
            // as the array grows automatically.
            GPtrArray *assoc_votes = 
                g_ptr_array_sized_new (infmt->width * infmt->height);

            // the vote bins are a bit hard to access, as they're scattered in
            // a bunch of different hash tables.  consolidate them here.
            GPtrArray *segments = gu_hash_table_get_vals (new_seg_table);
            for (int i=0; i < (int)segments->len; i++) {
                fhsegment_t *seg = (fhsegment_t*) segments->pdata[i];
                GPtrArray *vote_bins = gu_hash_table_get_vals (seg->vote_bins);
                for (int j=0; j < (int)vote_bins->len; j++) {
                    fhseg_assoc_vote_bin_t *bin = 
                        (fhseg_assoc_vote_bin_t*)vote_bins->pdata[j];

                    // ignore vote bins with a small number of votes
                    if (bin->nvotes > 20) {
                        g_ptr_array_add (assoc_votes, bin);
                    }
                }
                g_ptr_array_free (vote_bins, TRUE);
            }
            g_ptr_array_free (segments, TRUE);

            // vote bins assembled, now sort.
            g_ptr_array_sort (assoc_votes, fhseg_assoc_vote_bin_compare);

            // assoc_votes now has all the vote bins in ascending order (# of
            // votes).  do the segment associations in a greedy fashion.
            for (int i=assoc_votes->len-1; i>=0; i--) {
                fhseg_assoc_vote_bin_t *bin = 
                    (fhseg_assoc_vote_bin_t*) assoc_votes->pdata[i];

                assert (bin->old_seg && bin->new_seg);

                // one old segment may not be associated with more than one new
                // segment.  It is possible that an old segment is not
                // associated with any new segment, and vice versa.
                if (! bin->old_seg->next_seg && 
                    ! bin->new_seg->prev_seg) {
                    bin->new_seg->prev_seg = bin->old_seg;
                    bin->old_seg->next_seg = bin->new_seg;

                    bin->new_seg->color.r = bin->old_seg->color.r;
                    bin->new_seg->color.g = bin->old_seg->color.g;
                    bin->new_seg->color.b = bin->old_seg->color.b;
                }
            }

            g_ptr_array_free (assoc_votes, TRUE);
        }

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int comp_id = new_seg->find (y * width + x);
                fhsegment_t *seg = (fhsegment_t*)
                    g_hash_table_lookup (new_seg_table, &comp_id);

                // if a new segment was not mapped to an old segment, then
                // assign the new segment a random color
                if (!seg->prev_seg) {
                    rgb color = self->random_colors[seg->id];
                    seg->color.r = color.r;
                    seg->color.g = color.g;
                    seg->color.b = color.b;
                }

                self->gl_buf[y*width*3 + x*3 + 0] = seg->color.r;
                self->gl_buf[y*width*3 + x*3 + 1] = seg->color.g;
                self->gl_buf[y*width*3 + x*3 + 2] = seg->color.b;
            }
        }  

        cam_gl_texture_upload (self->frame_tex, CAM_PIXEL_FORMAT_RGB, width*3,
                self->gl_buf);

        if (self->last_segments) {
            g_hash_table_destroy (self->last_segments);
        }
        self->last_segments = new_seg_table;
    }

    if (self->last_segmentation) delete self->last_segmentation;
    self->last_segmentation = new_seg;

    cam_unit_produce_frame (super, inbuf, infmt);

	delete im;
}

// random color
static rgb random_rgb(){ 
    rgb c;

    c.r = (uchar)random();
    c.g = (uchar)random();
    c.b = (uchar)random();

    return c;
}

static void
on_input_format_changed (CamUnit *super, const CamUnitFormat *infmt)
{
    CamFHSegmenter *self = CAM_FHSEGMENTER (super);
    cam_unit_remove_all_output_formats (super);
    if (!infmt || infmt->pixelformat != CAM_PIXEL_FORMAT_RGB) return;

    cam_unit_add_output_format_full (super, infmt->pixelformat,
            infmt->name, infmt->width, infmt->height, 
            infmt->row_stride, infmt->max_data_size);

    if (self->last_segmentation) {
        delete self->last_segmentation;
        self->last_segmentation = NULL;
    }

    if (self->gl_buf) delete [] self->gl_buf;
    self->gl_buf = new uint8_t [infmt->width * infmt->height * 3];

    // pick random colors for each component
    if (self->random_colors) delete [] self->random_colors;
    self->random_colors = new rgb[infmt->width*infmt->height];
    for (int i = 0; i < infmt->width*infmt->height; i++) {
        self->random_colors[i] = random_rgb();
    }

    if (self->last_segments) {
        g_hash_table_destroy (self->last_segments);
        self->last_segments = NULL;
    }
}

static int 
draw_gl_init (CamUnit *super)
{
    CamFHSegmenter *self = CAM_FHSEGMENTER (super);
    if (! super->input_unit) {
        return -1;
    }
    const CamUnitFormat *outfmt = cam_unit_get_output_format (super);
    if (! outfmt) {
        return -1;
    }

    if (! self->frame_tex) {
        self->frame_tex = cam_gl_texture_new (outfmt->width, 
                outfmt->height, outfmt->height * outfmt->width * 3);
    }
    if (!self->frame_tex) return -1;
    return 0;
}

static int 
draw_gl (CamUnit *super)
{
    CamFHSegmenter *self = CAM_FHSEGMENTER (super);
    if (! super->fmt || !self->frame_tex) return -1;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, super->fmt->width, super->fmt->height, 0, -1, 1);
    glMatrixMode (GL_MODELVIEW);

    double alpha = cam_unit_control_get_float (self->gl_alpha_ctl);
    cam_gl_texture_draw_alpha (self->frame_tex, alpha);
    return 0;
}

static int 
draw_gl_shutdown (CamUnit *super)
{
    CamFHSegmenter *self = CAM_FHSEGMENTER (super);
    if (self->frame_tex) {
        cam_gl_texture_free (self->frame_tex);
        self->frame_tex = NULL;
    }
    return 0;
}

}
