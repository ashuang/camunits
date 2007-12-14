#ifndef __CamUnitManager_h__
#define __CamUnitManager_h__

#include <glib.h>
#include <glib-object.h>

#include "unit.h"
#include "unit_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:unit_manager
 * @short_description: Factory-like class that manages a collection of 
 * #CamUnitDriver objects.
 *
 * CamUnitManager is a factory-like class that provide a (hopefully) convenient
 * way to list and instantiate all available units.  A single #CamUnitDriver is
 * responsible only for describing and instantiating units of a specific type,
 * whereas a CamUnitManager attempts to aggregate all of the information
 * provided by all known #CamUnitDriver objects into a single object.
 *
 * Before becoming available for use, a #CamUnitDriver must first be
 * registered with a CamUnitManager.  There are three ways this can happen.
 * First, libcam provides a set of "core" drivers that are always loaded when
 * a CamUnitManager is created.  Second, a CamUnitManager searches the plugin
 * directories (typically /usr/lib/libcam/ and the colon-separated list of
 * directories in the "LIBCAM_PLUGIN_PATH" environment variable) for
 * dynamically loadable plugins.  Finally, it is possible to subclass
 * #CamUnitDriver and register drivers at runtime via 
 * cam_unit_manager_add_driver().
 *
 * In a simple libcam application, there is no need to work directly with the
 * CamUnitManager.  Instead, an simple libcam application may use a
 * #CamUnitChain object, which itself contains a CamUnitManager.  Reasons for
 * using a CamUnitManager directly could be sharing a manager across multiple
 * chains, implementing new #CamUnit and #CamUnitDriver subclasses, or
 * completely foregoing the #CamUnitChain for a non-standard arrangement of
 * #CamUnitChain objects.
 */
typedef struct _CamUnitManager CamUnitManager;
typedef struct _CamUnitManagerClass CamUnitManagerClass;

#define CAM_TYPE_UNIT_MANAGER  cam_unit_manager_get_type()
#define CAM_UNIT_MANAGER(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CAM_TYPE_UNIT_MANAGER, CamUnitManager))
#define CAM_UNIT_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_UNIT_MANAGER, CamUnitManagerClass))
#define CAM_IS_UNIT_MANAGER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_UNIT_MANAGER))
#define CAM_IS_UNIT_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE (\
            (klass), CAM_TYPE_UNIT_MANAGER))
#define CAM_UNIT_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_UNIT_MANAGER, CamUnitManagerClass))

struct _CamUnitManager {
    GObject parent;

    /*< public >*/

    /*< private >*/
    GList *drivers;

    int desired_driver_status;
};

struct _CamUnitManagerClass {
    GObjectClass parent_class;
};

GType cam_unit_manager_get_type (void);

// ========= Unit Manager functions ==========

/**
 * cam_unit_manager_new:
 * @start_drivers: TRUE if the unit manager should automatically call
 *                 #cam_unit_driver_start on unit drivers as they are added to
 *                 the unit manager.  If in doubt, set this to TRUE.
 *
 * Constructor.  Instantiates a new unit manager with a core set of unit
 * drivers already registered.   
 *
 * Returns: a newly allocated CamUnitManager
 */
CamUnitManager * cam_unit_manager_new (gboolean start_drivers);

/**
 * cam_unit_manager_add_driver:
 *
 * Adds a driver to the unit manager.  Only use this method when implementing
 * custom units and unit drivers.  After a unit driver has been added to the
 * manager, the manager can be used to enumerate and instantiate units provided
 * by the driver.
 * 
 * Calls g_object_ref_sink on the driver.
 */
void cam_unit_manager_add_driver (CamUnitManager *self, CamUnitDriver *driver);

/**
 * cam_unit_manager_remove_driver:
 *
 * not yet implemented, do not use.
 */
int cam_unit_manager_remove_driver(CamUnitManager *self, 
        CamUnitDriver *driver);

/**
 * cam_unit_manager_start_drivers:
 *
 * Invokes cam_unit_driver_start on each of the #CamUnitDriver objects
 * registered with the manager.
 *
 * Returns: 0
 */
int cam_unit_manager_start_drivers (CamUnitManager *self);

/**
 * cam_unit_manager_stop_drivers:
 *
 * calls cam_unit_driver_stop on all unit drivers managed by this unit manager.
 * This method is called automatically when the manager is destroyed.
 *
 * Returns: 0
 */
int cam_unit_manager_stop_drivers (CamUnitManager *self);

/**
 * cam_unit_manager_get_drivers:
 *
 * Retrieves a list of all CamUnitDriver objects registered with the manager.
 * This list should be freed with g_list_free when no longer needed
 *
 * Returns: a GList of CamUnitDriver objects
 */
GList *cam_unit_manager_get_drivers(CamUnitManager *self);

/**
 * cam_unit_manager_find_unit_description:
 * @unit_id: a string of the form "driver:id".
 *
 * Searches all known drivers for a unit description matching unit_id.
 *
 * Returns: a matching #CamUnitDescription, or NULL if not found
 */
const CamUnitDescription *
cam_unit_manager_find_unit_description (CamUnitManager *self, 
        const char *unit_id);

/**
 * cam_unit_manager_list_descriptions:
 * @driver_package:  the package to check
 * @recurse:  TRUE to recursively list unit descriptions in subpackages
 *
 * Use this method to list unit descriptions in a specific package.
 * For example, use this method to identify all known input unit descriptions
 * by listing everything in the package "input"
 *
 * Returns: a newly allocated GList of #CamUnitDescription objects, which must
 * be freed with g_list_free when no longer needed.  Do not modify the
 * descriptions within the list.
 */
GList * cam_unit_manager_list_package (CamUnitManager *self,
        const char *driver_package, gboolean recurse);

/**
 * cam_unit_manager_create_unit_by_id:
 *
 * Convenience method to instantiate a unit.
 *
 * Returns: a newly created unit, or NULL
 */
CamUnit * cam_unit_manager_create_unit_by_id (CamUnitManager *self,
        const char *unit_id);

/**
 * cam_unit_manager_add_plugin_dir:
 * @path: target directory containing libcam plugins
 *
 * Causes the unit manager to scan the specified directory for plugins.
 * Discovered plugins will be loaded into the manager.
 */
void cam_unit_manager_add_plugin_dir (CamUnitManager *self, const char *path);

#ifdef __cplusplus
}
#endif

#endif
