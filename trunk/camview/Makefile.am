INCLUDES = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) $(GLIB_CFLAGS)

bin_PROGRAMS = camview

camview_SOURCES = \
	camview.c \
	gtk_util.c \
	gtk_util.h

camview_LDADD = \
	$(GTK_LIBS) $(GLIB_LIBS) ../libcam-gtk/libcam-gtk.la ../libcam/libcam.la

man_MANS = camview.1

EXTRA_DIST = camview.1
