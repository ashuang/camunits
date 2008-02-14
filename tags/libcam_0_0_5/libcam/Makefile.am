AM_CFLAGS = \
	-DLIBCAM_PLUGINS_PATH=\""$(libcamplugindir)"\"

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcam \
	$(GLIB_CFLAGS)

lib_LTLIBRARIES = libcam.la
noinst_LTLIBRARIES = libcam_sse2.la libcam_sse3.la

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
	convert_to_rgb8.c \
	convert_to_rgb8.h \
	filter_fast_bayer.c \
	filter_fast_bayer.h \
	convert_jpeg_compress.c \
	convert_jpeg_compress.h \
	convert_jpeg_decompress.c \
	convert_jpeg_decompress.h \
	output_logger.c \
	output_logger.h \
	cpuid.c \
	cpuid.h \
	dbg.h \
	pwc-ioctl.h

libcam_sse2_la_CFLAGS = -msse2
libcam_sse2_la_SOURCES = \
	pixels_sse2.c \
	pixels_sse2.h

libcam_sse3_la_CFLAGS = -msse3
libcam_sse3_la_SOURCES = \
	pixels_sse3.c \
	pixels_sse3.h

if LINUX
libcam_la_SOURCES += input_v4l.c input_v4l.h input_v4l2.c input_v4l2.h
AM_CFLAGS += -DUSE_V4L -DUSE_V4L2
endif

libcam_la_LIBADD = $(GLIB_LIBS) $(JPEG_LIBS) $(GL_LIBS) \
		   libcam_sse3.la libcam_sse2.la

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
