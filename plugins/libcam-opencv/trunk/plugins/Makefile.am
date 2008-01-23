libcamplugin_LTLIBRARIES = \
		   demo_good_features_to_track.la \
		   demo_opencv_canny.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) $(OPENCV_CFLAGS)

# cvGoodFeaturesToTrack demo
demo_good_features_to_track_la_SOURCES = demo_good_features_to_track.c 
demo_good_features_to_track_la_LDFLAGS = -avoid-version -module 
demo_good_features_to_track_la_LIBADD = $(LIBCAM_LIBS) $(OPENCV_LIBS)

# cvCanny demo
demo_opencv_canny_la_SOURCES = demo_canny.c 
demo_opencv_canny_la_LDFLAGS = -avoid-version -module 
demo_opencv_canny_la_LIBADD = $(LIBCAM_LIBS) $(OPENCV_LIBS)
