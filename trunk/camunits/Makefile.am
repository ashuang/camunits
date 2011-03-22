AM_CFLAGS += \
	-DCAMUNITS_PLUGIN_PATH=\""$(CAMUNITS_PLUGIN_PATH)"\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/camunits \
	$(GLIB_CFLAGS)

lib_LTLIBRARIES = libcamunits.la

libcamunits_la_LDFLAGS = -version-info @CAMUNITS_VERSION_INFO@
libcamunits_la_SOURCES = \
	unit.c \
	unit_driver.c \
	unit_manager.c \
	unit_chain.c \
	framebuffer.c \
	unit_format.c \
	unit_control.c \
	camunits-gmarshal.c \
	plugin.c \
	pixels.c \
	log.c \
	log.h \
	gl_texture.c \
	cpuid.h \
	dbg.h

libcamunits_la_LIBADD = $(GLIB_LIBS) $(GL_LIBS)

if INTEL
libcamunits_la_SOURCES += cpuid.c

noinst_LTLIBRARIES = libcamunits_sse2.la libcamunits_sse3.la

libcamunits_sse2_la_CFLAGS = -msse2 -g
libcamunits_sse2_la_SOURCES = \
	pixels_sse2.c \
	pixels_sse2.h

libcamunits_sse3_la_CFLAGS = -msse3 -g
libcamunits_sse3_la_SOURCES = \
	pixels_sse3.c \
	pixels_sse3.h

libcamunits_la_LIBADD += libcamunits_sse3.la libcamunits_sse2.la
else
libcamunits_la_SOURCES += cpuid_generic.c
endif

camunitsincludedir = $(includedir)/camunits
camunitsinclude_HEADERS = \
	cam.h \
	unit.h \
	unit_driver.h \
	unit_manager.h \
	unit_chain.h \
	framebuffer.h \
	unit_format.h \
	unit_control.h \
	camunits-gmarshal.h \
	plugin.h \
	pixels.h \
	log.h \
	gl_texture.h \
	dbg.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = camunits.pc
EXTRA_DIST = camunits.pc.in
