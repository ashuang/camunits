if HAVE_CVD

camunitsplugin_LTLIBRARIES = demo_libcvd_fast.la

INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS) -msse -msse2

# FAST feature detector demo
demo_libcvd_fast_la_SOURCES = demo_libcvd_fast.cpp
demo_libcvd_fast_la_LDFLAGS = -avoid-version -module 
demo_libcvd_fast_la_LIBADD = -lcvd

endif
