INCLUDES = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) $(GTK_CFLAGS)

noinst_PROGRAMS = filter_example

filter_example_SOURCES = filter_example.c filter_example.h main.c

LDADD = $(GLIB_LIBS) ../../camunits-gtk/libcamunits-gtk.la

