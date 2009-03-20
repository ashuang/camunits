camunitsplugin_LTLIBRARIES = \
		   framewave_resize.la

INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS) $(FRAMEWAVE_CFLAGS)

# resize
framewave_resize_la_SOURCES = resize.c 
framewave_resize_la_LDFLAGS = -avoid-version -module
framewave_resize_la_LIBADD = $(FRAMEWAVE_IMAGE_LIBS)
