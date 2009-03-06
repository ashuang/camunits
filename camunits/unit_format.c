#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unit_format.h"
#include "dbg.h"

static void cam_unit_format_finalize (GObject *obj);
static void cam_unit_format_init (CamUnitFormat *self);
static void cam_unit_format_class_init (CamUnitFormatClass *klass);

G_DEFINE_TYPE (CamUnitFormat, cam_unit_format, G_TYPE_OBJECT);

static void
cam_unit_format_init (CamUnitFormat *self)
{
    self->pixelformat = 0;
    self->name = NULL;
    self->width = 0;
    self->height = 0;
    self->row_stride = 0;
}

static void
cam_unit_format_class_init (CamUnitFormatClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_unit_format_finalize;
}

static void
cam_unit_format_finalize (GObject *obj)
{
    CamUnitFormat *self = CAM_UNIT_FORMAT (obj);
    dbg(DBG_UNIT, "finalizing unit format %p\n", self);
    if (self->name) free (self->name);
    G_OBJECT_CLASS (cam_unit_format_parent_class)->finalize(obj);
}


CamUnitFormat *
cam_unit_format_new (CamPixelFormat pfmt, const char *name, 
        int width, int height, int row_stride)
{
    CamUnitFormat *self = CAM_UNIT_FORMAT(
            g_object_new(CAM_TYPE_UNIT_FORMAT, NULL));

    self->pixelformat = pfmt;
    self->name = strdup(name);
    self->width = width;
    self->height = height;
    self->row_stride = row_stride;

    return self;
}

int 
cam_unit_format_equals(const CamUnitFormat *self, const CamUnitFormat *a)
{
    return (self->pixelformat == a->pixelformat &&
            (!strcmp (self->name, a->name)) &&
            self->width == a->width &&
            self->height == a->height &&
            self->row_stride == a->row_stride);
}
