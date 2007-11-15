INCLUDES = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS)

lib_LTLIBRARIES = libcam-gtk.la

libcam_gtk_la_LDFLAGS = -version-info @LIBCAM_GTK_VERSION_INFO@
libcam_gtk_la_SOURCES = \
	cam_gtk.h \
	gl_drawing_area.c \
	gl_drawing_area.h \
	unit_chain_gl_widget.c \
	unit_chain_gl_widget.h \
	unit_chain_widget.c \
	unit_chain_widget.h \
	unit_control_widget.c \
	unit_control_widget.h \
	unit_manager_widget.c \
	unit_manager_widget.h 

libcam_gtk_la_LIBADD = \
	$(GTK_LIBS)

libcamincludedir = $(includedir)/libcam
libcaminclude_HEADERS = \
	cam_gtk.h \
	unit_chain_gl_widget.h \
	unit_chain_widget.h \
	unit_control_widget.h \
	unit_manager_widget.h \
	gl_drawing_area.h
