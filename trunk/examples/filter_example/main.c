#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

#include <gtk/gtk.h>

#include <libcam/cam.h>
#include <libcam/cam_gtk.h>

#include "filter_example.h"

typedef struct _state_t {
    CamUnitChain *chain;

    CamUnitChainWidget *chain_widget;
    CamUnitChainGLWidget *chain_gl_widget;
    CamUnitDescriptionWidget * desc_widget;

    GtkWindow *window;
    GtkWidget *chain_frame;
} state_t;

// ==================== signal handlers =====================

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, const CamFrameBuffer *buf, 
        void *user_data)
{
    state_t *self = (state_t*) user_data;
    cam_unit_chain_gl_widget_request_redraw (self->chain_gl_widget);
}

// ========== administrative methods (construction, destruction) ==========

static void
setup_gtk (state_t *self)
{
    self->window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(self->window, "Camview");
    gtk_window_set_resizable(self->window, TRUE);
    gtk_window_set_default_size(self->window, 700, 540);
    gtk_signal_connect (GTK_OBJECT (self->window), "delete_event", 
            GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (self->window), vbox);

    // horizontal panes
    GtkWidget *hpane2 = gtk_hpaned_new ();
    gtk_box_pack_start (GTK_BOX (vbox), hpane2, TRUE, TRUE, 0);

    self->chain_frame = gtk_frame_new ("Chain");
    GtkWidget *display_frame = gtk_frame_new ("Display");
    gtk_paned_pack1 (GTK_PANED(hpane2), display_frame, TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED(hpane2), self->chain_frame, FALSE, TRUE);

    gtk_paned_set_position (GTK_PANED (hpane2), 400);

    // chain gl widget
    self->chain_gl_widget = cam_unit_chain_gl_widget_new (self->chain);
    gtk_container_add (GTK_CONTAINER (display_frame), 
                GTK_WIDGET (self->chain_gl_widget));

    // chain widget
    self->chain_widget = cam_unit_chain_widget_new (self->chain);
    gtk_container_add (GTK_CONTAINER (self->chain_frame), 
            GTK_WIDGET (self->chain_widget));
    gtk_widget_show (GTK_WIDGET (self->chain_widget));

    gtk_widget_show_all (GTK_WIDGET (self->window));
}

static void
print_usage_and_inputs (const char *progname, CamUnitManager *manager)
{
    fprintf (stderr, "usage: %s <input_id>\n\n", progname);
    fprintf (stderr, "Available inputs:\n\n"); 
    GList *udlist = cam_unit_manager_list_package (manager, "input", TRUE);
    for (GList *uditer=udlist; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION(uditer->data);
        printf("  %s  (%s)\n", udesc->unit_id, udesc->name);
    }
    g_list_free(udlist);
}

int main (int argc, char **argv)
{
    gtk_init (&argc, &argv);
    g_thread_init (NULL);

    state_t * self = (state_t*) calloc (1, sizeof (state_t));

    // create the image processing chain
    self->chain = cam_unit_chain_new ();

    // register our custom unit driver
    cam_unit_manager_add_driver (self->chain->manager, 
            cam_filter_example_driver_new ());

    // abort if no input unit was specified
    if (argc < 2) {
        print_usage_and_inputs (argv[0], self->chain->manager);
        goto failed;
    }
    const char *input_id = argv[1];

    // instantiate the input unit
    if (! cam_unit_chain_add_unit_by_id (self->chain, input_id)) {
        fprintf (stderr, "Oh no!  Couldn't create input unit [%s].\n\n", 
                input_id);
        print_usage_and_inputs (argv[0], self->chain->manager);
        goto failed;
    }

    // create a unit to convert the input data to 8-bit RGB
    CamUnit *to_rgb8 = cam_unit_chain_add_unit_by_id (self->chain, 
            "convert.to_rgb8");

    // add our custom unit
    cam_unit_chain_add_unit_by_id (self->chain, "filter.example");

    // add a display unit
    cam_unit_chain_add_unit_by_id (self->chain, "output.opengl");

    // setup the GUI
    setup_gtk (self);

    cam_unit_chain_set_desired_status (self->chain, CAM_UNIT_STATUS_STREAMING);
    cam_unit_chain_attach_glib (self->chain, 1000, NULL);
    g_signal_connect (G_OBJECT (self->chain), "frame-ready",
            G_CALLBACK (on_frame_ready), self);

//    camview_gtk_quit_on_interrupt ();
    gtk_main ();

    // halt and destroy chain
    cam_unit_chain_set_desired_status (self->chain, CAM_UNIT_STATUS_IDLE);
    g_object_unref (self->chain);
    free (self);
    return 0;

failed:
    cam_unit_chain_set_desired_status (self->chain, CAM_UNIT_STATUS_IDLE);
    g_object_unref (self->chain);
    free (self);
    return 1;
}
