#ifndef __CamPlugin_h__
#define __CamPlugin_h__

#include <glib.h>
#include <glib-object.h>
#include "unit_driver.h"
#include "unit_manager.h"

#ifdef __cplusplus
extern "C" {
#endif


#define CAM_PLUGIN_TYPE(TN, t_n, T_P)			    CAM_PLUGIN_TYPE_EXTENDED (TN, t_n, T_P, 0, {})
#define CAM_PLUGIN_TYPE_WITH_CODE(TN, t_n, T_P, _C_)	    _CAM_PLUGIN_TYPE_EXTENDED_BEGIN (TN, t_n, T_P, 0) {_C_;} _CAM_PLUGIN_TYPE_EXTENDED_END(t_n)
#define CAM_PLUGIN_ABSTRACT_TYPE(TN, t_n, T_P)		    CAM_PLUGIN_TYPE_EXTENDED (TN, t_n, T_P, G_TYPE_FLAG_ABSTRACT, {})
#define CAM_PLUGIN_ABSTRACT_TYPE_WITH_CODE(TN, t_n, T_P, _C_) _CAM_PLUGIN_TYPE_EXTENDED_BEGIN (TN, t_n, T_P, G_TYPE_FLAG_ABSTRACT) {_C_;} _CAM_PLUGIN_TYPE_EXTENDED_END(t_n)
#define CAM_PLUGIN_TYPE_EXTENDED(TN, t_n, T_P, _f_, _C_)	    _CAM_PLUGIN_TYPE_EXTENDED_BEGIN (TN, t_n, T_P, _f_) {_C_;} _CAM_PLUGIN_TYPE_EXTENDED_END(t_n)

/* convenience macro to ease interface addition in the CODE
 * section of CAM_PLUGIN_TYPE_WITH_CODE() (this macro relies on
 * the g_define_type_id present within CAM_PLUGIN_TYPE_WITH_CODE()).
 * usage example:
 * CAM_PLUGIN_TYPE_WITH_CODE (GtkTreeStore, gtk_tree_store, G_TYPE_OBJECT,
 *                          G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
 *                                                 gtk_tree_store_tree_model_init));
 */
#define CAM_PLUGIN_INTERFACE(TYPE_IFACE, iface_init)       { \
  const GInterfaceInfo g_implement_interface_info = { \
    (GInterfaceInitFunc) iface_init, NULL, NULL \
  }; \
  g_type_module_add_interface (module, g_define_type_id, TYPE_IFACE, &g_implement_interface_info); \
}

#define _CAM_PLUGIN_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, flags) \
\
static void     type_name##_init              (TypeName        *self); \
static void     type_name##_class_init        (TypeName##Class *klass); \
static gpointer type_name##_parent_class = NULL; \
static GType    type_name##_type_id = 0; \
static void     type_name##_class_intern_init (gpointer klass) \
{ \
  type_name##_parent_class = g_type_class_peek_parent (klass); \
  type_name##_class_init ((TypeName##Class*) klass); \
} \
\
static void \
type_name##_register_type (GTypeModule * module) \
{ \
  static const GTypeInfo info = { \
    sizeof (TypeName##Class), \
    (GBaseInitFunc) NULL, \
    (GBaseFinalizeFunc) NULL, \
    (GClassInitFunc) type_name##_class_intern_init, \
    (GClassFinalizeFunc) NULL, \
    NULL, \
    sizeof (TypeName), \
    0, \
    (GInstanceInitFunc) type_name##_init, \
  }; \
  type_name##_type_id = g_type_module_register_type (module, \
          TYPE_PARENT, \
          g_intern_static_string (#TypeName), \
          &info, \
          (GTypeFlags) flags); \
  GType g_define_type_id = type_name##_type_id; \
  (void) g_define_type_id; \
      { /* custom code follows */
#define _CAM_PLUGIN_TYPE_EXTENDED_END(type_name)	\
        /* following custom code */	\
      }					\
} /* closes type_name##_get_type() */ \
\
GType \
type_name##_get_type (void) \
{ \
    return type_name##_type_id; \
}

CamUnitDriver *
cam_plugin_unit_driver_create (const char * filename);

#ifdef __cplusplus
}
#endif

#endif
