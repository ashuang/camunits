INCLUDES = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) $(GLIB_CFLAGS)

bin_PROGRAMS = camview

camview_SOURCES = camview.c gtk_util.c

camview_LDADD = \
	$(GTK_LIBS) $(GLIB_LIBS) ../libcam/libcam.la ../libcam-gtk/libcam-gtk.la -lGL -ljpeg
