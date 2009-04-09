#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "unit_manager.h"
#include "plugin.h"

#include "dbg.h"

#define err(args...) fprintf (stderr, args)

#ifdef G_OS_WIN32
static const char* PLUGIN_PATH_SEPARATOR = ";";
#else
static const char* PLUGIN_PATH_SEPARATOR = ":";
#endif

// if the compiler doesn't define plugin path, then don't use one
#ifndef CAMUNITS_PLUGIN_PATH
#define CAMUNITS_PLUGIN_PATH ""
#endif

// class private data
#define _GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_UNIT_MANAGER, PrivateData))
typedef struct _PrivateData PrivateData;

struct _PrivateData {
    GHashTable * running_drivers;
};

typedef struct _CamUnitManagerSource CamUnitManagerSource;
struct _CamUnitManagerSource {
    GSource gsource;
    CamUnitManager *manager;
};

struct _CamUnitManager {
    GObject parent;

    /*< public >*/

    /*< private >*/
    GList *drivers;

    int desired_driver_status;

    // stuff for asynchronous update
    GSourceFuncs source_funcs;
    CamUnitManagerSource * event_source;
    CamUnitDriver *driver_to_update;
    int event_source_attached_glib;
};

struct _CamUnitManagerClass {
    GObjectClass parent_class;
};


static CamUnitManager * _singleton = NULL;

static gboolean _source_prepare (GSource *source, gint *timeout);
static gboolean _source_check (GSource *source);
static gboolean _source_dispatch (GSource *source, 
        GSourceFunc callback, void *user_data);
static void _source_finalize (GSource *source);

enum {
    DRIVER_STOPPED,
    DRIVER_STARTED
};

enum {
    UNIT_DESCRIPTION_ADDED_SIGNAL,
    UNIT_DESCRIPTION_REMOVED_SIGNAL,
    UNIT_DRIVER_ADDED_SIGNAL,
    LAST_SIGNAL
};

static guint cam_unit_manager_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_manager_finalize (GObject *obj);
static void cam_unit_manager_register_core_drivers (CamUnitManager *self);

G_DEFINE_TYPE (CamUnitManager, cam_unit_manager, G_TYPE_OBJECT);

static void
cam_unit_manager_init (CamUnitManager *self)
{
    dbg (DBG_MANAGER, "constructor\n");

    self->drivers = NULL;
    self->desired_driver_status = DRIVER_STOPPED;

    self->source_funcs.prepare  = _source_prepare;
    self->source_funcs.check    = _source_check;
    self->source_funcs.dispatch = _source_dispatch;
    self->source_funcs.finalize = _source_finalize;

    self->event_source = (CamUnitManagerSource*) g_source_new (
            &self->source_funcs, sizeof (CamUnitManagerSource));
    self->event_source->manager = self;
    self->driver_to_update = NULL;
    self->event_source_attached_glib = 0;

    PrivateData * priv = _GET_PRIVATE(self);
    priv->running_drivers = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static void
cam_unit_manager_class_init (CamUnitManagerClass *klass)
{
    dbg (DBG_MANAGER, "class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_manager_finalize;

    /**
     * CamUnitManager::unit-description-added
     * @manager: the CamUnitManager emitting the signal
     * @udesc: the CamUnitDescription just added
     *
     * The unit-description-added signal is emitted when the CamUnitManager
     * detects that a driver has added a CamUnitDescription to its list of
     * available units.
     */
    cam_unit_manager_signals[UNIT_DESCRIPTION_ADDED_SIGNAL] = 
        g_signal_new ("unit-description-added",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_DESCRIPTION);

    /**
     * CamUnitManager::unit-description-removed
     * @driver: the CamUnitManager emitting the signal
     * @udesc: the CamUnitDescription being removed
     *
     * The unit-description-removed signal is emitted when a CamUnitDriver
     * removes a CamUnitDescription from its list of available units.
     *
     * %udesc is guaranteed to be valid through the duration of the signal
     * handlers, but is not guaranteed to be valid afterwards.  Thus, signal
     * handlers should not retain references to %udesc.
     */
    cam_unit_manager_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL] = 
        g_signal_new ("unit-description-removed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_DESCRIPTION);

    /**
     * CamUnitManager::unit-driver-added
     * @manager: the CamUnitManager emitting the signal
     * @driver: the CamUnitDriver just added
     *
     * The unit-driver-added signal is emitted when the CamUnitManager adds
     * a new CamUnitDriver to its list of managed drivers.
     */
    cam_unit_manager_signals[UNIT_DRIVER_ADDED_SIGNAL] = 
        g_signal_new ("unit-driver-added",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1,
                CAM_TYPE_UNIT_DRIVER);

    g_type_class_add_private(gobject_class, sizeof(PrivateData));
}

static void
cam_unit_manager_finalize (GObject *obj)
{
    dbg (DBG_MANAGER, "finalize\n");
    CamUnitManager *self = CAM_UNIT_MANAGER (obj);

    if (self->event_source)
        g_source_destroy ((GSource *) self->event_source);

    cam_unit_manager_stop_drivers (self);

    GList *iter;
    for (iter=self->drivers; iter; iter=iter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (iter->data);
        dbgl (DBG_REF, "unref driver [%s]\n", cam_unit_driver_get_name(driver));
        g_object_unref (driver);
    }
    g_list_free (self->drivers);

    PrivateData * priv = _GET_PRIVATE(self);
    g_hash_table_destroy(priv->running_drivers);

    G_OBJECT_CLASS (cam_unit_manager_parent_class)->finalize (obj);

    _singleton = NULL;
}

static CamUnitManager *
cam_unit_manager_new (gboolean start_drivers)
{
    CamUnitManager *self = 
        CAM_UNIT_MANAGER (g_object_new (CAM_TYPE_UNIT_MANAGER, NULL));
    cam_unit_manager_register_core_drivers (self);

    if (start_drivers) {
        cam_unit_manager_start_drivers (self);
    }

    return self;
}

CamUnitManager * 
cam_unit_manager_get_and_ref (void)
{
    // XXX this is not thread safe
    if(_singleton) {
        g_object_ref(G_OBJECT(_singleton));
        return _singleton;
    }
    dbg (DBG_MANAGER, "Instantiating singleton CamUnitManager\n");
    _singleton = cam_unit_manager_new(TRUE);
    return _singleton;
}

static void
on_unit_description_added (CamUnitDriver *driver, CamUnitDescription *udesc,
        CamUnitManager *self)
{
    dbg (DBG_MANAGER, "new unit description [%s] detected\n", 
            cam_unit_description_get_unit_id(udesc));

    // only care about new unit descriptions so that we can propagate the
    // signal
    g_signal_emit (G_OBJECT (self), 
            cam_unit_manager_signals[UNIT_DESCRIPTION_ADDED_SIGNAL],
            0, udesc);
}

static void
on_unit_description_removed (CamUnitDriver *driver, CamUnitDescription *udesc,
        CamUnitManager *self)
{
    dbg (DBG_MANAGER, "detected removed unit description [%s]\n", 
            cam_unit_description_get_unit_id(udesc));
    g_signal_emit (G_OBJECT (self),
            cam_unit_manager_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL],
            0, udesc);
}

void 
cam_unit_manager_add_driver (CamUnitManager *self, CamUnitDriver *driver)
{
    dbg (DBG_MANAGER, "add driver %s\n", cam_unit_driver_get_name(driver));
    self->drivers = g_list_append (self->drivers, driver);

    dbgl (DBG_REF, "ref_sink driver\n");
    g_object_ref_sink (driver);

    // subscribe to driver events
    g_signal_connect (G_OBJECT (driver), "unit-description-added",
            G_CALLBACK (on_unit_description_added), self);
    g_signal_connect (G_OBJECT (driver), "unit-description-removed",
            G_CALLBACK (on_unit_description_removed), self);

    // Check if the driver provides a file descriptor.  If so, add the
    // file descriptor to the manager's event source
    int driver_fileno = cam_unit_driver_get_fileno (driver);
    if (driver_fileno >= 0) {
        GPollFD *pfd = (GPollFD*) malloc (sizeof (GPollFD));

        pfd->fd = driver_fileno;
        pfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
        pfd->revents = 0;

        g_object_set_data (G_OBJECT (driver), "ManagerPollFD", pfd);
        g_source_add_poll ( (GSource *)self->event_source, pfd);
    }

    // maybe start the driver
    if (self->desired_driver_status == DRIVER_STARTED) {
        int status = cam_unit_driver_start (driver);

        // mark the driver if it started successfully
        if(0 != status) {
            PrivateData * priv = _GET_PRIVATE(self);
            g_hash_table_insert(priv->running_drivers, driver, driver);
        }
    }
}

int 
cam_unit_manager_remove_driver (CamUnitManager *self, 
        CamUnitDriver *driver)
{
    err ("unit_manager: remove driver not yet implemented!\n");
    // TODO
 
    // remove a GPollFD if it was setup earlier
    GPollFD *pfd = g_object_get_data (G_OBJECT (driver), "ManagerPollFD");
    if (pfd) {
        if (self->event_source)
            g_source_remove_poll ( (GSource*)self->event_source, pfd);
        g_object_set_data (G_OBJECT (driver), "ManagerPollFD", NULL);
        free (pfd);
    }

    return -1;
}

int
cam_unit_manager_start_drivers (CamUnitManager * self)
{
    dbg (DBG_MANAGER, "start all drivers \n");

    PrivateData * priv = _GET_PRIVATE(self);
    self->desired_driver_status = DRIVER_STARTED;

    /* Loop through the list of drivers that the unit manager has discovered. */
    GList *iter;
    for (iter=self->drivers; iter; iter=iter->next) {
        CamUnitDriver * driver = (CamUnitDriver*) iter->data;

        // only start drivers that haven't already been started
        if(NULL == g_hash_table_lookup(priv->running_drivers, driver)) {
            /* Assign callbacks and start the driver.  The driver will discover
             * any units that it provides, and notify us of their availability
             * by invoking the callbacks. */
            int status = cam_unit_driver_start (driver);

            if(0 == status) {
                g_hash_table_insert(priv->running_drivers, driver, driver);
            }
        }

    }

    return 0;
}

int
cam_unit_manager_stop_drivers (CamUnitManager * self)
{
    dbg (DBG_MANAGER, "stopping all drivers\n");

    PrivateData * priv = _GET_PRIVATE(self);
    self->desired_driver_status = DRIVER_STOPPED;

    GList *iter;
    for (iter=self->drivers; iter; iter=iter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (iter->data);

        // only try to stop running drivers
        if(g_hash_table_lookup(priv->running_drivers, driver)) {
            cam_unit_driver_stop (driver);
            g_hash_table_remove(priv->running_drivers, driver);
        }
    }
    return 0;
}

GList *
cam_unit_manager_get_drivers (CamUnitManager *self)
{
    return g_list_copy (self->drivers);
}

const CamUnitDescription *
cam_unit_manager_find_unit_description (CamUnitManager *self,
        const char *unit_id)
{
    dbg (DBG_MANAGER, "searching for %s\n", unit_id);
    GList *diter;
    for (diter=self->drivers; diter; diter=diter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (diter->data);
        CamUnitDescription *udesc = cam_unit_driver_find_unit_description (
                driver, unit_id);
        if (udesc) {
            return udesc;
        }
    }
    return NULL;
}

GList *
cam_unit_manager_list_package (CamUnitManager *self,
       const char *query_package, int recurse)
{
    if (!query_package) query_package = "";
    int query_len = strlen (query_package);

    GList *result = NULL;
    GList *diter;
    for (diter=self->drivers; diter; diter=diter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (diter->data);
        if(!strlen(cam_unit_driver_get_package(driver))) {
            continue;
        }

        int match = 0;
        if (recurse) {
            match = !strncmp(query_package, 
                            cam_unit_driver_get_package(driver),
                            query_len);
            // FIXME query of "input.foo" will match package "input.foobar"
        } else {
            match = !strcmp (query_package, 
                        cam_unit_driver_get_package(driver));
        }
        if (!match) continue;

        GList *udlist = cam_unit_driver_get_unit_descriptions (driver);
        GList *uditer;
        for (uditer=udlist; uditer; uditer=uditer->next) {
            CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION (uditer->data);
            result = g_list_append (result, udesc);
        }
        g_list_free (udlist);
    }
    return result;
}

CamUnit * 
cam_unit_manager_create_unit_by_id (CamUnitManager *self,
        const char *unit_id)
{
    const CamUnitDescription *udesc = 
        cam_unit_manager_find_unit_description (self, unit_id);
    if (! udesc) return NULL;
    CamUnitDriver *driver = cam_unit_description_get_driver(udesc);
    return cam_unit_driver_create_unit (driver, udesc);
}

void 
cam_unit_manager_add_plugin_dir (CamUnitManager *self, const char *path)
{
    if (! strlen(path)) return;
    dbg (DBG_MANAGER, "Checking %s for plugins...\n", path);

    DIR * dir = opendir (path);
    if (!dir) {
        dbg (DBG_MANAGER, "Warning: failed to open %s: %s\n", path,
                strerror (errno));
        return;
    }

    struct dirent * dirent;
    while ((dirent = readdir (dir))) {
        if (dirent->d_name[0] == '.')
            continue;
        int len = strlen (dirent->d_name);
        if (len > 3 && !strcmp (dirent->d_name + len - 3, ".la"))
            continue;

        gchar * filename = g_build_filename (path, dirent->d_name, NULL);

        CamUnitDriver * driver = cam_plugin_unit_driver_create (filename);
        if (driver)
            cam_unit_manager_add_driver (self, driver);

        g_free (filename);
    }

    closedir (dir);
}

static gboolean
_source_prepare (GSource *source, gint *timeout)
{
    *timeout = -1;
    return FALSE;
}

static gboolean
_source_check (GSource *source)
{
    CamUnitManagerSource * csource = (CamUnitManagerSource *) source;
    CamUnitManager * self = csource->manager;

    self->driver_to_update = NULL;

    for (GList *diter=self->drivers; diter; diter=diter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (diter->data);

        GPollFD *pfd = (GPollFD*) g_object_get_data (G_OBJECT (driver),
                "ManagerPollFD");
        if (pfd && pfd->fd >= 0 && pfd->revents) {
            self->driver_to_update = driver;
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
_source_dispatch (GSource *source, GSourceFunc callback, void *user_data)
{
    CamUnitManagerSource * csource = (CamUnitManagerSource *) source;
    CamUnitManager * self = csource->manager;

    if (self->driver_to_update) {
        cam_unit_driver_update (self->driver_to_update);
        self->driver_to_update = NULL;
    }

    return TRUE;
}

static void _source_finalize (GSource *source) {}

void 
cam_unit_manager_attach_glib (CamUnitManager *self, int priority,
        GMainContext * context)
{
    if (self->event_source_attached_glib) {
        cam_unit_manager_detach_glib (self);
    }
    g_source_attach ((GSource*) self->event_source, context);
    g_source_set_priority ((GSource*) self->event_source, priority);
    self->event_source_attached_glib = 1;
}

void 
cam_unit_manager_detach_glib (CamUnitManager *self)
{
    if (!self->event_source_attached_glib)
        return;

    // GLib (as of 2.12) does not provide a g_source_detach method, or
    // something similar.  It does, however, provide a g_source_destroy
    // method.  So destroy the GSource and create a new one.
    g_source_destroy ((GSource *) self->event_source);

    self->event_source = (CamUnitManagerSource*) g_source_new (
            &self->source_funcs, sizeof (CamUnitManagerSource));
    self->event_source->manager = self;

    for (GList *diter=self->drivers; diter; diter=diter->next) {
        CamUnitDriver *driver = diter->data;

        // Check if the driver provides a file descriptor.  If so, add the
        // file descriptor to the manager's event source
        int driver_fileno = cam_unit_driver_get_fileno (driver);
        if (driver_fileno >= 0) {
            GPollFD *pfd = (GPollFD*) malloc (sizeof (GPollFD));

            pfd->fd = driver_fileno;
            pfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
            pfd->revents = 0;

            g_object_set_data (G_OBJECT (driver), "ManagerPollFD", pfd);
            g_source_add_poll ( (GSource *)self->event_source, pfd);
        }
    }
    self->event_source_attached_glib = 0;
}

void 
cam_unit_manager_update (CamUnitManager *self)
{
    for (GList *diter=self->drivers; diter; diter=diter->next) {
        CamUnitDriver *driver = diter->data;
        cam_unit_driver_update (driver);
    }
}

static void
cam_unit_manager_register_core_drivers (CamUnitManager *self)
{
    // scan for plugins
    char **path_dirs = g_strsplit (CAMUNITS_PLUGIN_PATH, 
            PLUGIN_PATH_SEPARATOR, 0);
    for (int i=0; path_dirs[i]; i++) {
        cam_unit_manager_add_plugin_dir (self, path_dirs[i]);
    }
    g_strfreev (path_dirs);

    const char *plugin_path_env = g_getenv ("CAMUNITS_PLUGIN_PATH");
    if (plugin_path_env && strlen (plugin_path_env)) {
        char **env_dirs = g_strsplit (plugin_path_env, 
                PLUGIN_PATH_SEPARATOR, 0);
        for (int i=0; env_dirs[i]; i++) {
            cam_unit_manager_add_plugin_dir (self, env_dirs[i]);
        }
        g_strfreev (env_dirs);
    }
}
