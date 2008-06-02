#ifndef __camview_gtk_util_h__
#define __camview_gtk_util_h__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Adds an event handler to the GTK mainloop that calls gtk_main_quit() when 
 * SIGINT, SIGTERM, or SIGHUP are received
 */
int camview_gtk_quit_on_interrupt (void);

int camview_g_quit_on_interrupt (GMainLoop *mainloop);

#ifdef __cplusplus
}
#endif

#endif
