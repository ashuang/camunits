#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <camunits/dbg.h>
#include "unit_description_widget.h"

#define err(args...) fprintf(stderr, args)


static void cam_unit_description_widget_finalize( GObject *obj );

G_DEFINE_TYPE (CamUnitDescriptionWidget, cam_unit_description_widget,
        GTK_TYPE_TEXT_VIEW);

static void
cam_unit_description_widget_init( CamUnitDescriptionWidget *self )
{
    dbg(DBG_GUI, "unit description widget constructor\n");
    GtkTextBuffer * buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
    gtk_text_buffer_create_tag (buffer, "bold", "weight", PANGO_WEIGHT_BOLD,
            NULL);

    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self), PANGO_WRAP_WORD_CHAR);
    gtk_text_view_set_indent (GTK_TEXT_VIEW (self), -30);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self), 5);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self), 5);
    gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (self), 5);
}

static void
cam_unit_description_widget_class_init( CamUnitDescriptionWidgetClass *klass )
{
    dbg(DBG_GUI, "unit description widget class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    // add a class-specific destructor
    gobject_class->finalize = cam_unit_description_widget_finalize;
}

// destructor (more or less)
static void
cam_unit_description_widget_finalize( GObject *obj )
{
    dbg(DBG_GUI, "unit description widget finalize\n" );
    dbgl(DBG_REF, "unref description\n");

    G_OBJECT_CLASS (cam_unit_description_widget_parent_class)->finalize(obj);
}

CamUnitDescriptionWidget *
cam_unit_description_widget_new (void)
{
    CamUnitDescriptionWidget * self = CAM_UNIT_DESCRIPTION_WIDGET (
            g_object_new(CAM_TYPE_UNIT_DESCRIPTION_WIDGET, NULL));
    return self;
}

static void
on_cursor_changed (GtkTreeView * tree_view, void * user)
{
    CamUnitDescriptionWidget * self = user;
    CamUnitDescription * desc =
        cam_unit_manager_widget_get_selected_description (self->manager);
    GtkTextBuffer * buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
    gtk_text_buffer_set_text (buffer, "", -1);
    if (!desc)
        return;

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Name: ", -1,
            "bold", NULL);
    gtk_text_buffer_insert (buffer, &iter, desc->name, -1);
    gtk_text_buffer_insert (buffer, &iter, "\n", -1);

    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, "Unit ID: ", -1,
            "bold", NULL);
    gtk_text_buffer_insert (buffer, &iter, desc->unit_id, -1);
    //gtk_text_buffer_insert (buffer, &iter, "\n", -1);
}

int
cam_unit_description_widget_set_manager( CamUnitDescriptionWidget* self, 
        CamUnitManagerWidget *manager )
{
    self->manager = manager;
    g_signal_connect(G_OBJECT(self->manager), "cursor-changed", 
            G_CALLBACK(on_cursor_changed), self);
    return 0;
}
