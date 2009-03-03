camunitsplugin_LTLIBRARIES = input_log.la \
							 output_logger.la \
							 filter_gl.la \
							 input_example.la 

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

input_log_la_SOURCES = input_log.c 
input_log_la_LDFLAGS = -avoid-version -module

output_logger_la_SOURCES = output_logger.c 
output_logger_la_LDFLAGS = -avoid-version -module

filter_gl_la_SOURCES = filter_gl.c 
filter_gl_la_LDFLAGS = -avoid-version -module

input_example_la_SOURCES = input_example.c 
input_example_la_LDFLAGS = -avoid-version -module $(JPEG_LIBS)
