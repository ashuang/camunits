/*
 * CamTreeSource is a sub-class of GtkTreeSource that overrides the methods
 * for the GtkTreeDragSource interface so that we can control which rows
 * are draggable and which are not.  We also prevent rows from being deleted
 * after they are dragged.
 */

#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include "cam_tree_store.h"

typedef struct _CamTreeStorePriv CamTreeStorePriv;
struct _CamTreeStorePriv {
    gint draggable_col;
};
#define CAM_TREE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_TREE_STORE, CamTreeStorePriv))

/* DND interfaces */
static gboolean real_cam_tree_store_row_draggable   (GtkTreeDragSource *drag_source,
						   GtkTreePath       *path);
static gboolean cam_tree_store_drag_data_delete   (GtkTreeDragSource *drag_source,
						   GtkTreePath       *path);
static gboolean cam_tree_store_drag_data_get      (GtkTreeDragSource *drag_source,
						   GtkTreePath       *path,
						   GtkSelectionData  *selection_data);

static void
cam_tree_store_drag_source_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (CamTreeStore, cam_tree_store, GTK_TYPE_TREE_STORE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						cam_tree_store_drag_source_init))

static void
cam_tree_store_finalize (GObject *object)
{
    /* must chain up */
    G_OBJECT_CLASS (cam_tree_store_parent_class)->finalize (object);
}

static void
cam_tree_store_class_init (CamTreeStoreClass *class)
{
    GObjectClass *object_class;

    object_class = (GObjectClass *) class;

    object_class->finalize = cam_tree_store_finalize;

    g_type_class_add_private (object_class, sizeof (CamTreeStorePriv));
}

static void
cam_tree_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
    iface->row_draggable = real_cam_tree_store_row_draggable;
    iface->drag_data_delete = cam_tree_store_drag_data_delete;
    iface->drag_data_get = cam_tree_store_drag_data_get;
}

static void
cam_tree_store_init (CamTreeStore *tree_store)
{
    CamTreeStorePriv * priv = CAM_TREE_STORE_GET_PRIVATE(tree_store);
    priv->draggable_col = -1;
}

/**
 * cam_tree_store_new:
 * @n_columns: number of columns in the tree store
 * @Varargs: all #GType types for the columns, from first to last
 *
 * Creates a new tree store as with @n_columns columns each of the types passed
 * in.  Note that only types derived from standard GObject fundamental types 
 * are supported. 
 *
 * As an example, <literal>cam_tree_store_new (3, G_TYPE_INT, G_TYPE_STRING,
 * GDK_TYPE_PIXBUF);</literal> will create a new #GtkTreeStore with three columns, of type
 * <type>int</type>, <type>string</type> and #GdkPixbuf respectively.
 *
 * Return value: a new #GtkTreeStore
 **/
CamTreeStore *
cam_tree_store_new (gint n_columns,
			       ...)
{
    CamTreeStore *retval;
    va_list args;
    gint i;

    g_return_val_if_fail (n_columns > 0, NULL);

    retval = g_object_new (CAM_TYPE_TREE_STORE, NULL);
    GType types[n_columns];

    va_start (args, n_columns);
    for (i = 0; i < n_columns; i++)
        types[i] = va_arg (args, GType);
    va_end (args);
    gtk_tree_store_set_column_types (GTK_TREE_STORE (retval), n_columns, types);

    return retval;
}

void
cam_tree_store_set_draggable_col (CamTreeStore * store, gint col)
{
    g_return_if_fail (col >= 0 &&
            col < gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)));
    CamTreeStorePriv * priv = CAM_TREE_STORE_GET_PRIVATE(store);
    priv->draggable_col = col;
}

/* DND */


static gboolean real_cam_tree_store_row_draggable (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path)
{
    CamTreeStore * model = CAM_TREE_STORE (drag_source);
    CamTreeStorePriv * priv = CAM_TREE_STORE_GET_PRIVATE(model);
    if (priv->draggable_col < 0)
        return TRUE;

    GtkTreeIter iter;
    gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);

    gboolean val;
    gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
            priv->draggable_col, &val, -1);
    return val;
}
               
static gboolean
cam_tree_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                 GtkTreePath       *path)
{
    return FALSE;
}

static gboolean
cam_tree_store_drag_data_get (GtkTreeDragSource *drag_source,
                              GtkTreePath       *path,
                              GtkSelectionData  *selection_data)
{
    /* Note that we don't need to handle the GTK_TREE_MODEL_ROW
     * target, because the default handler does it for us, but
     * we do anyway for the convenience of someone maybe overriding the
     * default handler.
     */

    if (gtk_tree_set_row_drag_data (selection_data,
                GTK_TREE_MODEL (drag_source),
                path))
    {
        return TRUE;
    }
    else
    {
        /* FIXME handle text targets at least. */
    }

    return FALSE;
}
