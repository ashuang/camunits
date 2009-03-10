#ifndef __cam_tree_store_h__
#define __cam_tree_store_h__

/*
 * SECTION:cam_tree_store
 * @short_description: TreeStore for CamUnitManagerWidget
 *
 * CamTreeSource is a sub-class of GtkTreeSource that overrides the methods
 * for the GtkTreeDragSource interface so that we can control which rows
 * are draggable and which are not.  We also prevent rows from being deleted
 * after they are dragged.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CAM_TYPE_TREE_STORE  cam_tree_store_get_type()
#define CAM_TREE_STORE(obj)  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    CAM_TYPE_TREE_STORE, CamTreeStore))
#define CAM_TREE_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
    CAM_TYPE_TREE_STORE, CamTreeStoreClass ))
#define IS_CAM_TREE_STORE(obj)   \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAM_TYPE_TREE_STORE ))
#define IS_CAM_TREE_STORE_CLASS(klass)   \
    (G_TYPE_CHECK_CLASS_TYPE( (klass), CAM_TYPE_TREE_STORE))

typedef struct _CamTreeStore CamTreeStore;
typedef struct _CamTreeStoreClass CamTreeStoreClass;

struct _CamTreeStore
{
    GtkTreeStore parent;
};

struct _CamTreeStoreClass
{
    GtkTreeStoreClass parent_class;
};

GType cam_tree_store_get_type(void);

CamTreeStore *cam_tree_store_new (gint n_columns, ...);

/**
 * cam_tree_store_set_draggable_col:
 * @col: The column number of the CamTreeStore which is used to decide
 *       whether or not a row is draggable.
 *
 * This function designates a column of the CamTreeStore to determine
 * whether each row is draggable or not.  The column should have a
 * boolean data type.  If this function is not called, all rows are
 * considered draggable.
 */
void cam_tree_store_set_draggable_col (CamTreeStore * store, gint col);

G_END_DECLS

#endif  /* __cam_tree_store_h__ */
