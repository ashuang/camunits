#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <GL/gl.h>

#include <gdk/gdkkeysyms.h>

#include <camunits/unit.h>
#include <camunits/dbg.h>

#include "gl_drawing_area.h"
#include "unit_chain_gl_widget.h"

#define err(args...) fprintf (stderr, args)

typedef struct _CamUnitChainGLWidgetPriv CamUnitChainGLWidgetPriv;
struct _CamUnitChainGLWidgetPriv {
    /*< private >*/
    CamUnitChain *chain;
    GtkWidget *vbox;
    GtkWidget *aspect;
    GtkWidget *gl_area;
    GtkWidget *msg_area;

    double aspect_ratio;
};
#define CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_UNIT_CHAIN_GL_WIDGET, CamUnitChainGLWidgetPriv))

enum {
    GL_DRAW_FINISHED_SIGNAL,
    LAST_SIGNAL
};
static guint _signals[LAST_SIGNAL] = { 0 };

static void cam_unit_chain_gl_widget_finalize (GObject *obj);

G_DEFINE_TYPE (CamUnitChainGLWidget, cam_unit_chain_gl_widget, 
        GTK_TYPE_VBOX);


static gboolean on_gl_configure (GtkWidget *widget, GdkEventConfigure *event, 
        void* user_data);
static gboolean on_gl_expose (GtkWidget * widget, GdkEventExpose * event, 
        void* user_data);
static void on_unit_added (CamUnitChain *chain, CamUnit *unit, 
        void *user_data);
static void on_unit_removed (CamUnitChain *chain, CamUnit *unit, 
        void *user_data);
static void on_unit_status_changed (CamUnit *unit, CamUnitChainGLWidget *self);
static void on_unit_control_changed (CamUnit * unit, CamUnitControl * control,
        CamUnitChainGLWidget * self);
static void on_show_after (CamUnitChainGLWidget *self, CamUnitChain *chain);

static void
cam_unit_chain_gl_widget_init (CamUnitChainGLWidget *self)
{
    dbg (DBG_GUI, "unit chain gl widget constructor\n");
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    
    priv->chain = NULL;

    priv->aspect_ratio = 1.0;
    priv->aspect = gtk_aspect_frame_new (NULL, 0.5, 0.5, 
            priv->aspect_ratio, FALSE);
    gtk_box_pack_start (GTK_BOX (self), priv->aspect, TRUE, TRUE, 0);
    gtk_widget_show (priv->aspect);

    priv->gl_area = cam_gl_drawing_area_new (FALSE);
    g_object_ref_sink (G_OBJECT (priv->gl_area));
    priv->msg_area = gtk_label_new ("No units in chain");
    g_object_ref_sink (G_OBJECT (priv->msg_area));

//    gtk_widget_show (priv->gl_area);
    gtk_container_add (GTK_CONTAINER (priv->aspect), priv->msg_area);
    gtk_widget_show (priv->msg_area);

    g_signal_connect (G_OBJECT (priv->gl_area), "expose-event",
            G_CALLBACK (on_gl_expose), self);
    g_signal_connect (G_OBJECT (priv->gl_area), "configure-event",
            G_CALLBACK (on_gl_configure), self);
}

static void
cam_unit_chain_gl_widget_class_init (CamUnitChainGLWidgetClass *klass)
{
    dbg (DBG_GUI, "unit chain gl widget class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    // add a class-specific destructor
    gobject_class->finalize = cam_unit_chain_gl_widget_finalize;

    /**
     * CamUnit::gl-draw-finished
     * @widget: the CamUnitChainGLWidget emitting the signal
     *
     * Connect to this signal to insert custom OpenGL drawing code.
     *
     * The gl-draw-finished signal is emitted during the OpenGL drawing process
     * after all of the CamUnit objects that can render to OpenGL have done so,
     * but before the OpenGL buffers have been swapped.  At the time this
     * signal is emitted, the current OpenGL context is still set to the one
     * provided by this widget.
     */
    _signals[GL_DRAW_FINISHED_SIGNAL] = 
        g_signal_new("gl-draw-finished",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

    g_type_class_add_private (gobject_class, sizeof (CamUnitChainGLWidgetPriv));
}

// destructor (more or less)
static void
cam_unit_chain_gl_widget_finalize (GObject *obj)
{
    CamUnitChainGLWidget *self = CAM_UNIT_CHAIN_GL_WIDGET (obj);
    dbg (DBG_GUI, "unit chain gl widget finalize\n");
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);

    if (priv->chain) {
        g_signal_handlers_disconnect_by_func (G_OBJECT (priv->chain),
                on_unit_added, self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (priv->chain),
                on_unit_removed, self);

        GList *units = cam_unit_chain_get_units (priv->chain);
        for (GList *uiter=units; uiter; uiter=uiter->next) {
            CamUnit *unit = CAM_UNIT (uiter->data);
            if ((cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) && 
                cam_unit_is_streaming (unit)) {
                dbg (DBG_GUI, 
                        "CamUnitChainGLWidget: draw_gl_shutdown on [%s]\n",
                        cam_unit_get_id (unit));
                cam_unit_draw_gl_shutdown (unit);
            }
            g_signal_handlers_disconnect_by_func (G_OBJECT (unit),
                    on_unit_status_changed, self);
        }
        g_list_free (units);

        dbgl (DBG_REF, "unref chain [%p]\n", priv->chain);
        g_object_unref (priv->chain);
    }
    g_object_unref (G_OBJECT (priv->gl_area));
    g_object_unref (G_OBJECT (priv->msg_area));

    G_OBJECT_CLASS (cam_unit_chain_gl_widget_parent_class)->finalize (obj);
}

CamUnitChainGLWidget *
cam_unit_chain_gl_widget_new (CamUnitChain *chain)
{
    CamUnitChainGLWidget * self = 
        CAM_UNIT_CHAIN_GL_WIDGET (
                g_object_new (CAM_TYPE_UNIT_CHAIN_GL_WIDGET, NULL));


    // XXX should really connect to "realize", but if we do that then our
    // signal handlers gets called before the widget is realized, and no GL
    // context is available...
    g_signal_connect_after (G_OBJECT (self), "show", 
            G_CALLBACK (on_show_after), chain);
    return self;
}

static void
on_show_after (CamUnitChainGLWidget *self, CamUnitChain *chain) 
{
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    if (!priv->chain) {
        cam_unit_chain_gl_widget_set_chain (self, chain);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self),
                on_show_after, self);
    }
}

CamUnitChain *
cam_unit_chain_gl_widget_get_chain (CamUnitChainGLWidget *self)
{
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    return priv->chain;
}

int
cam_unit_chain_gl_widget_set_chain (CamUnitChainGLWidget *self, 
        CamUnitChain *chain)
{
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    if (priv->chain) return -1;
    if (!chain) return -1;
    dbg (DBG_GUI, "CamUnitChainGL: setting chain to [%p]\n", chain);

    priv->chain = chain;

    dbgl (DBG_REF, "ref chain [%p]\n", chain);
    g_object_ref (chain);

    GList *units = cam_unit_chain_get_units (chain);
    for (GList *uiter=units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        on_unit_added (chain, unit, self);
    }
    g_list_free (units);

    g_signal_connect (G_OBJECT (chain), "unit-added", 
            G_CALLBACK (on_unit_added), self);
    g_signal_connect (G_OBJECT (chain), "unit-removed", 
            G_CALLBACK (on_unit_removed), self);

    return 0;
}

static gboolean 
on_gl_configure (GtkWidget *widget, GdkEventConfigure *event, 
        void* user_data)
{
    CamUnitChainGLWidget *self = CAM_UNIT_CHAIN_GL_WIDGET (user_data);
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    if (!priv->chain) return TRUE;

    cam_gl_drawing_area_invalidate (CAM_GL_DRAWING_AREA (priv->gl_area));
    return TRUE;
}

static CamUnit *
get_first_renderable_unit (CamUnitChain *chain)
{
    GList *units = cam_unit_chain_get_units (chain);
    for (GList *uiter=units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) {
            if (cam_unit_get_output_format (unit)) {
                g_list_free (units);
                return unit;
            }
        }
    }
    g_list_free (units);
    return NULL;
}

static void
render_renderable_units (CamUnitChain *chain)
{
    GList *units = cam_unit_chain_get_units (chain);
    for (GList *uiter=units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) {
            cam_unit_draw_gl (unit);
        }
    }
    g_list_free (units);
}

static gboolean 
on_gl_expose (GtkWidget * widget, GdkEventExpose * event, 
        void* user_data)
{
    CamUnitChainGLWidget *self = CAM_UNIT_CHAIN_GL_WIDGET (user_data);
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    if (!priv->chain) goto blank;

    // if no units can draw, then don't bother
    CamUnit *first_render_unit = get_first_renderable_unit (priv->chain);
    if (!first_render_unit) goto blank;

    // some unit can draw, so clear the drawing buffer
    cam_gl_drawing_area_set_context (CAM_GL_DRAWING_AREA (priv->gl_area));

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // setup reasonable default matrices
    const CamUnitFormat *fmt = cam_unit_get_output_format (first_render_unit);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, fmt->width, fmt->height, 0, -1, 1);

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    // set the aspect ratio to match the aspect ratio of the 
    // first unit that can render
    double fmt_aspect = (double)fmt->width / fmt->height;
    if (fmt_aspect != priv->aspect_ratio) {
        priv->aspect_ratio = fmt_aspect;
        gtk_aspect_frame_set (GTK_ASPECT_FRAME (priv->aspect),
                0.5, 0.5, fmt_aspect, FALSE);
    }

    render_renderable_units (priv->chain);

    g_signal_emit (G_OBJECT(self), _signals[GL_DRAW_FINISHED_SIGNAL], 0, NULL);

    cam_gl_drawing_area_swap_buffers (CAM_GL_DRAWING_AREA (priv->gl_area));

    return TRUE;
blank:
    cam_gl_drawing_area_set_context (CAM_GL_DRAWING_AREA (priv->gl_area));
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    cam_gl_drawing_area_swap_buffers (CAM_GL_DRAWING_AREA (priv->gl_area));
    return TRUE;
}

static void
_set_aspect_widget (CamUnitChainGLWidget *self)
{
    int can_render = 0;
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);

    if (priv->chain) {
        GList *units = cam_unit_chain_get_units (priv->chain);
        for (GList *uiter=units; uiter; uiter=uiter->next) {
            CamUnit *unit = CAM_UNIT (uiter->data);
            if (cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) {
                can_render = 1;
                break;
            }
        }
        g_list_free (units);
    } else {
        can_render = 0;
    }

    GList *cur_wlist = 
        gtk_container_get_children (GTK_CONTAINER (priv->aspect));
    GtkWidget *cur_child = cur_wlist->data;
    assert (cur_child);
    g_list_free (cur_wlist);

    if (can_render) {
        if (cur_child == priv->msg_area) {
            gtk_container_remove (GTK_CONTAINER (priv->aspect), priv->msg_area);
            gtk_container_add (GTK_CONTAINER (priv->aspect), priv->gl_area);
            gtk_widget_show (GTK_WIDGET (priv->gl_area));
        } else {
            assert (cur_child == priv->gl_area);
        }
    } else {
        if (cur_child == priv->gl_area) {
            gtk_container_remove (GTK_CONTAINER (priv->aspect), priv->gl_area);
            gtk_container_add (GTK_CONTAINER (priv->aspect), priv->msg_area);
            gtk_widget_show (GTK_WIDGET (priv->msg_area));
        } else {
            assert (cur_child == priv->msg_area);
        }
        GList *units = cam_unit_chain_get_units (priv->chain);
        if (units) {
            gtk_label_set_text (GTK_LABEL (priv->msg_area),
                    "No renderable units in chain.\nDo you need to add an OpenGL unit?");
        } else {
            gtk_label_set_text (GTK_LABEL (priv->msg_area),
                    "No units in chain.");
        }
        g_list_free (units);
    }
}

static void
on_unit_added (CamUnitChain *chain, CamUnit *unit, void *user_data)
{
    CamUnitChainGLWidget *self = CAM_UNIT_CHAIN_GL_WIDGET (user_data);
    _set_aspect_widget (self);
    uint32_t flags = cam_unit_get_flags (unit);
    if (flags & CAM_UNIT_RENDERS_GL) {
        if (cam_unit_is_streaming (unit)) {
            dbg (DBG_GUI, "UnitChainGL:  initializing GL for new unit [%s]\n", 
                    cam_unit_get_name (unit));
            cam_unit_draw_gl_init (unit);
        }
        g_signal_connect (G_OBJECT (unit), "status-changed",
                G_CALLBACK (on_unit_status_changed), self);
        g_signal_connect (G_OBJECT (unit), "control-value-changed",
                G_CALLBACK (on_unit_control_changed), self);
    }
    return;
}

static void
on_unit_removed (CamUnitChain *chain, CamUnit *unit, void *user_data)
{
    uint32_t flags = cam_unit_get_flags (unit);
    if ((flags & CAM_UNIT_RENDERS_GL) && cam_unit_is_streaming (unit)) {
        dbg (DBG_GUI, "UnitChainGL:  shutting down GL for removed unit [%s]\n", 
                cam_unit_get_name (unit));
        cam_unit_draw_gl_shutdown (unit);
    }
    _set_aspect_widget (CAM_UNIT_CHAIN_GL_WIDGET (user_data));
    return;
}

static void
on_unit_control_changed (CamUnit * unit, CamUnitControl * control,
        CamUnitChainGLWidget * self)
{
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    if (cam_unit_is_streaming (unit))
        cam_gl_drawing_area_invalidate (CAM_GL_DRAWING_AREA (priv->gl_area));
}

static void 
on_unit_status_changed (CamUnit *unit, CamUnitChainGLWidget *self)
{
    if (cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) {
        if (cam_unit_is_streaming (unit)) {
            dbg (DBG_GUI, "CamUnitChainGLWidget: draw_gl_init on [%s]\n",
                    cam_unit_get_id (unit));
            cam_unit_draw_gl_init (unit);
        } else {
            dbg (DBG_GUI, "CamUnitChainGLWidget: draw_gl_shutdown on [%s]\n",
                    cam_unit_get_id (unit));
            cam_unit_draw_gl_shutdown (unit);
        }
    }
}

void 
cam_unit_chain_gl_widget_request_redraw (CamUnitChainGLWidget *self)
{
    // trigger an expose event
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    cam_gl_drawing_area_invalidate (CAM_GL_DRAWING_AREA (priv->gl_area));
}

void
cam_unit_chain_gl_widget_set_context(CamUnitChainGLWidget* self)
{
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    cam_gl_drawing_area_set_context (CAM_GL_DRAWING_AREA (priv->gl_area));
}

GtkWidget *
cam_unit_chain_gl_widget_get_gl_area (CamUnitChainGLWidget * self)
{
    CamUnitChainGLWidgetPriv * priv = CAM_UNIT_CHAIN_GL_WIDGET_GET_PRIVATE(self);
    return priv->gl_area;
}
