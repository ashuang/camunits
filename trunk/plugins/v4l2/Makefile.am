if WITH_V4L2_PLUGIN
camunitsplugin_LTLIBRARIES = input_v4l2.la
endif

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

input_v4l2_la_SOURCES = input_v4l2.c 
input_v4l2_la_LDFLAGS = -avoid-version -module
