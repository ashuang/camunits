#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <camunits/cam.h>
#include <camunits-gtk/cam-gtk.h>

#include "gtk_util.h"

typedef struct _state_t {
    CamUnitChain *chain;
    CamUnitManager *manager;

    CamUnitChainWidget *chain_widget;
    CamUnitManagerWidget *manager_widget;
    CamUnitChainGLWidget *chain_gl_widget;
    CamUnitDescriptionWidget * desc_widget;

    char *xml_fname;
    char *extra_plugin_path;
    int use_gui;

    GtkWindow *window;
    GtkWidget *manager_frame;
    GtkWidget *chain_frame;

    int64_t last_frame_utime;
    double fps;
    double non_decayed_fps;
    int64_t last_fps_utime;
} state_t;

// ==================== signal handlers =====================

static void
update_fps_label(state_t * self, int64_t now)
{
    if(now - self->last_fps_utime < 300000) 
        return;
    GtkWidget *label = gtk_frame_get_label_widget(GTK_FRAME(self->chain_frame));
    char text[80];
    sprintf(text, "Chain (%.2f fps)", self->fps);
    gtk_label_set_text(GTK_LABEL(label), text);
    self->last_fps_utime = now;
}

static gboolean
decay_fps(void * user_data)
{
    state_t *self = (state_t*) user_data;
    struct timeval tv;
    gettimeofday (&tv, NULL);
    int64_t now = (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
    if(self->fps < 0.01 && self->fps > 0) {
        self->fps = 0;
        update_fps_label(self, now);
    } else {
        double dt = (now - self->last_frame_utime) * 1e-6;
        if(dt > 1.5 * 1 / self->non_decayed_fps) {
            self->fps *= 0.8;
            update_fps_label(self, now);
        }
    }
    return TRUE;
}

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, const CamFrameBuffer *buf, 
        void *user_data)
{
    state_t *self = (state_t*) user_data;
    if (self->use_gui) {
        cam_unit_chain_gl_widget_request_redraw (self->chain_gl_widget);

        // update FPS display
        struct timeval tv;
        gettimeofday (&tv, NULL);
        int64_t now = (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
        if(self->last_frame_utime) {
            double dt = (now - self->last_frame_utime) * 1e-6;
            double a = 0.5;
            self->fps = a * (1/dt) + (1-a) * self->fps;
            update_fps_label(self, now);
            self->non_decayed_fps = self->fps;
        }
        self->last_frame_utime = now;
    }
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
    cam_unit_chain_add_unit_by_id (self->chain, 
            cam_unit_description_get_unit_id(udesc));
}

static void
on_open_menu_item_activate (GtkWidget *widget, void * user)
{
    state_t *self = user;

    GtkWidget *dialog = gtk_file_chooser_dialog_new ("Load Saved Chain",
            self->window, GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        char *text = NULL;
        GError *gerr = NULL;
        if (g_file_get_contents (path, &text, NULL, &gerr)) {
            cam_unit_chain_load_from_str (self->chain, text, &gerr);
        }
        free (text);
        free (path);
        if (gerr) {
            GtkWidget *mdlg = gtk_message_dialog_new (self->window,
                    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE, "Error opening file '%s': %s",
                    path, gerr->message);
            gtk_dialog_run (GTK_DIALOG (mdlg));
            gtk_widget_destroy (mdlg);

            g_error_free (gerr);
        }
    }
    gtk_widget_destroy (dialog);
}


static void
on_save_menu_item_activate (GtkWidget *widget, void * user)
{
    state_t *self = user;

    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new ("Save Chain State",
            self->window, GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        char *chain_state = cam_unit_chain_snapshot (self->chain);
        GError *gerr = NULL;
        g_file_set_contents (path, chain_state, -1, &gerr);
        free (chain_state);
        free (path);

        if (gerr) {
            GtkWidget *mdlg = gtk_message_dialog_new (self->window,
                    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE, "Error saving file '%s': %s",
                    path, gerr->message);
            gtk_dialog_run (GTK_DIALOG (mdlg));
            gtk_widget_destroy (mdlg);
            g_error_free (gerr);
        }
    }

    gtk_widget_destroy (dialog);
}

// ========== administrative methods (construction, destruction) ==========

static void
setup_gtk (state_t *self)
{
    self->window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(self->window, "Camview");
    gtk_window_set_resizable(self->window, TRUE);
    gtk_window_set_default_size(self->window, 1000, 540);
    gtk_signal_connect (GTK_OBJECT (self->window), "delete_event", 
            GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (self->window), vbox);

    // menu bar
    GtkWidget *menubar = gtk_menu_bar_new ();
    gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
    
    // file menu
    GtkWidget *file_menu_item = gtk_menu_item_new_with_mnemonic ("_File");
    gtk_menu_bar_append (GTK_MENU_BAR (menubar), file_menu_item);
    GtkWidget *file_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (file_menu_item), file_menu);
    
    GtkWidget *open_mi = 
        gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, NULL);
    gtk_menu_append (GTK_MENU (file_menu), open_mi);
    gtk_signal_connect (GTK_OBJECT (open_mi), "activate", 
            GTK_SIGNAL_FUNC (on_open_menu_item_activate), self);
    
    GtkWidget *save_mi = 
        gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE, NULL);
    gtk_menu_append (GTK_MENU (file_menu), save_mi);
    gtk_signal_connect (GTK_OBJECT (save_mi), "activate", 
            GTK_SIGNAL_FUNC (on_save_menu_item_activate), self);
    
    GtkWidget *quit_mi = 
        gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
    gtk_menu_append (GTK_MENU (file_menu), quit_mi);
    gtk_signal_connect (GTK_OBJECT (quit_mi), "activate", 
            GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

    // view menu
    GtkWidget *view_menu_item = gtk_menu_item_new_with_mnemonic ("_View");
    gtk_menu_bar_append (GTK_MENU_BAR (menubar), view_menu_item);
    GtkWidget *view_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (view_menu_item), view_menu);

    GtkWidget *show_manager_mi = 
        gtk_check_menu_item_new_with_mnemonic ("Show _Manager");
    gtk_menu_append (GTK_MENU (view_menu), show_manager_mi);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_manager_mi), 
            TRUE);
    gtk_signal_connect (GTK_OBJECT (show_manager_mi), "toggled",
            GTK_SIGNAL_FUNC (on_show_manager_mi_toggled), self);
    GtkWidget *show_chain_mi = 
        gtk_check_menu_item_new_with_mnemonic ("Show _Chain");
    gtk_menu_append (GTK_MENU (view_menu), show_chain_mi);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_chain_mi), TRUE);
    gtk_signal_connect (GTK_OBJECT (show_chain_mi), "toggled",
            GTK_SIGNAL_FUNC (on_show_chain_mi_toggled), self);

    // horizontal panes
    GtkWidget *hpane1 = gtk_hpaned_new ();
    gtk_box_pack_start (GTK_BOX (vbox), hpane1, TRUE, TRUE, 0);
    self->manager_frame = gtk_frame_new ("Manager");
    gtk_paned_pack1 (GTK_PANED (hpane1), self->manager_frame, FALSE, TRUE);

    
    GtkWidget *hpane2 = gtk_hpaned_new ();
    gtk_paned_pack2 (GTK_PANED(hpane1), hpane2, TRUE, TRUE);

    self->chain_frame = gtk_frame_new ("Chain");
    GtkWidget *display_frame = gtk_frame_new ("Display");
    gtk_paned_pack1 (GTK_PANED(hpane2), display_frame, TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED(hpane2), self->chain_frame, FALSE, TRUE);

    gtk_paned_set_position (GTK_PANED (hpane1), 200);
    gtk_paned_set_position (GTK_PANED (hpane2), 400);


    // manager widget
    self->manager_widget = cam_unit_manager_widget_new ();
    GtkWidget *sw1 = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (sw1), GTK_WIDGET (self->manager_widget));
    //gtk_paned_pack1 (GTK_PANED (hpane1), sw, FALSE, TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw1), 
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    g_signal_connect (G_OBJECT (self->manager_widget), 
            "unit-description-activated", 
            G_CALLBACK (on_unit_description_activated), self);

    // description widget
    self->desc_widget = cam_unit_description_widget_new ();
    cam_unit_description_widget_set_manager (
            CAM_UNIT_DESCRIPTION_WIDGET (self->desc_widget),
            self->manager_widget);
    GtkWidget *sw2 = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw2), 
            GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (sw2), GTK_WIDGET (self->desc_widget));

    // vertical pane
    GtkWidget * vpane = gtk_vpaned_new ();
    gtk_paned_pack1 (GTK_PANED (vpane), sw1, TRUE, TRUE);
    gtk_paned_pack2 (GTK_PANED (vpane), sw2, FALSE, TRUE);
    gtk_paned_set_position (GTK_PANED (vpane), 350);
    //gtk_paned_pack1 (GTK_PANED (hpane1), vpane, FALSE, TRUE);
    gtk_container_add (GTK_CONTAINER (self->manager_frame), vpane);

    // chain gl widget
    self->chain_gl_widget = cam_unit_chain_gl_widget_new (self->chain);
    gtk_container_add (GTK_CONTAINER (display_frame), 
                GTK_WIDGET (self->chain_gl_widget));

    // chain widget
    self->chain_widget = cam_unit_chain_widget_new (self->chain);
    GtkWidget *sw_chain = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (self->chain_frame), sw_chain);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw_chain), 
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw_chain), 
            GTK_WIDGET (self->chain_widget));

    gtk_widget_show_all (GTK_WIDGET (self->window));

    g_timeout_add(250, decay_fps, self);
}

static int
state_setup (state_t *self)
{
    // create the image processing chain
    self->chain = cam_unit_chain_new();
    self->manager = cam_unit_manager_get_and_ref();

    // search for plugins in non-standard directories
    if(self->extra_plugin_path) {
        char **path_dirs = g_strsplit(self->extra_plugin_path, ":", 0);
        for (int i=0; path_dirs[i]; i++) {
            cam_unit_manager_add_plugin_dir (self->manager, path_dirs[i]);
        }
        g_strfreev (path_dirs);
        free(self->extra_plugin_path);
        self->extra_plugin_path = NULL;
    }

    // setup the GUI
    if (self->use_gui) {
        setup_gtk (self);
    } else {
        assert (self->xml_fname);
    }

    cam_unit_chain_all_units_stream_init (self->chain);
    cam_unit_chain_attach_glib (self->chain, 1000, NULL);
    g_signal_connect (G_OBJECT (self->chain), "frame-ready",
            G_CALLBACK (on_frame_ready), self);

    if (self->xml_fname) {
        char *xml_str = NULL;
        GError *err = NULL;
        g_file_get_contents (self->xml_fname, &xml_str, NULL, &err);
        if(err) {
            fprintf(stderr, "\n\nError loading chain from file!\n");
            fprintf(stderr, "==============================\n");
            fprintf(stderr, "%s\n", err->message);
            free(xml_str);
            return -1;
        }
        cam_unit_chain_load_from_str (self->chain, xml_str, NULL);
        free (xml_str);
    }

    return 0;
}

static int 
state_cleanup (state_t *self)
{
    // halt and destroy chain
    if(self->chain) {
        cam_unit_chain_all_units_stream_shutdown (self->chain);
        g_object_unref (self->chain);
    }
    if(self->manager)
        g_object_unref (self->manager);
    free (self->xml_fname);
    return 0;
}

static void 
usage()
{
    fprintf (stderr, "usage: camview [options]\n"
    "\n"
    "camview is a graphical tool for viewing and testing image processing\n"
    "chains.  It uses GTK+ 2.0, OpenGL, and libcamunits.\n"
    "\n"
    "Options:\n"
    "  -c, --chain NAME     Load chain from file NAME\n"
    "  --no-gui             Run without a GUI.  If --no-gui is specified,\n"
    "                       then -c is required.\n"
    "  --plugin-path PATH   Add the directories in PATH to the plugin\n"
    "                       search path.  PATH should be a colon-delimited\n"
    "                       list.\n"
    "  -h, --help           Show this help text and exit\n");
    exit(1);
}

int main (int argc, char **argv)
{
    state_t * self = (state_t*) calloc (1, sizeof (state_t));
    self->use_gui = 1;

    char *optstring = "hc:p:";
    int c;
    struct option long_opts[] = { 
        { "help", no_argument, 0, 'h' },
        { "chain", required_argument, 0, 'c' },
        { "plugin-path", required_argument, 0, 'p' },
        { "no-gui", no_argument, 0, 'u' },
        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0)
    {
        switch (c) {
            case 'c':
                self->xml_fname = strdup (optarg);
                break;
            case 'u':
                self->use_gui = 0;
                break;
            case 'p':
                self->extra_plugin_path = strdup (optarg);
                break;
            case 'h':
            default:
                usage ();
                break;
        };
    }

    if (!self->use_gui && !self->xml_fname) usage();

    if (self->use_gui) {
        gtk_init (&argc, &argv);
    } else {
        g_type_init ();
    }

    if (!g_thread_supported()) {
        g_thread_init (NULL);
    }

    if (0 != state_setup (self)) {
        state_cleanup(self);
        return 1;
    }

    if (self->use_gui) {
        camview_gtk_quit_on_interrupt ();
        gtk_main ();
        state_cleanup (self);
    } else {
        // did everything start up correctly?
        CamUnit *faulty = cam_unit_chain_all_units_stream_init(self->chain);
        if (faulty) {
            int faulty_index = 0;
            for(GList *uiter = cam_unit_chain_get_units(self->chain);
                    uiter && uiter->data != faulty; uiter = uiter->next)
                faulty_index ++;

            fprintf(stderr, "Unit %d [%s] is not streaming, aborting...\n",
                    faulty_index, cam_unit_get_name (faulty));
            return -1;
        }

        GMainLoop *mainloop = g_main_loop_new (NULL, FALSE);
        camview_g_quit_on_interrupt (mainloop);
        g_main_loop_run (mainloop);
        g_main_loop_unref (mainloop);
        state_cleanup (self);
    }

    return 0;
}
