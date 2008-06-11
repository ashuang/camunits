EXTRA_DIST = segment

libcamplugin_LTLIBRARIES = demo_fh_segment.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS)

# Felzenswalb Huttenlocher graph based segmentation demo
demo_fh_segment_la_SOURCES = demo_fh_segment.cpp glib_util.c
demo_fh_segment_la_LDFLAGS = -avoid-version -module 
demo_fh_segment_la_LIBADD = $(LIBCAM_LIBS)
