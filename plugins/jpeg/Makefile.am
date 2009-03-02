#if WITH_JPEG_PLUGINS
camunitsplugin_LTLIBRARIES = convert_jpeg_compress.la \
							 convert_jpeg_decompress.la
#endif

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

convert_jpeg_compress_la_SOURCES = convert_jpeg_compress.c 
convert_jpeg_compress_la_LDFLAGS = -avoid-version -module $(JPEG_LIBS)

convert_jpeg_decompress_la_SOURCES = convert_jpeg_decompress.c 
convert_jpeg_decompress_la_LDFLAGS = -avoid-version -module $(JPEG_LIBS)
