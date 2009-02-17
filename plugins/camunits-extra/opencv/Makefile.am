INCLUDES = $(GLIB_CFLAGS) $(CAMUNITS_CFLAGS) $(OPENCV_CFLAGS)

camunitsplugin_LTLIBRARIES = \
		   opencv_undistort.la \
		   opencv_demo_good_features_to_track.la \
		   opencv_demo_canny.la \
		   opencv_demo_object_detection.la

# undistort
opencv_undistort_la_SOURCES = undistort.c 
opencv_undistort_la_LDFLAGS = -avoid-version -module 
opencv_undistort_la_LIBADD = $(CAMUNITS_LIBS) $(OPENCV_LIBS)

# GoodFeaturesToTrack
opencv_demo_good_features_to_track_la_SOURCES = demo_good_features_to_track.c 
opencv_demo_good_features_to_track_la_LDFLAGS = -avoid-version -module 
opencv_demo_good_features_to_track_la_LIBADD = $(CAMUNITS_LIBS) $(OPENCV_LIBS)

# Canny
opencv_demo_canny_la_SOURCES = demo_canny.c 
opencv_demo_canny_la_LDFLAGS = -avoid-version -module 
opencv_demo_canny_la_LIBADD = $(CAMUNITS_LIBS) $(OPENCV_LIBS)

# Object Detection (Haar Classifier Cascade)
opencv_demo_object_detection_la_SOURCES = demo_object_detection.c 
opencv_demo_object_detection_la_LDFLAGS = -avoid-version -module 
opencv_demo_object_detection_la_LIBADD = $(CAMUNITS_LIBS) $(OPENCV_LIBS)
