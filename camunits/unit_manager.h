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
 * CamUnitManager is a factory-like class that provide a 
 * way to list and instantiate all available units.  A single #CamUnitDriver is
 * responsible only for describing and instantiating units of a specific type,
 * whereas a CamUnitManager attempts to aggregate all of the information
 * provided by all known #CamUnitDriver objects into a single object.
 *
 * Before becoming available for use, a #CamUnitDriver must first be
 * registered with a CamUnitManager.  There are two recommended ways for this
 * tohappen.
 * First, Camunits provides a set of "core" drivers that are always loaded
 * when
 * a CamUnitManager is created.  Second, a CamUnitManager searches the plugin
 * directories (typically /usr/lib/camunits/ and the colon-separated list of
 * directories in the "CAMUNITS_PLUGIN_PATH" environment variable) for
 * dynamically loadable plugins.  
 *
 * In a simple Camunits application, there is no need to work directly with
 * the
 * CamUnitManager.  Instead, a simple Camunits application may use a
 * #CamUnitChain object, which itself uses a CamUnitManager.
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

GType cam_unit_manager_get_type (void);

// ========= Unit Manager functions ==========

/**
 * cam_unit_manager_get_and_ref:
 * @start_drivers: TRUE if the unit manager should automatically call
 *                 #cam_unit_driver_start on unit drivers as they are added to
 *                 the unit manager.  If in doubt, set this to TRUE.
 *
 * Singleton accessor.  Retrieves a pointer to the singleton instance of the
 * CamUnitManager, creating it and starting the drivers if necessary.  The
 * reference count on the instance is incremented on return.  If the reference
 * count ever reaches zero (decremented by g_object_unref), then the instance
 * is destroyed, but can be re-instantiated by another call to this function.
 *
 * Returns: a pointer to the singleton CamUnitManager, and increments the
 * reference count on it.
 */
CamUnitManager * cam_unit_manager_get_and_ref (void);

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
 * Searches all known drivers for a unit description matching unit_id.  The
 * reference count of the returned #CamUnitDescription, if any, is not
 * modified.
 *
 * Returns: a matching #CamUnitDescription, or NULL if not found
 */
const CamUnitDescription *
cam_unit_manager_find_unit_description (CamUnitManager *self, 
        const char *unit_id);

/**
 * cam_unit_manager_list_package:
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
 * @unit_id: ID of the unit to instantiate
 *
 * Convenience method to instantiate a unit.
 *
 * Returns: a newly created unit, or NULL
 */
CamUnit * cam_unit_manager_create_unit_by_id (CamUnitManager *self,
        const char *unit_id);

/**
 * cam_unit_manager_add_plugin_dir:
 * @path: target directory containing Camunits plugins
 *
 * Causes the unit manager to scan the specified directory for plugins.
 * Discovered plugins will be loaded into the manager.
 */
void cam_unit_manager_add_plugin_dir (CamUnitManager *self, const char *path);

/**
 * cam_unit_manager_attach_glib:
 * @priority: the GLib event priority to give the event sources in the
 *            CamUnitManager.
 * @context: a GMainContext to which the manager will be attached.  May be
 *           NULL, in which case the default GMainContext is used.
 *
 * Attaches a CamUnitManager to a GLib event context.  When attached to a
 * running GMainContext/GMainLoop, the unit manager will self-update
 * when necessary (i.e. monitor the file descriptors provided by each unit
 * driver, and call cam_unit_manager_update when a unit driver has a pending
 * update).
 *
 * If already attached to a GMainContext, this method will first detach from 
 * the old context.
 */
void cam_unit_manager_attach_glib (CamUnitManager *self, int priority,
        GMainContext * context);

/**
 * cam_unit_manager_detach_glib:
 *
 * Detaches a CamUnitManager from a GLib event context.
 *
 * This method is idempotent.
 */
void cam_unit_manager_detach_glib (CamUnitManager *self);

/**
 * cam_unit_manager_update:
 *
 * Convenience function to call cam_unit_driver_update() on all the drivers
 * managed by this UnitManager.
 */
void cam_unit_manager_update (CamUnitManager *self);

#ifdef __cplusplus
}
#endif

#endif
