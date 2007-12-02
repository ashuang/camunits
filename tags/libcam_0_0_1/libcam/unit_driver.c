#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unit_driver.h"

#include "dbg.h"

#define err(args...) fprintf (stderr, args)

// ================= CamUnitDescription ==================

static void cam_unit_description_finalize (GObject *obj);
static CamUnit * cam_unit_driver_default_create_unit (CamUnitDriver *self,
        const CamUnitDescription * udesc);

G_DEFINE_TYPE (CamUnitDescription, cam_unit_description, 
        G_TYPE_INITIALLY_UNOWNED);

static void
cam_unit_description_init (CamUnitDescription *self)
{
    self->driver = NULL;
    self->name = NULL;
    self->unit_id = NULL;
    self->flags = 0;
}

static void
cam_unit_description_class_init (CamUnitDescriptionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_description_finalize;
}

CamUnitDescription *
cam_unit_description_new (CamUnitDriver *driver, const char *name, 
        const char *id, uint32_t flags)
{
    CamUnitDescription *self = g_object_new (CAM_TYPE_UNIT_DESCRIPTION, NULL);
    self->driver = driver;
    self->name = strdup (name);
    self->unit_id = g_strdup_printf ("%s%s%s%s%s",
            driver->package ? driver->package : "",
            driver->package ? "." : "",
            driver->name, id ? ":" : "", id ? id : "");
    self->flags = flags;
    return self;
}

static void
cam_unit_description_finalize (GObject *obj)
{
    CamUnitDescription *self = CAM_UNIT_DESCRIPTION (obj);
    dbg (DBG_DRIVER, "finalize unit description [%s]\n", self->unit_id);

    if (self->name) free (self->name);
    if (self->unit_id) g_free (self->unit_id);

    G_OBJECT_CLASS (cam_unit_description_parent_class)->finalize (obj);
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
static CamUnitDescription* cam_unit_driver_default_search_unit_description (
        CamUnitDriver *self, const char *id);

G_DEFINE_TYPE (CamUnitDriver, cam_unit_driver, G_TYPE_INITIALLY_UNOWNED);

static void
cam_unit_driver_init (CamUnitDriver *self)
{
    self->package = NULL;
    self->udescs = NULL;
    self->stock_unit_name = NULL;
    self->stock_flags = 0;
    self->stock_constructor = NULL;
}

static void
cam_unit_driver_class_init (CamUnitDriverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_driver_finalize;

    klass->start = cam_unit_driver_default_start;
    klass->start = cam_unit_driver_default_start;
    klass->stop = cam_unit_driver_default_stop;
    klass->search_unit_description = 
        cam_unit_driver_default_search_unit_description;

    // pure virtual
    klass->create_unit = cam_unit_driver_default_create_unit;

    cam_unit_driver_signals[UNIT_DESCRIPTION_ADDED_SIGNAL] = 
        g_signal_new ("unit-description-added",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                G_TYPE_OBJECT);
    cam_unit_driver_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL] = 
        g_signal_new ("unit-description-removed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                G_TYPE_OBJECT);
}

static void
cam_unit_driver_finalize (GObject *obj)
{
    CamUnitDriver *self = CAM_UNIT_DRIVER (obj);

    dbg (DBG_DRIVER, "CamUnitDriver finalize [%s]\n", self->package);

    if (self->stock_unit_name) free (self->stock_unit_name);
    if (self->package) free (self->package);
    if (self->name) free (self->name);
    GList *ucopy = g_list_copy (self->udescs);
    GList *uditer;
    for (uditer=ucopy; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        cam_unit_driver_remove_unit_description (self, udesc->unit_id);
    }
    g_list_free (ucopy);

    if (self->stock_module) {
        g_type_module_unuse (self->stock_module);
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

    cam_unit_driver_set_name (self, package, driver_name);

    self->stock_constructor = constructor;
    self->stock_flags = flags;
    self->stock_unit_name = strdup (unit_name);
    if (module) {
        self->stock_module = module;
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
    return self->package;
}

CamUnit *
cam_unit_driver_create_unit (CamUnitDriver *self, 
        const CamUnitDescription * udesc)
{
    CamUnit * unit =
        CAM_UNIT_DRIVER_GET_CLASS (self)->create_unit (self, udesc);
    if (!unit)
        return NULL;

    cam_unit_set_id (unit, udesc->unit_id);
    cam_unit_set_name (unit, udesc->name);
    unit->flags = udesc->flags;

    return unit;
}

CamUnitDescription *
cam_unit_driver_find_unit_description (CamUnitDriver *self, const char *id)
{
    dbg (DBG_DRIVER, "[%s] searching for unit description [%s]\n", 
            self->package, id);
    CamUnitDescription *result = NULL;
    GList *uditer;
    for (uditer=self->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        if (!strcmp (udesc->unit_id, id)) return udesc;
    }
    result = 
        CAM_UNIT_DRIVER_GET_CLASS (self)->search_unit_description (self, id);
    return result;
}

GList *
cam_unit_driver_get_unit_descriptions (CamUnitDriver *self)
{
    return g_list_copy (self->udescs);
}

static CamUnit * 
cam_unit_driver_default_create_unit (CamUnitDriver *self,
        const CamUnitDescription * udesc)
{
    if (! self->stock_constructor || !self->stock_unit_name) return NULL;

    return self->stock_constructor ();
}

static int 
cam_unit_driver_default_start (CamUnitDriver *self) 
{ 
    if (! self->stock_constructor) return 0;

    cam_unit_driver_add_unit_description (self, self->stock_unit_name,
            NULL, self->stock_flags);
    return 0;
}

static int 
cam_unit_driver_default_stop (CamUnitDriver *self) 
{ 
    GList * uditer;
    for (uditer=self->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        g_signal_emit (G_OBJECT (self), 
                cam_unit_driver_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL],
                0, udesc);
        g_object_unref (udesc);
    }
    g_list_free (self->udescs);
    self->udescs = NULL;
    
    return 0;
}

static CamUnitDescription* 
cam_unit_driver_default_search_unit_description (CamUnitDriver *self, 
        const char *unit_id)
{
    return NULL;
}

CamUnitDescription *
cam_unit_driver_add_unit_description (CamUnitDriver *self,
        const char *name, const char *id, uint32_t flags)
{
    CamUnitDescription *udesc = cam_unit_description_new (self,
            name, id, flags);
    g_object_ref_sink (udesc);

    // check for dupes
    GList *uditer;
    for (uditer=self->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *cand = CAM_UNIT_DESCRIPTION (uditer->data);
        if (! strcmp (cand->unit_id, udesc->unit_id)) {
            err ("CamUnitDriver:  detected duplicate unit description for\n"
                "                [%s]\n", udesc->unit_id);
            g_object_unref (udesc);
            return NULL;
        }
    }

    dbg (DBG_DRIVER, "adding new description for unit [%s]\n",
            udesc->unit_id);
    self->udescs = g_list_append (self->udescs, udesc);
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
    GList *uditer;
    for (uditer=self->udescs; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
        if (! strcmp (udesc->unit_id, unit_id)) {
            dbg (DBG_DRIVER, "removing [%s] (%p)\n", unit_id, udesc);
            self->udescs = g_list_delete_link (self->udescs, uditer);
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
    if (self->package)
        free (self->package);
    self->package = NULL;
    if (package)
        self->package = strdup (package);
    if (self->name)
        free (self->name);
    self->name = strdup (name);
    dbg (DBG_DRIVER, "setting package to %s%s%s\n",
            self->package ? self->package : "",
            self->package ? "." : "", self->name);
}
