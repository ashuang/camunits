#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

#include <gtk/gtk.h>

#include <libcam/cam.h>
#include <libcam-gtk/cam_gtk.h>

typedef struct _state_t {
    CamUnitChain *chain;

    CamUnitChainWidget *chain_widget;
    CamUnitManagerWidget *manager_widget;
    CamUnitChainGLWidget *chain_gl_widget;

    int framecount;

    char *cmdline_input_id;

    GtkWindow *window;
    GtkWidget *manager_frame;
    GtkWidget *chain_frame;
} state_t;

state_t *g_self;

// ==================== signal handlers =====================

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, const CamFrameBuffer *buf, 
        void *user_data)
{
    state_t *self = (state_t*) user_data;

    cam_unit_chain_gl_widget_request_redraw (self->chain_gl_widget);

    self->framecount++;
}

void
on_open_menu_item_activate (GtkWidget *widget, void *nil)
{
    state_t *self = g_self;

    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new ("Log File",
            self->window,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        char *unit_id = g_strjoin ("", "input.log:", filename, NULL);
        cam_unit_manager_find_unit_description (self->chain->manager, unit_id);
        free (unit_id);
        g_free (filename);
    }

    gtk_widget_hide (dialog);
}

static void
on_show_manager_mi_toggled (GtkCheckMenuItem *mi, void *user_data)
{
    state_t *self = user_data;
    if (gtk_check_menu_item_get_active (mi)) {
        gtk_widget_show (self->manager_frame);
    } else {
        gtk_widget_hide (self->manager_frame);
    }
}

static void
on_show_chain_mi_toggled (GtkCheckMenuItem *mi, void *user_data)
{
    state_t *self = user_data;
    if (gtk_check_menu_item_get_active (mi)) {
        gtk_widget_show (self->chain_frame);
    } else {
        gtk_widget_hide (self->chain_frame);
    }
}

static void
on_unit_description_activated (CamUnitManagerWidget *mw, 
        CamUnitDescription *udesc, void *user_data)
{
    state_t *self = user_data;
    cam_unit_chain_add_unit_by_id (self->chain, udesc->unit_id);
}

// ========== administrative methods (construction, destruction) ==========

void
setup_gtk (state_t *self)
{
    self->window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(self->window, "Camview");
    gtk_window_set_resizable(self->window, TRUE);
    gtk_window_set_default_size(self->window, 800, 540);
    gtk_signal_connect (GTK_OBJECT (self->window), "delete_event", 
            GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (self->window), vbox);

    GtkWidget *menubar = gtk_menu_bar_new ();
    gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
    
    GtkWidget *file_menu_item = gtk_menu_item_new_with_mnemonic ("_File");
    gtk_menu_bar_append (GTK_MENU_BAR (menubar), file_menu_item);
    GtkWidget *file_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (file_menu_item), file_menu);
    
    GtkWidget *quit_mi = 
        gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
    gtk_menu_append (GTK_MENU (file_menu), quit_mi);
    gtk_signal_connect (GTK_OBJECT (quit_mi), "activate", 
            GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

    GtkWidget *hpane1 = gtk_hpaned_new ();
    gtk_box_pack_start (GTK_BOX (vbox), hpane1, TRUE, TRUE, 0);
    
    GtkWidget *hpane2 = gtk_hpaned_new ();
    gtk_paned_pack2 (GTK_PANED(hpane1), hpane2, TRUE, TRUE);

    // setup the chain widget
    self->chain_widget = cam_unit_chain_widget_new (self->chain);

    // setup the manager widget
    self->manager_widget = cam_unit_manager_widget_new (self->chain->manager);
    GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (self->manager_widget));
    gtk_paned_pack1 (GTK_PANED (hpane1), sw, FALSE, TRUE);
//    gtk_widget_show (GTK_WIDGET (self->manager_widget));
//    gtk_widget_show (sw);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), 
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    g_signal_connect (G_OBJECT (self->manager_widget), 
            "unit-description-activated", 
            G_CALLBACK (on_unit_description_activated), self);

    // setup the chain gl widget
    self->chain_gl_widget = cam_unit_chain_gl_widget_new (self->chain);
    gtk_paned_pack1 (GTK_PANED (hpane2), 
            GTK_WIDGET (self->chain_gl_widget), TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED(hpane2), GTK_WIDGET (self->chain_widget), 
            FALSE, TRUE);
    gtk_widget_show (GTK_WIDGET (self->chain_widget));

    gtk_widget_show_all (GTK_WIDGET (self->window));
}

int
state_setup (state_t *self)
{
    self->framecount = 0;

    // create the image processing chain
    self->chain = cam_unit_chain_new ();
    cam_unit_chain_set_desired_status (self->chain, CAM_UNIT_STATUS_STREAMING);
    cam_unit_chain_attach_glib_mainloop (self->chain, 1000);
    g_signal_connect (G_OBJECT (self->chain), "frame-ready",
            G_CALLBACK (on_frame_ready), self);

    // setup the GUI
    setup_gtk (self);

    // was an input ID specified on the command line?
    if (self->cmdline_input_id) {
        cam_unit_chain_add_unit_by_id (self->chain, self->cmdline_input_id);
    }

    return 0;
}

int 
state_cleanup (state_t *self)
{
    // halt and destroy chain
    cam_unit_chain_set_desired_status (self->chain, CAM_UNIT_STATUS_IDLE);
    g_object_unref (self->chain);

    if (self->cmdline_input_id) free (self->cmdline_input_id);

    return 0;
}

void 
usage (const char *progname)
{
    fprintf (stderr, "usage: %s\n", progname);
}

int main (int argc, char **argv)
{
    g_self = (state_t*) calloc (1, sizeof (state_t));
    state_t *self = g_self;
    self->cmdline_input_id = NULL;

    char *optstring = "hi:C:";
    char c;
    struct option long_opts[] = { 
        { "help", no_argument, 0, 'h' },
        { "input", required_argument, 0, 'i' },
        { 0, 0, 0, 0 }
    };

    gtk_init (&argc, &argv);

    g_thread_init (NULL);

    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0)
    {
        switch (c) {
            case 'i':
                self->cmdline_input_id = strdup (optarg);
                break;
            case 'h':
            default:
                usage (argv[0]);
                return 1;
        };
    }

    if (0 != state_setup (self)) return 1;

//    cam_gtk_quit_on_interrupt ();
    gtk_main ();

    state_cleanup (self);
    free (self);

    return 0;
}
