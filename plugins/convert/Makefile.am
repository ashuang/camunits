camunitsplugin_LTLIBRARIES = convert_to_rgb8.la \
							 filter_fast_bayer.la \
							 convert_colorspace.la \
							 convert_jpeg_compress.la \
							 convert_jpeg_decompress.la

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

convert_to_rgb8_la_SOURCES = convert_to_rgb8.c 
convert_to_rgb8_la_LDFLAGS = -avoid-version -module

convert_colorspace_la_SOURCES = convert_colorspace.c 
convert_colorspace_la_LDFLAGS = -avoid-version -module

filter_fast_bayer_la_SOURCES = filter_fast_bayer.c 
filter_fast_bayer_la_LDFLAGS = -avoid-version -module

convert_jpeg_compress_la_SOURCES = convert_jpeg_compress.c 
convert_jpeg_compress_la_LDFLAGS = -avoid-version -module $(JPEG_LIBS)

convert_jpeg_decompress_la_SOURCES = convert_jpeg_decompress.c 
convert_jpeg_decompress_la_LDFLAGS = -avoid-version -module $(JPEG_LIBS)
