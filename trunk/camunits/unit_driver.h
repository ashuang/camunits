#ifndef __CamUnitDriver_h__
#define __CamUnitDriver_h__

#include <stdarg.h>
#include <inttypes.h>

#include <glib-object.h>

#include "unit.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:unit_driver
 * @short_description: Factory-like class that instantiates CamUnit objects.
 *
 * CamUnitDriver is a factory-like class that instantiates #CamUnit objects.
 *
 * A CamUnitDriver serves two purposes.  The first is to provide a list of
 * #CamUnitDescription objects that describe the types of #CamUnit objects the
 * CamUnitDriver is able to instantiate.  The second is to instantiate a
 * #CamUnit object, given a #CamUnitDescription.
 *
 * More concretely, it is often the case that multiple input devices of the
 * same type are attached to a computer (e.g. several DC1394 cameras, or
 * several USB V4L2 cameras).  The driver in this case probes the hardware upon
 * startup, enumerates the available options, and then releases hardware
 * resources.  The enumerated options are then converted into
 * #CamUnitDescription objects, which are presented to describe what the driver
 * can instantiate.  The user can then choose one of these descriptions and
 * request that an actual #CamUnit object be created to interface with that
 * specific input device.
 *
 * The CamUnitDriver is useful primarily for input devices, where the steps
 * required to instantiate a #CamUnit may differ depending on the physical
 * device.  Pure software units such as color conversion filters do not have to
 * worry about this, and will typically use a "stock" CamUnitDriver -- see
 * cam_unit_driver_new_stock().
 *
 * Unless you are writing a new input unit, or doing something other than
 * implementing a standard image processing filter, you should not have to deal
 * with this class very much, and cam_unit_driver_new_stock() should be
 * sufficient.  The #CamUnitManager maintains a list of drivers to provide a
 * global view of what's available across all drivers, and the #CamUnitChain
 * should usually be used as a proxy to instantiate new #CamUnit objects.
 */

// =============== CamUnitDescription =============

typedef struct _CamUnitDescription CamUnitDescription;
typedef struct _CamUnitDescriptionClass CamUnitDescriptionClass;
typedef struct _CamUnitDriver CamUnitDriver;

#define CAM_TYPE_UNIT_DESCRIPTION  cam_unit_description_get_type ()
#define CAM_UNIT_DESCRIPTION(obj)  (G_TYPE_CHECK_INSTANCE_CAST ( (obj), \
        CAM_TYPE_UNIT_DESCRIPTION, CamUnitDescription))
#define CAM_UNIT_DESCRIPTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ( (klass), \
            CAM_TYPE_UNIT_DESCRIPTION, CamUnitDescriptionClass))
#define CAM_IS_UNIT_DESCRIPTION(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ( (obj), \
            CAM_TYPE_UNIT_DESCRIPTION))
#define CAM_IS_UNIT_DESCRIPTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE (\
            (klass), CAM_TYPE_UNIT_DESCRIPTION))
#define CAM_UNIT_DESCRIPTION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
            CAM_TYPE_UNIT_DESCRIPTION, CamUnitDescriptionClass))

/**
 * CamUnitDescription:
 * @driver:  the #CamUnitDriver providing this description
 * @name:    a nickname / human-understandable name for the available unit
 * @unit_id: a string identifying the unit
 * @flags:   a bitwise OR of #CamUnitFlags
 *
 * Provides a brief description of an available unit, before that unit
 * is actually instantiated.
 */
struct _CamUnitDescription {
    GInitiallyUnowned parent;
};

struct _CamUnitDescriptionClass {
    GInitiallyUnownedClass parent_class;
};

GType cam_unit_description_get_type (void);

/**
 * cam_unit_description_new:
 *
 * constructor.  creates a new CamUnitDescription.  When the description is no
 * longer needed, free it with g_object_unref ().  You should not use this
 * unless you're implementing a new #CamUnitDriver.
 */
CamUnitDescription *
cam_unit_description_new (CamUnitDriver *driver, const char *name, 
        const char *unit_id, uint32_t flags);

/**
 * cam_unit_description_get_driver:
 *
 * Returns: the driver that can produce the described unit.
 */
CamUnitDriver * cam_unit_description_get_driver(const CamUnitDescription *udesc);

/**
 * cam_unit_description_get_name:
 *
 * Returns: the name of the unit.
 */
const char * cam_unit_description_get_name(const CamUnitDescription *udesc);

/**
 * cam_unit_description_get_unit_id:
 *
 * Returns: the id of the described unit.
 */
const char * cam_unit_description_get_unit_id(const CamUnitDescription *udesc);

/**
 * cam_unit_description_get_flags:
 *
 * Returns: the flags associated with the described unit.
 */
uint32_t cam_unit_description_get_flags(const CamUnitDescription *udesc);

// ================ CamUnitDriver ===============

typedef CamUnit* (*CamUnitConstructor) (void);

typedef struct _CamUnitDriverClass CamUnitDriverClass;

#define CAM_TYPE_UNIT_DRIVER  cam_unit_driver_get_type ()
#define CAM_UNIT_DRIVER(obj)  (G_TYPE_CHECK_INSTANCE_CAST ( (obj), \
        CAM_TYPE_UNIT_DRIVER, CamUnitDriver))
#define CAM_UNIT_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ( (klass), \
            CAM_TYPE_UNIT_DRIVER, CamUnitDriverClass))
#define CAM_IS_UNIT_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ( (obj), \
            CAM_TYPE_UNIT_DRIVER))
#define CAM_IS_UNIT_DRIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE (\
            (klass), CAM_TYPE_UNIT_DRIVER))
#define CAM_UNIT_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
            CAM_TYPE_UNIT_DRIVER, CamUnitDriverClass))

struct _CamUnitDriver {
    GInitiallyUnowned parent;
};

struct _CamUnitDriverClass {
    GInitiallyUnownedClass parent_class;

    int (*start) (CamUnitDriver *self);
    int (*stop) (CamUnitDriver *self);
    CamUnit* (*create_unit) (CamUnitDriver *self, 
            const CamUnitDescription *udesc);
    int (*get_fileno) (CamUnitDriver *self);
    void (*update) (CamUnitDriver *self);
};

GType cam_unit_driver_get_type (void);

/**
 * cam_unit_driver_new_stock:
 * @package:
 * @driver_name:
 * @unit_name:   the name to assign #CamUnit objects instantiated by the
 *               driver.
 * @flags: a bitwise OR of #CamUnitFlags
 * @constructor: Invoking this function should return a newly allocated
 * #CamUnit object.
 *
 * Creates a "stock" unit driver that always has a single unit available with
 * the specified unit id, flags.  Invoking @constructor should always return a
 * new CamUnit.
 *
 * This is useful if you're implementing something like a filter that doesn't
 * need any special driver functionality.
 *
 * Returns: a stock #CamUnitDriver
 */
CamUnitDriver * cam_unit_driver_new_stock (const char *package,
        const char *driver_name,
        const char *unit_name, uint32_t flags, 
        CamUnitConstructor constructor);

CamUnitDriver * cam_unit_driver_new_stock_full (const char *package,
        const char *driver_name,
        const char *unit_name, uint32_t flags, 
        CamUnitConstructor constructor,
        GTypeModule * module);

/**
 * cam_unit_driver_create_unit:
 * @udesc: a description of the unit to instantiate
 *
 * Creates a new instance of a #CamUnit using the provided description.
 *
 * Returns: a newly created #CamUnit object.
 */
CamUnit * cam_unit_driver_create_unit (CamUnitDriver *self,
        const CamUnitDescription * udesc);

/**
 * cam_unit_driver_start:
 *
 * Instructs a driver to do whatever it needs to do to discover available
 * units.  This is most applicable to drivers that need to reserve system
 * resources such as network sockets or file descriptors.  A unit driver is not
 * guaranteed to discover available units until it has been started.
 * 
 * Returns: 0 on success, -1 on failure
 */
int cam_unit_driver_start (CamUnitDriver * self);

/**
 * cam_unit_driver_stop:
 *
 * Instructs a unit driver to stop discovering new units and release whatever
 * resources it used for doing so.
 */
void cam_unit_driver_stop (CamUnitDriver * self);

/**
 * cam_unit_driver_get_package:
 *
 * Returns: the package of the unit driver.  The result is always a valid
 * string.
 */
const char * cam_unit_driver_get_package (const CamUnitDriver *self);

/**
 * cam_unit_driver_get_name:
 *
 * Returns: the name of the unit driver.
 */
const char * cam_unit_driver_get_name (const CamUnitDriver *self);

/**
 * cam_unit_driver_find_unit_description:
 *
 * Looks for a #CamUnitDescription provided by the driver that matches the 
 * specified @unit_id.
 *
 * Returns: the matching #CamUnitDescription, or NULL if no such description 
 * was found
 */
CamUnitDescription* cam_unit_driver_find_unit_description (CamUnitDriver *self, 
        const char *unit_id);

/**
 * cam_unit_driver_get_unit_descriptions:
 *
 * Obtains a list of #CamUnitDescription objects describing all units this
 * driver is able to instantiate.  This list should be released with
 * g_list_free when no longer needed.
 *
 * Returns: a GList of #CamUnitDescription objects
 */
GList* cam_unit_driver_get_unit_descriptions (CamUnitDriver *self);

/**
 * cam_unit_driver_get_fileno:
 *
 * Unit Drivers can optionally implement and provide a file descriptor that
 * can be used to asynchronously notify applications when the driver has a 
 * pending update (e.g. a unit description can be added or removed).  When the
 * file descriptor is readable (e.g. as identified by poll or select or some
 * other means) then the application should call cam_unit_driver_update().
 *
 * If a UnitDriver provides a file descriptor, the descriptor should be
 * available on construction, and must never change.
 *
 * Returns: a nonnegative file descriptor, or -1 to indicate that the unit
 *          driver does not provide a file descriptor.
 */
int cam_unit_driver_get_fileno (CamUnitDriver *self);

/**
 * cam_unit_driver_update:
 *
 * Unit Drivers can optionally implement an update method that checks for new
 * or removed unit descriptions.  This function invokes that method for a
 * specific driver.
 */
void cam_unit_driver_update (CamUnitDriver *self);

// ======== CamUnitDriver protected methods =======

/**
 * cam_unit_driver_add_unit_description:
 *
 * Protected method.  Adds a new unit description to the driver.
 *
 * Returns: the newly added #CamUnitDescription.  It is safe to ignore the
 * return value if you don't need it.
 */
CamUnitDescription* cam_unit_driver_add_unit_description (CamUnitDriver *self,
        const char *name, const char *unit_id, uint32_t flags);

/**
 * cam_unit_driver_remove_unit_description:
 *
 * Protected method.  Removes a unit description from the driver.  Calling
 * this will emit the "unit-description-removed" signal.
 *
 * Returns: 0 on success, -1 on failure (if the specified unit was not found)
 */
int cam_unit_driver_remove_unit_description (CamUnitDriver *self,
        const char *unit_id);

/**
 * cam_unit_driver_set_name:
 *
 * Protected method.  Sets the package and name of the driver.  Subclasses
 * should call this once on instantiation only.
 */
void cam_unit_driver_set_name (CamUnitDriver *self, const char *package,
        const char *name);

#ifdef __cplusplus
}
#endif

#endif
