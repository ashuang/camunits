libcamplugin_LTLIBRARIES = demo_harris.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) $(OPENCV_CFLAGS)

# Harris corners demo
demo_harris_la_SOURCES = demo_harris.c demo_harris.h 
demo_harris_la_LDFLAGS = -avoid-version -module 
demo_harris_la_LIBADD = $(LIBCAM_LIBS) $(OPENCV_LIBS)
