#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <gmodule.h>

#include "plugin.h"
#include "dbg.h"

typedef struct _CamPlugin CamPlugin;
typedef struct _CamPluginClass CamPluginClass;

#define CAM_TYPE_PLUGIN  cam_plugin_get_type()
#define CAM_PLUGIN(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_PLUGIN, CamPlugin))
#define CAM_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_PLUGIN, CamPluginClass ))
#define CAM_IS_PLUGIN(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_PLUGIN ))
#define CAM_IS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_PLUGIN))
#define CAM_PLUGIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_PLUGIN, CamPluginClass))

struct _CamPlugin {
    GTypeModule parent;

    char * filename;

    CamUnitDriver * (*create) (GTypeModule *);
    void (*initialize) (GTypeModule *);

    GModule * module;
};

struct _CamPluginClass {
    GTypeModuleClass parent_class;
};

GType cam_plugin_get_type (void);

G_DEFINE_TYPE (CamPlugin, cam_plugin, G_TYPE_TYPE_MODULE);

static void
cam_plugin_init (CamPlugin * self)
{
    self->filename = NULL;
}

static void
cam_plugin_finalize (GObject * obj)
{
    CamPlugin * self = CAM_PLUGIN (obj);
    free (self->filename);

    G_OBJECT_CLASS (cam_plugin_parent_class)->finalize (obj);
}

gboolean
plugin_load (GTypeModule * super);
void
plugin_unload (GTypeModule * super);

static void
cam_plugin_class_init (CamPluginClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GTypeModuleClass * module_class = G_TYPE_MODULE_CLASS (klass);

    gobject_class->finalize = cam_plugin_finalize;

    module_class->load = plugin_load;
    module_class->unload = plugin_unload;
}

CamUnitDriver *
cam_plugin_unit_driver_create (const char * filename)
{
    CamPlugin * self = CAM_PLUGIN (g_object_new (CAM_TYPE_PLUGIN, NULL));
    self->filename = strdup (filename);

    if (!g_module_supported ()) {
        g_object_unref (self);
        return NULL;
    }

    if (!g_type_module_use (G_TYPE_MODULE (self))) {
        g_object_unref (self);
        return NULL;
    }

    CamUnitDriver * driver = self->create (G_TYPE_MODULE (self));
    g_type_module_unuse (G_TYPE_MODULE (self));
    return driver;
}

gboolean
plugin_load (GTypeModule * super)
{
    CamPlugin * self = CAM_PLUGIN (super);

    self->module = g_module_open (self->filename, G_MODULE_BIND_LOCAL);
    if (!self->module) {
        g_warning ("%s", g_module_error ());
        return FALSE;
    }

    if (!g_module_symbol (self->module, "cam_plugin_initialize",
                (gpointer *) &self->initialize) ||
        !g_module_symbol (self->module, "cam_plugin_create",
                (gpointer *) &self->create)) {
        g_warning ("%s", g_module_error ());
        g_module_close (self->module);
        self->module = NULL;
        return FALSE;
    }

    dbg (DBG_PLUGIN, "Loaded %s\n", self->filename);
    self->initialize (G_TYPE_MODULE (self));
    return TRUE;
}

void
plugin_unload (GTypeModule * super)
{
    CamPlugin * self = CAM_PLUGIN (super);
    dbg (DBG_PLUGIN, "Unload %s\n", self->filename);
    g_module_close (self->module);
}
