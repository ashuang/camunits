#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <GL/gl.h>

#include <gdk/gdkkeysyms.h>

#include <libcam/unit.h>
#include <libcam/dbg.h>

#include "gl_drawing_area.h"
#include "unit_chain_gl_widget.h"

#define err(args...) fprintf (stderr, args)

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
static void on_unit_status_changed (CamUnit *unit, int old_status, 
        CamUnitChainGLWidget *self);
static void on_unit_control_changed (CamUnit * unit, CamUnitControl * control,
        CamUnitChainGLWidget * self);
static void on_show_after (CamUnitChainGLWidget *self, CamUnitChain *chain);

static void
cam_unit_chain_gl_widget_init (CamUnitChainGLWidget *self)
{
    dbg (DBG_GUI, "unit control widget constructor\n");
    
    self->chain = NULL;

    self->aspect_ratio = 1.0;
    self->aspect = gtk_aspect_frame_new (NULL, 0.5, 0.5, 
            self->aspect_ratio, FALSE);
    gtk_box_pack_start (GTK_BOX (self), self->aspect, TRUE, TRUE, 0);
    gtk_widget_show (self->aspect);

    self->gl_area = cam_gl_drawing_area_new (FALSE);
    g_object_ref_sink (G_OBJECT (self->gl_area));
    self->msg_area = gtk_label_new ("No units in chain");
    g_object_ref_sink (G_OBJECT (self->msg_area));

//    gtk_widget_show (self->gl_area);
    gtk_container_add (GTK_CONTAINER (self->aspect), self->msg_area);
    gtk_widget_show (self->msg_area);

    g_signal_connect (G_OBJECT (self->gl_area), "expose-event",
            G_CALLBACK (on_gl_expose), self);
    g_signal_connect (G_OBJECT (self->gl_area), "configure-event",
            G_CALLBACK (on_gl_configure), self);
}

static void
cam_unit_chain_gl_widget_class_init (CamUnitChainGLWidgetClass *klass)
{
    dbg (DBG_GUI, "unit chain gl widget class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    // add a class-specific destructor
    gobject_class->finalize = cam_unit_chain_gl_widget_finalize;
}

// destructor (more or less)
static void
cam_unit_chain_gl_widget_finalize (GObject *obj)
{
    CamUnitChainGLWidget *self = CAM_UNIT_CHAIN_GL_WIDGET (obj);
    dbg (DBG_GUI, "unit chain gl widget finalize\n");

    if (self->chain) {
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->chain),
                on_unit_added, self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->chain),
                on_unit_removed, self);

        GList *units = cam_unit_chain_get_units (self->chain);
        for (GList *uiter=units; uiter; uiter=uiter->next) {
            CamUnit *unit = CAM_UNIT (uiter->data);
            if ((cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) && 
                cam_unit_get_status (unit) != CAM_UNIT_STATUS_IDLE) {
                dbg (DBG_GUI, 
                        "CamUnitChainGLWidget: draw_gl_shutdown on [%s]\n",
                        cam_unit_get_id (unit));
                cam_unit_draw_gl_shutdown (unit);
            }
            g_signal_handlers_disconnect_by_func (G_OBJECT (unit),
                    on_unit_status_changed, self);
        }
        g_list_free (units);

        dbgl (DBG_REF, "unref chain [%p]\n", self->chain);
        g_object_unref (self->chain);
    }
    g_object_unref (G_OBJECT (self->gl_area));
    g_object_unref (G_OBJECT (self->msg_area));

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
    if (!self->chain) {
        cam_unit_chain_gl_widget_set_chain (self, chain);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self),
                on_show_after, self);
    }
}

int
cam_unit_chain_gl_widget_set_chain (CamUnitChainGLWidget *self, 
        CamUnitChain *chain)
{
    if (self->chain) return -1;
    if (!chain) return -1;
    dbg (DBG_GUI, "CamUnitChainGL: setting chain to [%p]\n", chain);

    self->chain = chain;

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
    if (!self->chain) return TRUE;

    cam_gl_drawing_area_invalidate (CAM_GL_DRAWING_AREA (self->gl_area));
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
    if (!self->chain) goto blank;

    // if no units can draw, then don't bother
    CamUnit *first_render_unit = get_first_renderable_unit (self->chain);
    if (!first_render_unit) goto blank;

    // some unit can draw, so clear the drawing buffer
    cam_gl_drawing_area_set_context (CAM_GL_DRAWING_AREA (self->gl_area));
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    // set the aspect ratio to match the aspect ratio of the 
    // first unit that can render
    const CamUnitFormat *fmt = cam_unit_get_output_format (first_render_unit);
    double fmt_aspect = (double)fmt->width / fmt->height;
    if (fmt_aspect != self->aspect_ratio) {
        self->aspect_ratio = fmt_aspect;
        gtk_aspect_frame_set (GTK_ASPECT_FRAME (self->aspect),
                0.5, 0.5, fmt_aspect, FALSE);
    }

    render_renderable_units (self->chain);

    cam_gl_drawing_area_swap_buffers (CAM_GL_DRAWING_AREA (self->gl_area));

    return TRUE;
blank:
    cam_gl_drawing_area_set_context (CAM_GL_DRAWING_AREA (self->gl_area));
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    cam_gl_drawing_area_swap_buffers (CAM_GL_DRAWING_AREA (self->gl_area));
    return TRUE;
}

static void
_set_aspect_widget (CamUnitChainGLWidget *self)
{
    int can_render = 0;

    if (self->chain) {
        GList *units = cam_unit_chain_get_units (self->chain);
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
        gtk_container_get_children (GTK_CONTAINER (self->aspect));
    GtkWidget *cur_child = cur_wlist->data;
    assert (cur_child);
    g_list_free (cur_wlist);

    if (can_render) {
        if (cur_child == self->msg_area) {
            gtk_container_remove (GTK_CONTAINER (self->aspect), self->msg_area);
            gtk_container_add (GTK_CONTAINER (self->aspect), self->gl_area);
            gtk_widget_show (GTK_WIDGET (self->gl_area));
        } else {
            assert (cur_child == self->gl_area);
        }
    } else {
        if (cur_child == self->gl_area) {
            gtk_container_remove (GTK_CONTAINER (self->aspect), self->gl_area);
            gtk_container_add (GTK_CONTAINER (self->aspect), self->msg_area);
            gtk_widget_show (GTK_WIDGET (self->msg_area));
        } else {
            assert (cur_child == self->msg_area);
        }
        GList *units = cam_unit_chain_get_units (self->chain);
        if (units) {
            gtk_label_set_text (GTK_LABEL (self->msg_area),
                    "No renderable units in chain.\nDo you need to add render:opengl?");
        } else {
            gtk_label_set_text (GTK_LABEL (self->msg_area),
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
        if (cam_unit_get_status (unit) != CAM_UNIT_STATUS_IDLE) {
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
    _set_aspect_widget (CAM_UNIT_CHAIN_GL_WIDGET (user_data));
    if (flags & CAM_UNIT_RENDERS_GL) {
        dbg (DBG_GUI, "UnitChainGL:  shutting down GL for removed unit [%s]\n", 
                cam_unit_get_name (unit));
        cam_unit_draw_gl_shutdown (unit);
    }
    return;
}

static void
on_unit_control_changed (CamUnit * unit, CamUnitControl * control,
        CamUnitChainGLWidget * self)
{
    if (cam_unit_get_status (unit) != CAM_UNIT_STATUS_IDLE)
        cam_gl_drawing_area_invalidate (CAM_GL_DRAWING_AREA (self->gl_area));
}

static void 
on_unit_status_changed (CamUnit *unit, int old_status, 
        CamUnitChainGLWidget *self)
{
    int new_status = cam_unit_get_status (unit);
    if (cam_unit_get_flags (unit) & CAM_UNIT_RENDERS_GL) {
        if (new_status == CAM_UNIT_STATUS_IDLE) {
            dbg (DBG_GUI, 
                    "CamUnitChainGLWidget: draw_gl_shutdown on [%s]\n",
                    cam_unit_get_id (unit));
            cam_unit_draw_gl_shutdown (unit);
        } else if (old_status == CAM_UNIT_STATUS_IDLE) {
            dbg (DBG_GUI, "CamUnitChainGLWidget: draw_gl_init on [%s]\n",
                    cam_unit_get_id (unit));
            cam_unit_draw_gl_init (unit);
        }
    }
}

void 
cam_unit_chain_gl_widget_request_redraw (CamUnitChainGLWidget *self)
{
    // trigger an expose event
    cam_gl_drawing_area_invalidate (CAM_GL_DRAWING_AREA (self->gl_area));
}

GtkWidget *
cam_unit_chain_gl_widget_get_gl_area (CamUnitChainGLWidget * self)
{
    return self->gl_area;
}
