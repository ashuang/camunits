#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unit_driver.h"

#include "dbg.h"

#define err(args...) fprintf (stderr, args)

typedef struct _CamUnitDriverPriv CamUnitDriverPriv;
struct _CamUnitDriverPriv {
    char *package;
    char *name;

    /*< private >*/
    GList *udescs;

    CamUnitConstructor stock_constructor;
    uint32_t stock_flags;
    char *stock_unit_name;
    GTypeModule * stock_module;
};
#define CAM_UNIT_DRIVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_UNIT_DRIVER, CamUnitDriverPriv))

typedef struct _CamUnitDescriptionPriv CamUnitDescriptionPriv;
struct _CamUnitDescriptionPriv {
    /*< public >*/
    CamUnitDriver * driver;
    char * name;
    char * unit_id;
    uint32_t flags;
    /*< private >*/
};
#define CAM_UNIT_DESCRIPTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_UNIT_DESCRIPTION, CamUnitDescriptionPriv))

// ================= CamUnitDescription ==================

static void cam_unit_description_finalize (GObject *obj);
static CamUnit * cam_unit_driver_default_create_unit (CamUnitDriver *self,
        const CamUnitDescription * udesc);

G_DEFINE_TYPE (CamUnitDescription, cam_unit_description, 
        G_TYPE_INITIALLY_UNOWNED);

static void
cam_unit_description_init (CamUnitDescription *self)
{
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(self);
    priv->driver = NULL;
    priv->name = NULL;
    priv->unit_id = NULL;
    priv->flags = 0;
}

static void
cam_unit_description_class_init (CamUnitDescriptionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_description_finalize;
    g_type_class_add_private (gobject_class, sizeof (CamUnitDescriptionPriv));
}

CamUnitDescription *
cam_unit_description_new (CamUnitDriver *driver, const char *name, 
        const char *id, uint32_t flags)
{
    CamUnitDescription *self = CAM_UNIT_DESCRIPTION(
        g_object_new (CAM_TYPE_UNIT_DESCRIPTION, NULL));
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(self);
    priv->driver = driver;
    priv->name = strdup (name);
    const char *driver_name = cam_unit_driver_get_name(driver);
    const char *package = cam_unit_driver_get_package(driver);
    priv->unit_id = g_strdup_printf ("%s%s%s%s%s",
            package,
            strlen(package) ? "." : "",
            driver_name, id ? ":" : "", id ? id : "");
    priv->flags = flags;
    return self;
}

static void
cam_unit_description_finalize (GObject *obj)
{
    CamUnitDescription *self = CAM_UNIT_DESCRIPTION (obj);
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(self);
    dbg (DBG_DRIVER, "finalize unit description [%s]\n", priv->unit_id);

    free(priv->name);
    free(priv->unit_id);

    G_OBJECT_CLASS (cam_unit_description_parent_class)->finalize (obj);
}

CamUnitDriver * 
cam_unit_description_get_driver(const CamUnitDescription *udesc)
{
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(udesc);
    return priv->driver;
}

/**
 * cam_unit_description_get_name:
 *
 * Returns: the name of the unit.
 */
const char * 
cam_unit_description_get_name(const CamUnitDescription *udesc)
{
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(udesc);
    return priv->name;
}

/**
 * cam_unit_description_get_unit_id:
 *
 * Returns: the id of the described unit.
 */
const char * 
cam_unit_description_get_unit_id(const CamUnitDescription *udesc)
{
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(udesc);
    return priv->unit_id;
}

/**
 * cam_unit_description_get_flags:
 *
 * Returns: the flags associated with the described unit.
 */
uint32_t 
cam_unit_description_get_flags(const CamUnitDescription *udesc)
{
    CamUnitDescriptionPriv *priv = CAM_UNIT_DESCRIPTION_GET_PRIVATE(udesc);
    return priv->flags;
}

// ================= CamUnitDriver ===================

enum {
    UNIT_DESCRIPTION_ADDED_SIGNAL,
    UNIT_DESCRIPTION_REMOVED_SIGNAL,
    LAST_SIGNAL
};

static guint cam_unit_driver_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_driver_finalize (GObject *obj);
static int cam_unit_driver_default_start (CamUnitDriver *self);
static int cam_unit_driver_default_stop (CamUnitDriver *self);
static int cam_unit_driver_default_get_fileno (CamUnitDriver *self);
static void cam_unit_driver_default_update (CamUnitDriver *self);

G_DEFINE_TYPE (CamUnitDriver, cam_unit_driver, G_TYPE_INITIALLY_UNOWNED);

static void
cam_unit_driver_init (CamUnitDriver *self)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    priv->package = NULL;
    priv->udescs = NULL;
    priv->stock_unit_name = NULL;
    priv->stock_flags = 0;
    priv->stock_constructor = NULL;
}

static void
cam_unit_driver_class_init (CamUnitDriverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_driver_finalize;

    klass->start = cam_unit_driver_default_start;
    klass->stop = cam_unit_driver_default_stop;
    klass->get_fileno = cam_unit_driver_default_get_fileno;
    klass->update = cam_unit_driver_default_update;

    g_type_class_add_private (gobject_class, sizeof (CamUnitDriverPriv));

    // pure virtual
    klass->create_unit = cam_unit_driver_default_create_unit;

    /**
     * CamUnitDriver::unit-description-added
     * @driver: the CamUnitDriver emitting the signal
     * @udesc: the CamUnitDescription just added
     *
     * The unit-description-added signal is emitted when a CamUnitDriver
     * adds a CamUnitDescription to its list of available units.
     */
    cam_unit_driver_signals[UNIT_DESCRIPTION_ADDED_SIGNAL] = 
        g_signal_new ("unit-description-added",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_DESCRIPTION);

    /**
     * CamUnitDriver::unit-description-removed
     * @driver: the CamUnitDriver emitting the signal
     * @udesc: the CamUnitDescription being removed
     *
     * The unit-description-removed signal is emitted when a CamUnitDriver
     * removes a CamUnitDescription from its list of available units.
     *
     * %udesc is guaranteed to be valid through the duration of the signal
     * handlers, but is not guaranteed to be valid afterwards.  Thus, signal
     * handlers should not retain references to %udesc.
     */
    cam_unit_driver_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL] = 
        g_signal_new ("unit-description-removed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_DESCRIPTION);
}

static void
cam_unit_driver_finalize (GObject *obj)
{
    CamUnitDriver *self = CAM_UNIT_DRIVER (obj);
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);

    dbg (DBG_DRIVER, "CamUnitDriver finalize [%s.%s]\n", priv->package, 
            priv->name);

    free (priv->stock_unit_name);
    free (priv->package);
    free (priv->name);
    GList *ucopy = g_list_copy (priv->udescs);
    GList *uditer;
    for (uditer=ucopy; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        const char *unit_id = cam_unit_description_get_unit_id(udesc);
        cam_unit_driver_remove_unit_description (self, unit_id);
    }
    g_list_free (ucopy);

    if (priv->stock_module) {
        g_type_module_unuse (priv->stock_module);
    }

    G_OBJECT_CLASS (cam_unit_driver_parent_class)->finalize (obj);
}

CamUnitDriver * 
cam_unit_driver_new_stock_full (const char *package, const char * driver_name,
        const char *unit_name, uint32_t flags, 
        CamUnitConstructor constructor,
        GTypeModule * module)
{
    CamUnitDriver *self = 
        CAM_UNIT_DRIVER (g_object_new (CAM_TYPE_UNIT_DRIVER, NULL));
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);

    cam_unit_driver_set_name (self, package, driver_name);

    priv->stock_constructor = constructor;
    priv->stock_flags = flags;
    priv->stock_unit_name = strdup (unit_name);
    if (module) {
        priv->stock_module = module;
        g_type_module_use (module);
    }

    return self;
}

CamUnitDriver * 
cam_unit_driver_new_stock (const char *package, const char * driver_name,
        const char *unit_name, uint32_t flags, 
        CamUnitConstructor constructor)
{
    return cam_unit_driver_new_stock_full (package, driver_name, unit_name,
            flags, constructor, NULL);
}

int 
cam_unit_driver_start (CamUnitDriver * self)
{
    return CAM_UNIT_DRIVER_GET_CLASS (self)->start (self);
}

void 
cam_unit_driver_stop (CamUnitDriver * self)
{
    CAM_UNIT_DRIVER_GET_CLASS (self)->stop (self);
}

const char * 
cam_unit_driver_get_package (const CamUnitDriver *self)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    return priv->package;
}

const char * 
cam_unit_driver_get_name (const CamUnitDriver *self)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    return priv->name;
}

void cam_unit_set_flags (CamUnit *self, uint32_t flags);
void cam_unit_set_name (CamUnit *self, const char *name);
void cam_unit_set_id (CamUnit *self, const char *unit_id);

CamUnit *
cam_unit_driver_create_unit (CamUnitDriver *self, 
        const CamUnitDescription * udesc)
{
    CamUnit * unit =
        CAM_UNIT_DRIVER_GET_CLASS (self)->create_unit (self, udesc);
    if (!unit)
        return NULL;

    const char *unit_id = cam_unit_description_get_unit_id(udesc);
    const char *name = cam_unit_description_get_name(udesc);
    uint32_t flags = cam_unit_description_get_flags(udesc);
    cam_unit_set_id (unit, unit_id);
    cam_unit_set_name (unit, name);
    cam_unit_set_flags(unit, flags);

    return unit;
}

CamUnitDescription *
cam_unit_driver_find_unit_description (CamUnitDriver *self, const char *id)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    dbg (DBG_DRIVER, "[%s%s%s] searching for unit description [%s]\n", 
            priv->package, strlen(priv->package) ? "." : "",
            priv->name, id);
    GList *uditer;
    for (uditer=priv->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        const char *unit_id = cam_unit_description_get_unit_id(udesc);
        if (!strcmp (unit_id, id))
            return udesc;
    }
    return NULL;
}

GList *
cam_unit_driver_get_unit_descriptions (CamUnitDriver *self)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    return g_list_copy (priv->udescs);
}

int 
cam_unit_driver_get_fileno (CamUnitDriver *self)
{ return CAM_UNIT_DRIVER_GET_CLASS (self)->get_fileno (self); }

void 
cam_unit_driver_update (CamUnitDriver *self)
{ return CAM_UNIT_DRIVER_GET_CLASS (self)->update (self); }

static CamUnit * 
cam_unit_driver_default_create_unit (CamUnitDriver *self,
        const CamUnitDescription * udesc)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    if (! priv->stock_constructor || !priv->stock_unit_name) return NULL;

    return priv->stock_constructor ();
}

static int 
cam_unit_driver_default_start (CamUnitDriver *self) 
{ 
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    if (! priv->stock_constructor) return 0;

    cam_unit_driver_add_unit_description (self, priv->stock_unit_name,
            NULL, priv->stock_flags);
    return 0;
}

static int 
cam_unit_driver_default_stop (CamUnitDriver *self) 
{ 
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    GList * uditer;
    for (uditer=priv->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        g_signal_emit (G_OBJECT (self), 
                cam_unit_driver_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL],
                0, udesc);
        g_object_unref (udesc);
    }
    g_list_free (priv->udescs);
    priv->udescs = NULL;
    
    return 0;
}

static int
cam_unit_driver_default_get_fileno (CamUnitDriver *self)
{ return -1; }

static void
cam_unit_driver_default_update (CamUnitDriver *self) {}

CamUnitDescription *
cam_unit_driver_add_unit_description (CamUnitDriver *self,
        const char *name, const char *id, uint32_t flags)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    CamUnitDescription *udesc = cam_unit_description_new (self,
            name, id, flags);
    g_object_ref_sink (udesc);
    const char *unit_id = cam_unit_description_get_unit_id(udesc);

    // check for dupes
    for (GList *uditer=priv->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *cand = CAM_UNIT_DESCRIPTION (uditer->data);
        const char *cand_id = cam_unit_description_get_unit_id(cand);
        if (! strcmp (cand_id, unit_id)) {
            err ("CamUnitDriver:  detected duplicate unit description for\n"
                "                [%s]\n", unit_id);
            g_object_unref (udesc);
            return NULL;
        }
    }

    dbg (DBG_DRIVER, "adding new description for unit [%s]\n", unit_id);
    priv->udescs = g_list_append (priv->udescs, udesc);
    g_signal_emit (G_OBJECT (self),
            cam_unit_driver_signals[UNIT_DESCRIPTION_ADDED_SIGNAL],
            0,
            udesc);
    return udesc;
}

int
cam_unit_driver_remove_unit_description (CamUnitDriver *self,
        const char *unit_id)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    for (GList *uditer=priv->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        const char *cand_id = cam_unit_description_get_unit_id(udesc);
        if (! strcmp (cand_id, unit_id)) {
            dbg (DBG_DRIVER, "removing [%s] (%p)\n", unit_id, udesc);
            priv->udescs = g_list_delete_link (priv->udescs, uditer);
            g_signal_emit (G_OBJECT (self), 
                    cam_unit_driver_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL],
                    0, udesc);
            g_object_unref (udesc);
            return 0;
        }
    }
    return -1;
}

void 
cam_unit_driver_set_name (CamUnitDriver *self, const char *package,
        const char *name)
{
    CamUnitDriverPriv * priv = CAM_UNIT_DRIVER_GET_PRIVATE (self);
    free (priv->package);
    free (priv->name);
    priv->package = NULL;
    if (package)
        priv->package = strdup(package);
    else
        priv->package = strdup("");
    priv->name = strdup (name);
    dbg (DBG_DRIVER, "setting package to %s%s%s\n",
            priv->package ? priv->package : "",
            priv->package ? "." : "", priv->name);
}
