libcamplugin_LTLIBRARIES = \
		   ipp_pyr_down_gauss.la ipp_resize.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) $(IPP_CFLAGS)

# gaussian downsample
ipp_pyr_down_gauss_la_SOURCES = pyr_down_gauss.c 
ipp_pyr_down_gauss_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_pyr_down_gauss_la_LIBADD = $(LIBCAM_LIBS) $(IPP_LIBS)

# resize
ipp_resize_la_SOURCES = resize.c 
ipp_resize_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_resize_la_LIBADD = $(LIBCAM_LIBS) $(IPP_LIBS)
