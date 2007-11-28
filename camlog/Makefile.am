INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

bin_PROGRAMS = camlog

camlog_SOURCES = camlog.c signal_pipe.c

camlog_LDADD = $(GLIB_LIBS) ../libcam/libcam.la -lGL -ljpeg

man_MANS = camlog.1

EXTRA_DIST = camlog.1
