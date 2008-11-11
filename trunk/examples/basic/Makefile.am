INCLUDES = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS)

noinst_PROGRAMS = snapshot trivial-acquire

snapshot_SOURCES = snapshot.c
trivial_acquire_SOURCES = trivial-acquire.c

LDADD = $(GLIB_LIBS) ../../camunits/libcamunits.la

