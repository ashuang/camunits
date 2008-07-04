INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS)

libcamplugin_LTLIBRARIES = throttle.la

# throttle unit
throttle_la_SOURCES = \
	throttle.c \
	throttle.h 

throttle_la_LDFLAGS = -avoid-version -module 
throttle_la_LIBADD = $(LIBCAM_LIBS) $(LCM_LIBS)
