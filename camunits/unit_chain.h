#ifndef __cam_unit_chain_h__
#define __cam_unit_chain_h__

#include "unit_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:unit_chain
 * @short_description: A sequence of #CamUnit objects that form an image
 * processing chain.
 *
 * CamUnitChain is largely a convenience class for creating and managing a
 * sequence of #CamUnit objects in an image processing chain.  CamUnitChain is
 * optimized for use within a GLib GMainLoop, but it is possible to use a
 * CamUnitChain within other event loops with a little more work.
 *
 * The CamUnitChain handles the tedium of connecting units together,
 * consolidating their file descriptors and timers (for input units) and
 * attaching the units to a GMainLoop.
 */

typedef struct _CamUnitChain CamUnitChain;
typedef struct _CamUnitChainClass CamUnitChainClass;

#define CAM_TYPE_UNIT_CHAIN  cam_unit_chain_get_type()
#define CAM_UNIT_CHAIN(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CAM_TYPE_UNIT_CHAIN, CamUnitChain))
#define CAM_UNIT_CHAIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_UNIT_CHAIN, CamUnitChainClass))
#define CAM_IS_UNIT_CHAIN(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_UNIT_CHAIN))
#define CAM_IS_UNIT_CHAIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE (\
            (klass), CAM_TYPE_UNIT_CHAIN))
#define CAM_UNIT_CHAIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_UNIT_CHAIN, CamUnitChainClass))

GType cam_unit_chain_get_type (void);

// ========= Unit Chain public methods ==========

/**
 * Constructor.  create a new chain
 *
 * Returns: a newly allocated CamUnitChain
 */
CamUnitChain * cam_unit_chain_new (void);

/**
 * cam_unit_chain_get_length:
 * @self: the CamUnitChain
 *
 * Returns: the number of units in the chain
 */
int cam_unit_chain_get_length (const CamUnitChain *self);

/**
 * cam_unit_chain_has_unit:
 * @self: the CamUnitChain
 *
 * Returns: 1 if the unit is in the chain, 0 if not
 */
int cam_unit_chain_has_unit (const CamUnitChain *self, const CamUnit *unit);

/**
 * cam_unit_chain_insert_unit:
 * @self: the CamUnitChain
 * @unit: the CamUnit to insert
 * @position: the position within the chain to place the unit.
 *
 * Inserts a unit into the chain at the specified position.  Also invokes
 * unit_set_input on affected units.  If the chain is streaming
 * (i.e. cam_unit_chain_all_units_stream_init has been called)
 * then all the units after the insertion position
 * will be automatically restarted.  Calls g_object_ref_sink on the unit
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_unit_chain_insert_unit (CamUnitChain *self, CamUnit *unit, 
        int position);

/**
 * cam_unit_chain_insert_unit_tail:
 * @self: the CamUnitChain
 * @unit: the CamUnit to insert
 *
 * Convenience method.  Inserts a unit into the chain as the last unit.
 * calls g_object_ref_sink on unit
 *
 * Returns: 0 on success, < 0 on failure
 */
int cam_unit_chain_insert_unit_tail (CamUnitChain *self, CamUnit *unit);

/**
 * cam_unit_chain_add_unit_by_id:
 * @self: the CamUnitChain
 * @unit_id: ID of the new unit to instantiate and add.
 *
 * Convenience method.  Searches for a unit description with specified ID, 
 * creates a new unit from it, and adds it to the chain.
 *
 * Returns: the newly created unit, or NULL on failure
 */
CamUnit * cam_unit_chain_add_unit_by_id (CamUnitChain *self, 
        const char *unit_id);

/**
 * cam_unit_chain_remove_unit:
 * @self: the CamUnitChain
 * @unit: the CamUnit to remove
 *
 * removes a unit from the chain and decrements the unit's refcount.  Note that
 * this may destroy the unit if the chain is the only object with a reference
 * to the unit.  So if you want the unit to persist after removing it from the
 * chain, then g_object_ref the unit first.
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_unit_chain_remove_unit (CamUnitChain *self, CamUnit *unit);

/**
 * cam_unit_chain_remove_all_units:
 * @self: the CamUnitChain
 *
 * removes all units from the chain.  This is done automatically when the chain
 * is finalized, so only use this if you need to purge the units in the chain
 * and still keep the chain around.
 */
void cam_unit_chain_remove_all_units (CamUnitChain *self);

/**
 * cam_unit_chain_get_last_unit:
 * @self: the CamUnitChain
 *
 * Returns a pointer to the last unit in the chain.  Does not modify the
 * reference count of the unit.
 *
 * Returns: the last unit, or NULL if there are no units in the chain
 */
CamUnit * cam_unit_chain_get_last_unit (const CamUnitChain *self);

/**
 * cam_unit_chain_find_unit_by_id:
 * @self: the CamUnitChain
 * @unit_id: search query
 *
 * Searches for a unit in the chain with the specified id, and returns the
 * first matching unit.  Does not modify the reference count of the unit.
 *
 * Returns: the unit, or NULL if no matching unit was found.
 */
CamUnit * cam_unit_chain_find_unit_by_id (const CamUnitChain *self, 
        const char *unit_id);

/**
 * cam_unit_chain_get_units:
 * @self: the CamUnitChain
 *
 * Returns: a newly allocated list of all the units in the chain.  This list
 * must be released with g_list_free
 */
GList * cam_unit_chain_get_units (const CamUnitChain *self);

/**
 * cam_unit_chain_get_unit_index:
 * @self: the CamUnitChain
 * @unit: the unit to look for
 *
 * Returns: the index of the specified unit within the chain, or -1 if the unit
 * does not belong to the chain.
 */
int cam_unit_chain_get_unit_index (CamUnitChain *self, const CamUnit *unit);

/**
 * cam_unit_chain_reorder_unit:
 * @self: the CamUnitChain
 * @unit: the target CamUnit
 * @new_index: the new position within the chain for the CamUnit.
 *
 * Re-positions a unit within the chain.  
 *
 * Returns: 0 on success, -1 on failure (i.e. the unit does not belong to the
 * chain, or new_index is not a valid index)
 */
int cam_unit_chain_reorder_unit (CamUnitChain *self, CamUnit *unit,
        int new_index);

/**
 * cam_unit_chain_all_units_stream_init:
 * @self: the CamUnitChain
 *
 * Calls cam_unit_stream_init on all non-streaming units in the chain.
 *
 * Returns: NULL on success, or a pointer to the first CamUnit that failed
 *          cam_unit_stream_init
 */
CamUnit * cam_unit_chain_all_units_stream_init (CamUnitChain *self);

/**
 * cam_unit_chain_all_units_stream_shutdown:
 * @self: the CamUnitChain
 *
 * Calls cam_unit_stream_shutdown on all streaming units in the chain.
 *
 * Returns: NULL on success, or a pointer to the first CamUnit that failed
 *          cam_unit_stream_shutdown
 */
CamUnit * cam_unit_chain_all_units_stream_shutdown (CamUnitChain *self);

/**
 * cam_unit_chain_attach_glib:
 * @priority: the GLib event priority to give the event sources in the
 *            CamUnitChain.
 * @context: a GMainContext to which the chain will be attached.  May be NULL,
 *           in which case the default GMainContext is used.
 *
 * Attaches a CamUnitChain to a GLib event context.  Use this method when
 * using a GTK or GLib event loop.  Calling this method also attaches the 
 * chain's unit manager to the GMainContext.
 */
int cam_unit_chain_attach_glib (CamUnitChain *self, int priority,
        GMainContext * context);

/**
 * cam_unit_chain_detach_glib:
 *
 * Detaches a CamUnitChain from its GLib event context.
 */
void cam_unit_chain_detach_glib (CamUnitChain *self);

/**
 * cam_unit_chain_snapshot:
 *
 * Makes a snapshot of the chain state, and returns it in a form suitable for
 * passing to cam_unit_chain_load_from_str()
 *
 * Returns: A newly allocated string
 */
char * cam_unit_chain_snapshot (const CamUnitChain *self);

/**
 * cam_unit_chain_load_from_str:
 * @xml_str: A string generated by cam_unit_chain_snapshot
 * @error: Output variable.  If not NULL and an error occurs while loading the
 *         chain, this will point to a GError describing the failure.
 *
 * Configures the chain according to the layout specified in %xml_str.
 *
 * Implicitly calls cam_unit_stream_init on each newly created unit.
 *
 * Controls for a unit specified in xml_str will be set after the unit has been
 * initialized.
 */
void cam_unit_chain_load_from_str (CamUnitChain *self, const char *xml_str,
        GError **error);

#ifdef __cplusplus
}
#endif

#endif
