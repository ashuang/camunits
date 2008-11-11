INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

bin_PROGRAMS = camlog

camlog_SOURCES = camlog.c signal_pipe.c signal_pipe.h

camlog_LDADD = $(GLIB_LIBS) ../camunits/libcamunits.la

man_MANS = camlog.1

EXTRA_DIST = camlog.1
