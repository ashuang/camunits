#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <camunits/dbg.h>

#include "unit_manager_widget.h"
#include "unit_control_widget.h"
#include "unit_chain_widget.h"

#define err(args...) fprintf(stderr, args)

static void cam_unit_chain_widget_finalize (GObject *obj);
//static void append_new_unit (CamUnitChainWidget *self, 
//        CamUnitDescription *udesc);
static gboolean on_drag_motion (GtkWidget * widget, 
        GdkDragContext * drag, gint x, gint y, guint time, 
        CamUnitChainWidget *self);
static void on_drag_drop(GtkWidget *widget, 
        GdkDragContext *drag, gint x, gint y, guint time, 
        CamUnitChainWidget *self);
static void on_drag_leave(GtkWidget *widget, GdkDragContext *drag, guint time, 
        CamUnitChainWidget *self);
static void on_unit_added(CamUnitChain *chain, CamUnit *unit, 
        CamUnitChainWidget *self);
static void on_unit_removed(CamUnitChain *chain, CamUnit *unit, 
        CamUnitChainWidget *self);
static void on_unit_reordered(CamUnitChain *chain, CamUnit *unit, 
        CamUnitChainWidget *self);
static void on_unit_control_close_button_clicked(CamUnitControlWidget *ucw, 
        CamUnitChainWidget *self);

static void make_widget_for_unit (CamUnitChainWidget *self, CamUnit *unit);

typedef struct _ChainWidgetInfo {
    GtkWidget *widget;
    GtkWidget *labelval;
    int maxchars;
    CamUnitChain *ctl;
} ChainWidgetInfo;

G_DEFINE_TYPE (CamUnitChainWidget, cam_unit_chain_widget, 
        GTK_TYPE_EVENT_BOX);

static void
cam_unit_chain_widget_init (CamUnitChainWidget *self)
{
    dbg(DBG_GUI, "unit chain widget constructor\n");

    cam_unit_chain_widget_set_orientation(self, GTK_ORIENTATION_VERTICAL);
    
    GtkTargetEntry drag_sources[2];
    memcpy (drag_sources + 0, &cam_unit_manager_widget_target_entry,
            sizeof(GtkTargetEntry));
    memcpy (drag_sources + 1, &cam_unit_control_widget_target_entry,
            sizeof(GtkTargetEntry));

    gtk_drag_dest_set (GTK_WIDGET(self), GTK_DEST_DEFAULT_MOTION, 
//            &cam_unit_description_widget_target_entry, 1,
            drag_sources, sizeof(drag_sources)/sizeof(GtkTargetEntry), 
            GDK_ACTION_PRIVATE);
    g_signal_connect (G_OBJECT (self), "drag-motion",
            G_CALLBACK (on_drag_motion), self);
    g_signal_connect (G_OBJECT(self), "drag-drop", G_CALLBACK(on_drag_drop), 
            self);
    g_signal_connect (G_OBJECT(self), "drag-leave", G_CALLBACK(on_drag_leave), 
            self);

    self->drag_proxy = NULL;
    self->insert_position = -1;

    self->chain = NULL;
}

static void
cam_unit_chain_widget_class_init (CamUnitChainWidgetClass *klass)
{
    dbg(DBG_GUI, "unit chain widget class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    // add a class-specific destructor
    gobject_class->finalize = cam_unit_chain_widget_finalize;
}

// destructor (more or less)
static void
cam_unit_chain_widget_finalize (GObject *obj)
{
    CamUnitChainWidget *self = CAM_UNIT_CHAIN_WIDGET (obj);
    dbg(DBG_GUI, "unit chain widget finalize\n");

    g_object_unref(self->chain);

    G_OBJECT_CLASS (cam_unit_chain_widget_parent_class)->finalize(obj);
}

CamUnitChainWidget *
cam_unit_chain_widget_new (CamUnitChain *chain)
{
    CamUnitChainWidget * self = 
        CAM_UNIT_CHAIN_WIDGET(g_object_new(CAM_TYPE_UNIT_CHAIN_WIDGET, NULL));
    cam_unit_chain_widget_set_chain (self, chain);
    return self;
}

int
cam_unit_chain_widget_set_chain(CamUnitChainWidget *self, CamUnitChain *chain)
{
    if (self->chain) return -1;
    self->chain = chain;

    g_object_ref(chain);

    // add widgets for all the existing units in the chain
    GList *units = cam_unit_chain_get_units (chain);
    GList *uiter;
    for (uiter=units; uiter; uiter=uiter->next) {
        make_widget_for_unit (self, CAM_UNIT(uiter->data));
    }
    g_list_free (units);

    // subscribe to chain signals for prompt updating of widget when the chain
    // changes
    g_signal_connect(G_OBJECT(chain), "unit-added", 
            G_CALLBACK(on_unit_added), self);
    g_signal_connect(G_OBJECT(chain), "unit-removed", 
            G_CALLBACK(on_unit_removed), self);
    g_signal_connect(G_OBJECT(chain), "unit-reordered", 
            G_CALLBACK(on_unit_reordered), self);

    return 0;
}

void
cam_unit_chain_widget_set_orientation(CamUnitChainWidget *self,
       GtkOrientation orientation)
{
    if ((orientation == GTK_ORIENTATION_HORIZONTAL && 
                GTK_IS_HBOX (self->box)) ||
        (orientation == GTK_ORIENTATION_VERTICAL && 
                GTK_IS_VBOX (self->box))) return;

    GtkBox *newbox = NULL;
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        newbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        self->child_expand = TRUE;
        self->child_fill = TRUE;
    } else {
        newbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        self->child_expand = FALSE;
        self->child_fill = FALSE;
    }
    self->child_padding = 5;
    if (self->box) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(self->box));
        GList *iter;
        for (iter=children; iter; iter=iter->next) {
            GtkWidget *child = GTK_WIDGET(iter->data);
            gtk_widget_reparent (child, GTK_WIDGET(newbox));
            gtk_box_set_child_packing (newbox, child, self->child_expand, 
                    self->child_fill, self->child_padding, GTK_PACK_START);
        }
        g_list_free(children);

        gtk_container_remove (GTK_CONTAINER(self), GTK_WIDGET(self->box));
    }
    gtk_container_add (GTK_CONTAINER(self), GTK_WIDGET(newbox));
    self->box = newbox;
    gtk_widget_show (GTK_WIDGET(newbox));

}

// this function is invoked when the user starts dragging a control widget
// around.  the control widget is removed from the chain widget, and given its
// own floating window.
static void
on_unit_control_drag_begin(GtkWidget *widget, GdkDragContext *drag,
        CamUnitChainWidget *self)
{
    dbgl (DBG_GUI, "ctl drag begin\n");
    int x, y;
    int w, h;

    w = widget->allocation.width;
    h = widget->allocation.height;

    gtk_widget_get_pointer (widget, &x, &y);
    GtkWidget *drag_window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_widget_reparent(widget, drag_window);
    gtk_widget_set_size_request(widget, w, h);
    gtk_drag_set_icon_widget (drag, drag_window, x, y);

    self->insert_position = 
        cam_unit_chain_get_unit_index (self->chain, 
                CAM_UNIT_CONTROL_WIDGET(widget)->unit);

    g_object_set_data (G_OBJECT (widget), "cam-unit-control-widget",
            widget);
}

/* This is invoked when a drag operation ends, regardless of whether
 * the unit was dropped in a valid or invalid location. */
static void
on_unit_control_drag_end(GtkWidget *widget, GdkDragContext *drag,
        CamUnitChainWidget *self)
{
    dbgl (DBG_GUI, "ctl drag end\n");

    CamUnitControlWidget *ucw = CAM_UNIT_CONTROL_WIDGET(widget);
    CamUnit *unit = ucw->unit;
    int original_index = cam_unit_chain_get_unit_index(self->chain, unit);
    GtkWidget *old_parent = gtk_widget_get_parent(widget);

    gtk_widget_reparent(widget, GTK_WIDGET(self->box));
    gtk_box_set_child_packing(self->box, widget, self->child_expand, 
            self->child_fill, self->child_padding, GTK_PACK_START);
    if (original_index == self->insert_position) {
        gtk_box_reorder_child (self->box, widget, original_index);
    } else {
        cam_unit_chain_reorder_unit(self->chain, unit, self->insert_position);
    }
    gtk_widget_show(widget);
    gtk_widget_destroy(old_parent);
}

static gboolean
on_drag_motion (GtkWidget * widget, GdkDragContext * drag, 
        gint x, gint y, guint time, CamUnitChainWidget *self)
{
    GtkWidget * source = gtk_drag_get_source_widget (drag);

    GtkWidget * control_widget = g_object_get_data (G_OBJECT (source),
            "cam-unit-control-widget");

    if (!control_widget || !IS_CAM_UNIT_CONTROL_WIDGET (control_widget)) {
//        printf("Warning: \"cam-unit-control-widget\" not set "
//                "on drag context\n");
        return FALSE;
    }
    if (! CAM_UNIT_CONTROL_WIDGET(control_widget)->unit) {
        return FALSE;
    }
    gdk_drag_status (drag, GDK_ACTION_MOVE, time);

    if (self->drag_proxy) {
        gtk_widget_destroy (self->drag_proxy);
        self->drag_proxy = NULL;
    }

    // which unit?
    self->insert_position = 0;
    GList *biter;
    for (biter=self->box->children; biter; biter=biter->next) {
        GtkBoxChild *child = (GtkBoxChild*) biter->data;
        if (! IS_CAM_UNIT_CONTROL_WIDGET (child->widget)) continue;

        int ucw_x, ucw_y;
        gtk_widget_translate_coordinates (widget, child->widget, 
                x, y, &ucw_x, &ucw_y);

        if (ucw_y <= child->widget->allocation.height/2) break;
        self->insert_position++;
    }

    if (! self->drag_proxy) {
        self->drag_proxy = gtk_drawing_area_new();
        gtk_widget_set_size_request (self->drag_proxy, 
                control_widget->allocation.width,
                control_widget->allocation.height);
        gtk_box_pack_start (self->box, self->drag_proxy, 
               self->child_expand, self->child_fill, self->child_padding);
        gtk_drag_highlight (self->drag_proxy);
    }

    gtk_box_reorder_child(self->box, self->drag_proxy, self->insert_position);
    gtk_widget_show (self->drag_proxy);

//    gtk_widget_set_size_request(s->drag_proxy, -1, widget->allocation.height);
//    gtk_widget_show(s->drag_proxy);

//    set_drag_dest_slot (self, x, y);
    return TRUE;
}

static void
on_drag_leave(GtkWidget *widget, GdkDragContext *drag, guint time, 
        CamUnitChainWidget *self)
{
    dbgl (DBG_GUI, "drag leave\n");
    if (self->drag_proxy) {
        gtk_widget_destroy (self->drag_proxy);
        self->drag_proxy = NULL;
    }
}

static void
on_drag_drop(GtkWidget *widget, GdkDragContext *drag, gint x,
        gint y, guint time, CamUnitChainWidget *self)
{
    dbgl (DBG_GUI, "chain drag drop\n");
    GtkWidget *source_widget = gtk_drag_get_source_widget (drag);

    GtkWidget * control_widget = g_object_get_data (G_OBJECT (source_widget),
            "cam-unit-control-widget");
    if (!control_widget) {
        err("Warning: \"cam-unit-control-widget\" not set on drag context\n");
        return;
    }

    if (self->insert_position < 0) {
        err("ChainWidget: invalid insert position %d!!\n", 
                self->insert_position);
        return;
    }

    dbg(DBG_GUI, "dropping control\n");

    CamUnitControlWidget *ucw = CAM_UNIT_CONTROL_WIDGET(control_widget);
    CamUnit *unit = ucw->unit;

    if (unit) {
        int original_index = cam_unit_chain_get_unit_index(self->chain, unit);

        dbg(DBG_GUI, "dragging [%s] from %d to %d\n", 
                cam_unit_get_id(unit), original_index, self->insert_position);

        if (original_index == -1) {
            cam_unit_chain_insert_unit (self->chain, unit,
                    self->insert_position);
        }
    }

    gtk_drag_finish(drag, TRUE, TRUE, time);
}

static void
on_unit_added(CamUnitChain *chain, CamUnit *unit, CamUnitChainWidget *self)
{
    dbg(DBG_GUI, "ChainWidget detected new unit [%s] in chain\n", 
            cam_unit_get_id(unit));

    make_widget_for_unit (self, unit);
}

static void
on_unit_removed(CamUnitChain *chain, CamUnit *unit, CamUnitChainWidget *self)
{
    dbg(DBG_GUI, "ChainWidget detected unit [%s] removed from chain\n", 
            cam_unit_get_id(unit));
    
    GList *biter;
    for (biter=self->box->children; biter; biter=biter->next) {
        GtkBoxChild *child = (GtkBoxChild*) biter->data;
        if (! IS_CAM_UNIT_CONTROL_WIDGET (child->widget)) continue;

        CamUnitControlWidget *ucw = CAM_UNIT_CONTROL_WIDGET(child->widget);
        if (ucw->unit == unit) {
            cam_unit_control_widget_detach (ucw);
            gtk_widget_destroy (child->widget);
            return;
        }
    }
}

static void
on_unit_reordered(CamUnitChain *chain, CamUnit *unit, CamUnitChainWidget *self)
{
    dbg(DBG_GUI, "ChainWidget detected unit [%s] reordered\n",
            cam_unit_get_id(unit));
    int newindex = cam_unit_chain_get_unit_index (chain, unit);
    GList *biter;
    for (biter=self->box->children; biter; biter=biter->next) {
        GtkBoxChild *child = (GtkBoxChild*) biter->data;
        if (! IS_CAM_UNIT_CONTROL_WIDGET (child->widget)) continue;

        if (CAM_UNIT_CONTROL_WIDGET(child->widget)->unit == unit) {
            gtk_box_reorder_child (self->box, child->widget, newindex);
            break;
        }
    }
}

static void
on_unit_control_close_button_clicked(CamUnitControlWidget *ucw, 
        CamUnitChainWidget *self)
{
    cam_unit_chain_remove_unit (self->chain, ucw->unit);
}

static void
make_widget_for_unit (CamUnitChainWidget *self, CamUnit *unit)
{
    // create a widget to control the unit
    GtkWidget *ucw = GTK_WIDGET(cam_unit_control_widget_new(unit));

    int position = cam_unit_chain_get_unit_index (self->chain, unit);
    gtk_box_pack_start (self->box, ucw, self->child_expand,
            self->child_fill, self->child_padding);
    gtk_box_reorder_child (self->box, ucw, position);
    gtk_widget_show(ucw);

    g_signal_connect (ucw, "drag-begin", 
            G_CALLBACK(on_unit_control_drag_begin), self);
    g_signal_connect (ucw, "drag-end",
            G_CALLBACK(on_unit_control_drag_end), self);
    g_signal_connect (ucw, "close-button-clicked",
            G_CALLBACK(on_unit_control_close_button_clicked), self);
}

CamUnitControlWidget *
cam_unit_chain_widget_find_unit_by_id (const CamUnitChainWidget * self,
        const char * unit_id)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(self->box));
    GList * iter;

    for (iter = children; iter; iter = iter->next) {
        if (!IS_CAM_UNIT_CONTROL_WIDGET (iter->data))
            continue;

        CamUnitControlWidget * widget = iter->data;
        if (!strcmp (widget->unit->unit_id, unit_id))
            return widget;
    }
    return NULL;
}
