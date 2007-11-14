AM_CFLAGS = \
	-DLIBCAM_PLUGINS_PATH=\""$(libcamplugindir)"\" \
	-msse3

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcam \
	$(GLIB_CFLAGS)

lib_LTLIBRARIES = libcam.la

libcam_la_LDFLAGS = -version-info @LIBCAM_VERSION_INFO@
libcam_la_SOURCES = \
	unit.c \
	unit_driver.c \
	unit_manager.c \
	unit_chain.c \
	framebuffer.c \
	unit_format.c \
	unit_control.c \
	libcam-gmarshal.c \
	plugin.c \
	pixels.c \
	log.c \
	log.h \
	input_example.c \
	input_example.h \
	input_log.c \
	input_log.h \
	filter_gl.c \
	filter_gl.h \
	gl_texture.c \
	convert_colorspace.c \
	convert_colorspace.h \
	filter_fast_bayer.c \
	filter_fast_bayer.h \
	filter_jpeg.c \
	filter_jpeg.h

libcam_la_LIBADD = \
	$(GLIB_LIBS)

libcamincludedir = $(includedir)/libcam
libcaminclude_HEADERS = \
	cam.h \
	unit.h \
	unit_driver.h \
	unit_manager.h \
	unit_chain.h \
	framebuffer.h \
	unit_format.h \
	unit_control.h \
	libcam-gmarshal.h \
	plugin.h \
	pixels.h \
	log.h \
	gl_texture.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcam.pc
EXTRA_DIST = libcam.pc.in
