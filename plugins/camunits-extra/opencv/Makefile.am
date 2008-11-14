camunitsplugin_LTLIBRARIES = \
		   opencv_undistort.la

INCLUDES = $(GLIB_CFLAGS) $(CAMUNITS_CFLAGS) $(OPENCV_CFLAGS)

# undistort
opencv_undistort_la_SOURCES = undistort.c 
opencv_undistort_la_LDFLAGS = -avoid-version -module 
opencv_undistort_la_LIBADD = $(CAMUNITS_LIBS) $(OPENCV_LIBS)
