INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS) $(OPENCV_CFLAGS)

if HAVE_OPENCV

camunitsplugin_LTLIBRARIES = \
		   opencv_undistort.la \
		   opencv_demo_good_features_to_track.la \
		   opencv_demo_canny.la \
		   opencv_demo_object_detection.la

endif

# undistort
opencv_undistort_la_SOURCES = undistort.c 
opencv_undistort_la_LDFLAGS = -avoid-version -module 
opencv_undistort_la_LIBADD = $(OPENCV_LIBS)

# GoodFeaturesToTrack
opencv_demo_good_features_to_track_la_SOURCES = demo_good_features_to_track.c 
opencv_demo_good_features_to_track_la_LDFLAGS = -avoid-version -module 
opencv_demo_good_features_to_track_la_LIBADD = $(OPENCV_LIBS)

# Canny
opencv_demo_canny_la_SOURCES = demo_canny.c 
opencv_demo_canny_la_LDFLAGS = -avoid-version -module 
opencv_demo_canny_la_LIBADD = $(OPENCV_LIBS)

# Object Detection (Haar Classifier Cascade)
opencv_demo_object_detection_la_SOURCES = demo_object_detection.c 
opencv_demo_object_detection_la_LDFLAGS = -avoid-version -module 
opencv_demo_object_detection_la_LIBADD = $(OPENCV_LIBS)
