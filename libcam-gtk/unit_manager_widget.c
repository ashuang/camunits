#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libcam/dbg.h>
#include "unit_control_widget.h"
#include "unit_manager_widget.h"

#define err(args...) fprintf(stderr, args)

enum {
    UNIT_DESCRIPTION_ACTIVATED_SIGNAL,
    LAST_SIGNAL
};

static guint unit_manager_widget_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_manager_widget_finalize( GObject *obj );
static void on_unit_description_added(CamUnitManager *manager, 
        CamUnitDescription *description, CamUnitManagerWidget *self);
static void on_unit_description_removed(CamUnitManager *manager, 
        CamUnitDescription *description, CamUnitManagerWidget *self);
static void on_row_selected (GtkTreeView *tv, GtkTreePath *path,
        GtkTreeViewColumn *column, void *user_data);

static gboolean button_press (GtkWidget * widget, GdkEventButton * event);

//typedef struct _ManagerWidgetInfo {
//    GtkWidget *widget;
//    GtkWidget *labelval;
//    int maxchars;
//    CamUnitManager *ctl;
//} ManagerWidgetInfo;

G_DEFINE_TYPE (CamUnitManagerWidget, cam_unit_manager_widget, GTK_TYPE_TREE_VIEW);

enum {
    COL_TEXT,
    COL_DESC_PTR,
    N_COLUMNS,
};

GtkTargetEntry cam_unit_manager_widget_target_entry = {
    .target = "UnitManager",
    .flags = GTK_TARGET_SAME_APP,
    .info = CAM_UNIT_MANAGER_WIDGET_DND_ID,
};

static void
cam_unit_manager_widget_init( CamUnitManagerWidget *self )
{
    dbg(DBG_GUI, "unit manager widget constructor\n");

    self->tree_store = gtk_tree_store_new (N_COLUMNS,
            G_TYPE_STRING, G_TYPE_POINTER);
    gtk_tree_view_set_model (GTK_TREE_VIEW (self),
            GTK_TREE_MODEL (self->tree_store));
    GtkCellRenderer * renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn * column = 
        gtk_tree_view_column_new_with_attributes ("Avail. Units",
            renderer, "markup", COL_TEXT, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);
#if 0
    gtk_tree_selection_set_select_function (
            gtk_tree_view_get_selection (GTK_TREE_VIEW (self)),
            selection_func, NULL, NULL);
#endif
    gtk_drag_source_set (GTK_WIDGET (self), GDK_BUTTON1_MASK,
            &cam_unit_manager_widget_target_entry, 1, GDK_ACTION_PRIVATE);
    g_signal_connect (G_OBJECT (self), "row-activated", 
            G_CALLBACK (on_row_selected), self);

    self->manager = NULL;
}

static void drag_begin (GtkWidget * widget, GdkDragContext * context);
static void drag_end (GtkWidget * widget, GdkDragContext * context);

static void
cam_unit_manager_widget_class_init( CamUnitManagerWidgetClass *klass )
{
    dbg(DBG_GUI, "unit manager widget class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    // add a class-specific destructor
    gobject_class->finalize = cam_unit_manager_widget_finalize;

    widget_class->drag_begin = drag_begin;
    widget_class->drag_end = drag_end;
    widget_class->button_press_event = button_press;

    // signals

    /**
     * CamUnitManagerWidget::unit-description-activated
     * @manager_widget: CamUnitManagerWidget emitting the signal
     * @udesc: the CamUnitDescription that was activated
     *
     * The unit-description-activated signal is emitted when the user
     * double-clicks on a unit description
     */
    unit_manager_widget_signals[UNIT_DESCRIPTION_ACTIVATED_SIGNAL] = 
        g_signal_new("unit-description-activated",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            CAM_TYPE_UNIT_DESCRIPTION);
}

// destructor (more or less)
static void
cam_unit_manager_widget_finalize( GObject *obj )
{
    CamUnitManagerWidget *self = CAM_UNIT_MANAGER_WIDGET( obj );
    dbg(DBG_GUI, "unit manager widget finalize\n" );
    dbgl(DBG_REF, "unref manager\n");

    g_object_unref (self->manager);
    g_object_unref (self->tree_store);

    G_OBJECT_CLASS (cam_unit_manager_widget_parent_class)->finalize(obj);
}

CamUnitManagerWidget *
cam_unit_manager_widget_new( CamUnitManager *manager )
{
    CamUnitManagerWidget * self = 
        CAM_UNIT_MANAGER_WIDGET(g_object_new(CAM_TYPE_UNIT_MANAGER_WIDGET, 
                    NULL));
    cam_unit_manager_widget_set_manager( self, manager );
    return self;
}

static int
find_udesc_parent_iter (CamUnitManagerWidget *self, 
        char **levels, int i, GtkTreeIter *parent, GtkTreeIter *result)
{
    GtkTreeModel *model = GTK_TREE_MODEL (self->tree_store);
    if (!levels[i]) {
        *result = *parent;
        return 1;
    }

    GtkTreeIter child;
    gboolean more_children = gtk_tree_model_iter_children (model,
            &child, parent);
    while (more_children) {
        char *child_name = NULL;
        gtk_tree_model_get (model, &child, COL_TEXT, &child_name, -1);
        int is_match = !strcmp (child_name, levels[i]);
        free (child_name);
        if (is_match) {
            return find_udesc_parent_iter (self, levels, i+1, &child, result);
        }
        more_children = gtk_tree_model_iter_next (model, &child);
    }
    return 0;
}

static int
remove_description (CamUnitManagerWidget *self, CamUnitDescription *desc)
{
    // find the parent node
    GtkTreeIter parent;
    char **driver_and_id = g_strsplit (desc->unit_id, ":", 2);
    char **levels = g_strsplit (driver_and_id[0], ".", 0);
    g_strfreev (driver_and_id);
    int found_parent = find_udesc_parent_iter (self, levels, 0, NULL, &parent);
    g_strfreev (levels);

    assert (found_parent);

    // find and remove the child node corresponding to the unit description
    GtkTreeModel *model = GTK_TREE_MODEL (self->tree_store);
    GtkTreeIter iter;
    int found_iter = 0;
    for (gboolean more_children = 
            gtk_tree_model_iter_children (model, &iter, &parent);
            more_children;
            more_children = gtk_tree_model_iter_next (model, &iter)) {
        CamUnitDescription *qdesc = NULL;
        gtk_tree_model_get (model, &iter, COL_DESC_PTR, &qdesc, -1);

        if (qdesc == desc) {
            gtk_tree_store_remove (self->tree_store, &iter);
            found_iter = 1;
            break;
        }
    }

    // if the removal of the description resulted in a childless parent, then 
    // remove the parent from the tree store.
    while (! gtk_tree_model_iter_has_child (model, &parent)) {
        GtkTreeIter parent_parent;
        gboolean has_parent = gtk_tree_model_iter_parent (model, &parent_parent,
                &parent);
        gtk_tree_store_remove (self->tree_store, &parent);
        if (has_parent) {
            parent = parent_parent;
        } else {
            break;
        }
    }

    return -1;
}

static int
find_or_make_parent_iter (CamUnitManagerWidget * self, 
        char **levels, int i, GtkTreeIter * parent, GtkTreeIter * result)
{
    GtkTreeModel *model = GTK_TREE_MODEL (self->tree_store);

    if (!levels[i]) return 0;

    GtkTreeIter child;
    gboolean more_children = gtk_tree_model_iter_children (model,
            &child, parent);
    while (more_children) {
        char *child_name = NULL;
        gtk_tree_model_get (model, &child, COL_TEXT, &child_name, -1);
        int is_parent = !strcmp (child_name, levels[i]);
        free (child_name);
        if (is_parent) {
            *result = child;
            find_or_make_parent_iter (self, levels, i+1, &child, result);
            return 1;
        }
        more_children = gtk_tree_model_iter_next (model, &child);
    }

    gtk_tree_store_append (self->tree_store, result, parent);
    gtk_tree_store_set (self->tree_store, result, 
            COL_TEXT, levels[i], 
            COL_DESC_PTR, NULL, 
            -1);
    find_or_make_parent_iter (self, levels, i+1, result, result);
    return 1;
}

static int
add_description (CamUnitManagerWidget * self, CamUnitDescription * desc)
{
    GtkTreeIter iter, iter2, parent_iter;

    char **driver_and_id = g_strsplit (desc->unit_id, ":", 2);
    char **levels = g_strsplit (driver_and_id[0], ".", 0);
    g_strfreev (driver_and_id);
    int found_parent = 
        find_or_make_parent_iter (self, levels, 0, NULL, &parent_iter);
    g_strfreev (levels);

    if (found_parent) {
        gtk_tree_store_append (self->tree_store, &iter, &parent_iter);
    } else {
        gtk_tree_store_append (self->tree_store, &iter, NULL);
    }
    gtk_tree_store_set (self->tree_store, &iter,
            COL_TEXT, desc->name,
            COL_DESC_PTR, desc,
            -1);
    GtkTreePath * path =
        gtk_tree_model_get_path (GTK_TREE_MODEL (self->tree_store), &iter);
    gtk_tree_view_expand_to_path (GTK_TREE_VIEW (self), path);
    gtk_tree_path_free (path);

//    gtk_tree_store_append (self->tree_store, &iter2, &iter);
//    sprintf (str, "<small><tt><b>Driver:  </b>%s</tt></small>", 
//            desc->driver->package);
//    gtk_tree_store_set (self->tree_store, &iter2, COL_TEXT, str, -1);
//
#if 0
    gtk_tree_store_append (self->tree_store, &iter2, &iter);
    char *idstr = g_strdup_printf ("<small><tt><b>ID:      </b>%s</tt></small>",
            desc->unit_id);
    gtk_tree_store_set (self->tree_store, &iter2, 
            COL_TEXT, idstr, 
            COL_DESC_PTR, desc,
            -1);
    free (idstr);
#endif
    return 0;
}

static int
add_driver (CamUnitManagerWidget * self, CamUnitDriver * driver)
{
    GList * descs = cam_unit_driver_get_unit_descriptions (driver);
    GList * diter;
    for (diter = descs; diter; diter = diter->next)
        add_description (self, CAM_UNIT_DESCRIPTION (diter->data));

    g_list_free (descs);
    return 0;
}

int
cam_unit_manager_widget_set_manager(CamUnitManagerWidget *self, 
        CamUnitManager *manager)
{
    if( self->manager ) return -1;
    self->manager = manager;

    dbgl(DBG_REF, "ref manager\n");
    g_object_ref(manager);
    g_signal_connect(G_OBJECT(manager), "unit-description-added", 
            G_CALLBACK(on_unit_description_added), self);
    g_signal_connect(G_OBJECT(manager), "unit-description-removed", 
            G_CALLBACK(on_unit_description_removed), self);

    GList *drivers = cam_unit_manager_get_drivers( self->manager );
    GList *diter;
    for( diter=drivers; diter; diter=diter->next )
        add_driver (self, CAM_UNIT_DRIVER (diter->data));

    g_list_free( drivers );

    return 0;
}

static void
on_unit_description_added (CamUnitManager * manager,
        CamUnitDescription * desc, CamUnitManagerWidget * self)
{
    add_description (self, desc);
}

static void
on_unit_description_removed (CamUnitManager * manager,
        CamUnitDescription * desc, CamUnitManagerWidget * self)
{
    remove_description (self, desc);
}

static gboolean
button_press (GtkWidget * widget, GdkEventButton * event)
{
    CamUnitManagerWidget *self = CAM_UNIT_MANAGER_WIDGET (widget);
    self->press_x = event->x;
    self->press_y = event->y;

    GtkWidgetClass *wclass = 
        GTK_WIDGET_CLASS (cam_unit_manager_widget_parent_class);
    wclass->button_press_event ( widget, event);

    return TRUE;
}

static void
drag_begin (GtkWidget * widget, GdkDragContext * context)
{
    GtkTreePath * path = NULL;
    gint cell_x, cell_y;
    CamUnitManagerWidget *self = CAM_UNIT_MANAGER_WIDGET (widget);

    if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (self),
            self->press_x, self->press_y,
            &path, NULL, &cell_x, &cell_y))
        return;

//    if (gtk_tree_path_get_depth (path) > 1)
//        gtk_tree_path_up (path);

    GtkTreeIter iter;
    gtk_tree_model_get_iter (GTK_TREE_MODEL (self->tree_store), &iter, path);
    gtk_tree_path_free (path);

    CamUnitDescription * desc;
    gtk_tree_model_get (GTK_TREE_MODEL (self->tree_store), &iter,
            COL_DESC_PTR, &desc, -1);
    if (!desc) return;

    CamUnit * unit = cam_unit_driver_create_unit (desc->driver, desc);
    if (!unit)
        return;

    GtkWidget * unit_widget = GTK_WIDGET (cam_unit_control_widget_new (unit));

    gtk_widget_set_size_request (unit_widget, 250, -1);

    GtkWidget * drag_window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_container_add (GTK_CONTAINER (drag_window), unit_widget);
    gtk_widget_show (unit_widget);

    gtk_drag_set_icon_widget (context, drag_window, cell_x, cell_y);

    g_object_set_data (G_OBJECT (widget), "cam-unit-control-widget",
            unit_widget);
}

static void
drag_end (GtkWidget * widget, GdkDragContext * context)
{
    GtkWidget * control_widget = g_object_get_data (G_OBJECT (widget),
            "cam-unit-control-widget");
    if (!control_widget || !IS_CAM_UNIT_CONTROL_WIDGET (control_widget))
        return;
    GtkWidget *parent = gtk_widget_get_parent (control_widget);
	dbg(DBG_GUI, "manager drag end\n");
    gtk_widget_destroy (parent);
}

static void 
on_row_selected (GtkTreeView *tv, GtkTreePath *path,
        GtkTreeViewColumn *column, void *user_data)
{
    CamUnitManagerWidget *self = CAM_UNIT_MANAGER_WIDGET (user_data);
    GtkTreeIter iter;
    gtk_tree_model_get_iter (GTK_TREE_MODEL (self->tree_store), &iter, path);
    CamUnitDescription *udesc = NULL;
    gtk_tree_model_get (GTK_TREE_MODEL (self->tree_store), &iter, 
            COL_DESC_PTR, &udesc, -1);
    if (!udesc) return;
    g_signal_emit (G_OBJECT(self), 
            unit_manager_widget_signals[UNIT_DESCRIPTION_ACTIVATED_SIGNAL], 0,
            udesc);
}
