if WITH_V4L1_PLUGIN
camunitsplugin_LTLIBRARIES = input_v4l.la
endif

INCLUDES = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS)

input_v4l_la_SOURCES = \
	input_v4l.c \
	pwc-ioctl.h

input_v4l_la_LDFLAGS = -avoid-version -module
