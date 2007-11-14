libcamplugin_LTLIBRARIES = input_dc1394.la

INCLUDES = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS)

input_dc1394_la_SOURCES = \
	input_dc1394.c \
	input_dc1394.h

input_dc1394_la_LDFLAGS = -avoid-version -module
input_dc1394_la_LIBADD = -ldc1394
