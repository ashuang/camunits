if HAVE_CVD

libcamplugin_LTLIBRARIES = demo_libcvd_fast.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS)

# FAST feature detector demo
demo_libcvd_fast_la_SOURCES = demo_libcvd_fast.cpp
demo_libcvd_fast_la_LDFLAGS = -avoid-version -module 
demo_libcvd_fast_la_LIBADD = $(LIBCAM_LIBS) -lcvd

endif
