#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "unit_manager.h"
#include "plugin.h"

#include "input_example.h"
#include "input_log.h"
#include "filter_gl.h"
#include "filter_fast_bayer.h"
#include "convert_colorspace.h"
#include "convert_to_rgb8.h"
#include "filter_jpeg.h"
#include "output_logger.h"

#ifdef USE_V4L2
#include "input_v4l2.h"
#endif

#ifdef USE_V4L
#include "input_v4l.h"
#endif

#ifdef ENABLE_DC1394
#include "input_dc1394.h"
#include "filter_bayer.h"
#endif

#include "dbg.h"

#define err(args...) fprintf (stderr, args)

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
     * The unit-description-addede signal is emitted when the CamUnitManager
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
                G_TYPE_OBJECT);

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
}

static void
cam_unit_manager_finalize (GObject *obj)
{
    dbg (DBG_MANAGER, "finalize\n");
    CamUnitManager *self = CAM_UNIT_MANAGER (obj);

    cam_unit_manager_stop_drivers (self);

    GList *iter;
    for (iter=self->drivers; iter; iter=iter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (iter->data);
        dbgl (DBG_REF, "unref driver [%s]\n", driver->name);
        g_object_unref (driver);
    }
    g_list_free (self->drivers);

    G_OBJECT_CLASS (cam_unit_manager_parent_class)->finalize (obj);
}

CamUnitManager *
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

static void
on_unit_description_added (CamUnitDriver *driver, CamUnitDescription *udesc,
        CamUnitManager *self)
{
    dbg (DBG_MANAGER, "new unit description [%s] detected\n", udesc->unit_id);

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
            udesc->unit_id);
    g_signal_emit (G_OBJECT (self),
            cam_unit_manager_signals[UNIT_DESCRIPTION_REMOVED_SIGNAL],
            0, udesc);
}

void 
cam_unit_manager_add_driver (CamUnitManager *self, CamUnitDriver *driver)
{
    dbg (DBG_MANAGER, "add driver %s\n", driver->name);
    self->drivers = g_list_append (self->drivers, driver);

    dbgl (DBG_REF, "ref_sink driver\n");
    g_object_ref_sink (driver);

    g_signal_connect (G_OBJECT (driver), "unit-description-added",
            G_CALLBACK (on_unit_description_added), self);
    g_signal_connect (G_OBJECT (driver), "unit-description-removed",
            G_CALLBACK (on_unit_description_removed), self);

    if (self->desired_driver_status == DRIVER_STARTED) {
        cam_unit_driver_start (driver);
    }
}

int 
cam_unit_manager_remove_driver (CamUnitManager *self, 
        CamUnitDriver *driver)
{
    err ("unit_manager: remove driver not yet implemented!\n");
    // TODO
    return -1;
}

int
cam_unit_manager_start_drivers (CamUnitManager * self)
{
    dbg (DBG_MANAGER, "start all drivers \n");

    self->desired_driver_status = DRIVER_STARTED;

    /* Loop through the list of drivers that the unit manager has discovered. */
    GList *iter;
    for (iter=self->drivers; iter; iter=iter->next) {
        CamUnitDriver * driver = (CamUnitDriver*) iter->data;

        /* Assign callbacks and start the driver.  The driver will discover
         * any units that it provides, and notify us of their availability
         * by invoking the callbacks. */
        cam_unit_driver_start (driver);
    }

    return 0;
}

int
cam_unit_manager_stop_drivers (CamUnitManager * self)
{
    dbg (DBG_MANAGER, "stopping all drivers\n");
    self->desired_driver_status = DRIVER_STOPPED;

    GList *iter;
    for (iter=self->drivers; iter; iter=iter->next) {
        CamUnitDriver *driver = CAM_UNIT_DRIVER (iter->data);
        cam_unit_driver_stop (driver);
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
        if (!driver->package)
            continue;

        int match = 0;
        if (recurse) {
            match = (!strncmp (query_package, driver->package,
                        query_len));
            // FIXME query of "input.foo" will match package "input.foobar"
        } else {
            match = (!strcmp (query_package, driver->package));
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
    return cam_unit_driver_create_unit (udesc->driver, udesc);
}

void 
cam_unit_manager_add_plugin_dir (CamUnitManager *self, const char *path)
{
    DIR * dir = opendir (path);
    if (!dir) {
        fprintf (stderr, "Warning: failed to open %s: %s\n", path,
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

static void
cam_unit_manager_register_core_drivers (CamUnitManager *self)
{
    // register core CamUnit drivers

    CamUnitDriver *input_example_driver = 
        CAM_UNIT_DRIVER (cam_input_example_driver_new ());
    cam_unit_manager_add_driver (self, input_example_driver);

    CamUnitDriver *input_log_driver = 
        CAM_UNIT_DRIVER (cam_input_log_driver_new ());
    cam_unit_manager_add_driver (self, input_log_driver);

#ifdef USE_V4L2
    CamUnitDriver *v4l2_driver = cam_v4l2_driver_new ();
    cam_unit_manager_add_driver (self, v4l2_driver);
#endif

#ifdef USE_V4L
    CamUnitDriver *v4l_driver = cam_v4l_driver_new ();
    cam_unit_manager_add_driver (self, v4l_driver);
#endif

#ifdef ENABLE_DC1394
    CamUnitDriver *dc1394_driver = cam_dc1394_driver_new ();
    cam_unit_manager_add_driver (self, dc1394_driver);

    CamUnitDriver *bayer_filter_driver = cam_bayer_filter_driver_new (); 
    cam_unit_manager_add_driver (self, bayer_filter_driver);
#endif

    CamUnitDriver *fast_bayer_filter_driver = 
        cam_fast_bayer_filter_driver_new (); 
    cam_unit_manager_add_driver (self, fast_bayer_filter_driver);

    CamUnitDriver *cconv_filter = cam_color_conversion_filter_driver_new ();
    cam_unit_manager_add_driver (self, cconv_filter);

    CamUnitDriver *torgb8 = cam_convert_to_rgb8_driver_new ();
    cam_unit_manager_add_driver (self, torgb8);

    CamUnitDriver *filter_gl_driver = cam_filter_gl_driver_new ();
    cam_unit_manager_add_driver (self, filter_gl_driver);

    CamUnitDriver *filter_jpeg_driver = cam_filter_jpeg_driver_new ();
    cam_unit_manager_add_driver (self, filter_jpeg_driver);

    CamUnitDriver *logger_driver = cam_logger_unit_driver_new ();
    cam_unit_manager_add_driver (self, logger_driver);

    cam_unit_manager_add_plugin_dir (self, LIBCAM_PLUGINS_PATH);

    const char *plugin_path_env = g_getenv ("LIBCAM_PLUGIN_PATH");
    if (plugin_path_env && strlen (plugin_path_env)) {
        char **env_dirs = g_strsplit (plugin_path_env, ":", 0);
        for (int i=0; env_dirs[i]; i++) {
            cam_unit_manager_add_plugin_dir (self, env_dirs[i]);
        }
        g_strfreev (env_dirs);
    }
}
