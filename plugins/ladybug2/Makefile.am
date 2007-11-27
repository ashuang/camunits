EXTRA_LTLIBRARIES = input_ladybug2.la

INCLUDES = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS)

input_ladybug2_la_SOURCES = \
	input_ladybug2.c \
	input_ladybug2.h

input_ladybug2_la_LDFLAGS = -avoid-version -module
input_ladybug2_la_LIBADD = -ldc1394
