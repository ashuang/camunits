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

typedef struct _CamUnitChainSource CamUnitChainSource;

struct _CamUnitChain {
    GObject parent;

    CamUnitManager *manager;

    /*< private >*/
    GSourceFuncs source_funcs;
    CamUnitChainSource * event_source;

    GList *units;

    /*
     * link within units that points to the next unit in the chain ready to 
     * generate frames.
     */
    GList *pending_unit_link;

    CamUnitStatus desired_unit_status;
};

struct _CamUnitChainClass {
    GObjectClass parent_class;

    /*< private >*/
};

GType cam_unit_chain_get_type (void);

// ========= Unit Chain public methods ==========

/**
 * constructor.  create a new chain with a new unit manager
 */
CamUnitChain * cam_unit_chain_new (void);

/**
 * cam_unit_chain_new_with_manager:
 *
 * constructor.  create a new chain with an existing manager.  on return,
 * the reference count on manager is incremeneted.
 */
CamUnitChain * cam_unit_chain_new_with_manager (CamUnitManager *manager);

/**
 * cam_unit_chain_get_manager:
 *
 * Returns: the unit manager associated with the chain
 */
CamUnitManager * cam_unit_chain_get_manager (CamUnitChain *chain);

/**
 * cam_unit_chain_get_length:
 *
 * Returns: the number of units in the chain
 */
int cam_unit_chain_get_length (const CamUnitChain *self);

/**
 * cam_unit_chain_has_unit:
 *
 * Returns: 1 if the unit is in the chain, 0 if not
 */
int cam_unit_chain_has_unit (const CamUnitChain *self, const CamUnit *unit);

/**
 * cam_unit_chain_insert_unit:
 * @unit: the CamUnit to insert
 * @position: the position within the chain to place the unit.
 *
 * Inserts a unit into the chain at the specified position.  Also invokes
 * unit_set_input on affected units.  If the chain is streaming
 * (i.e. cam_unit_chain_set_desired_status has been called with
 * CAM_UNIT_STATUS_STREAMING), then all the units after the insertion position
 * will be automatically restarted.  Calls g_object_ref_sink on the unit
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_unit_chain_insert_unit (CamUnitChain *self, CamUnit *unit, 
        int position);

/**
 * cam_unit_chain_insert_unit_tail:
 *
 * Convenience method.  Inserts a unit into the chain as the last unit.
 * calls g_object_ref_sink on unit
 *
 * Returns: 0 on success, < 0 on failure
 */
int cam_unit_chain_insert_unit_tail (CamUnitChain *self, CamUnit *unit);

/**
 * cam_unit_chain_add_unit_by_id:
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
 *
 * removes all units from the chain.  This is done automatically when the chain
 * is finalized, so only use this if you need to purge the units in the chain
 * and still keep the chain around.
 */
void cam_unit_chain_remove_all_units (CamUnitChain *self);

/**
 * cam_unit_chain_get_last_unit:
 *
 * Returns a pointer to the last unit in the chain.  Does not modify the
 * reference count of the unit.
 *
 * Returns: the last unit, or NULL if there are no units in the chain
 */
CamUnit * cam_unit_chain_get_last_unit (const CamUnitChain *self);

/**
 * cam_unit_chain_find_unit_by_id:
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
 *
 * Returns: a newly allocated list of all the units in the chain.  This list
 * must be released with g_list_free
 */
GList * cam_unit_chain_get_units (const CamUnitChain *self);

/**
 * cam_unit_chain_get_unit_index:
 *
 * Returns: the index of the specified unit within the chain, or -1 if the unit
 * does not belong to the chain.
 */
int cam_unit_chain_get_unit_index (CamUnitChain *self, const CamUnit *unit);

/**
 * cam_unit_chain_reorder_unit:
 *
 * Re-positions a unit within the chain.  
 *
 * Returns: 0 on success, -1 on failure (i.e. the unit does not belong to the
 * chain, or new_index is not a valid index)
 */
int cam_unit_chain_reorder_unit (CamUnitChain *self, CamUnit *unit,
        int new_index);

/**
 * cam_unit_chain_set_desired_status:
 *
 * Sets the desired status for every unit in the chain.  Upon calling this, the
 * chain will attempt to call stream_{init_any_format,on,off,shutdown} on each
 * unit if necessary.  When new units are added to the chain, the chain will
 * also do this.  If the status of a unit changes on its own, the chain will
 * not try to force the unit back to the desired status.
 *
 * Returns: 0 if status is valid, -1 if status is not one of
 *          CAM_UNIT_STATUS_IDLE, CAM_UNIT_STATUS_READY, or
 *          CAM_UNIT_STATUS_STREAMING
 */
int cam_unit_chain_set_desired_status (CamUnitChain *self, 
        CamUnitStatus status);

CamUnitStatus cam_unit_chain_get_desired_status(const CamUnitChain *self);

/**
 * cam_unit_chain_are_all_units_status:
 *
 * Convenience function.  Checks to see if all units in the chain have the 
 * specified status.
 *
 * Returns: a pointer to the first unit that does not have status %status, or
 * NULL if all units have the specified status.
 */
CamUnit * cam_unit_chain_check_status_all_units (const CamUnitChain *self,
        CamUnitStatus status);

int cam_unit_chain_attach_glib (CamUnitChain *self, int priority,
        GMainContext * context);

void cam_unit_chain_detach_glib (CamUnitChain *self);

#ifdef __cplusplus
}
#endif

#endif
