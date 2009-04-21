camunitsplugin_LTLIBRARIES = \
		   ipp_pyr_down_gauss.la \
		   ipp_resize.la \
		   ipp_filter_fixed_gauss.la \
		   ipp_filter_sobel.la

INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS) $(IPP_CFLAGS)

# gaussian downsample
ipp_pyr_down_gauss_la_SOURCES = pyr_down_gauss.c 
ipp_pyr_down_gauss_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_pyr_down_gauss_la_LIBADD = $(IPP_LIBS)

# resize
ipp_resize_la_SOURCES = resize.c 
ipp_resize_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_resize_la_LIBADD = $(IPP_LIBS)

# fixed gaussian filter
ipp_filter_fixed_gauss_la_SOURCES = filter_fixed_gauss.c 
ipp_filter_fixed_gauss_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_filter_fixed_gauss_la_LIBADD = $(IPP_LIBS)

# sobel filter
ipp_filter_sobel_la_SOURCES = filter_sobel.c 
ipp_filter_sobel_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_filter_sobel_la_LIBADD = $(IPP_LIBS)
