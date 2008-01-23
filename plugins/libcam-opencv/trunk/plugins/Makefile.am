libcamplugin_LTLIBRARIES = demo_harris.la demo_opencv_canny.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) $(OPENCV_CFLAGS)

# Harris corners demo
demo_harris_la_SOURCES = demo_harris.c demo_harris.h 
demo_harris_la_LDFLAGS = -avoid-version -module 
demo_harris_la_LIBADD = $(LIBCAM_LIBS) $(OPENCV_LIBS)

# Canny Edge demo
demo_opencv_canny_la_SOURCES = demo_opencv_canny.c demo_opencv_canny.h 
demo_opencv_canny_la_LDFLAGS = -avoid-version -module 
demo_opencv_canny_la_LIBADD = $(LIBCAM_LIBS) $(OPENCV_LIBS)
