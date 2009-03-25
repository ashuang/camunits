SUBDIRS = pixmaps

AM_CFLAGS += \
	-DCAMUNITS_PIXMAP_PATH=\""$(camunitspixmapdir)"\"

INCLUDES = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS)

lib_LTLIBRARIES = libcamunits-gtk.la

libcamunits_gtk_la_LDFLAGS = -version-info @CAMUNITS_GTK_VERSION_INFO@
libcamunits_gtk_la_SOURCES = \
	cam-gtk.h \
	gl_drawing_area.c \
	gl_drawing_area.h \
	unit_chain_gl_widget.c \
	unit_chain_gl_widget.h \
	unit_chain_widget.c \
	unit_chain_widget.h \
	unit_control_widget.c \
	unit_control_widget.h \
	unit_manager_widget.c \
	unit_manager_widget.h \
	cam_tree_store.c \
	cam_tree_store.h \
	unit_description_widget.c \
	unit_description_widget.h

libcamunits_gtk_la_LIBADD = $(GTK_LIBS) ../camunits/libcamunits.la

camunitsgtkincludedir = $(includedir)/camunits-gtk
camunitsgtkinclude_HEADERS = \
	cam-gtk.h \
	unit_chain_gl_widget.h \
	unit_chain_widget.h \
	unit_control_widget.h \
	unit_manager_widget.h \
	unit_description_widget.h \
	gl_drawing_area.h \
	cam_tree_store.h 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = camunits-gtk.pc
EXTRA_DIST = camunits-gtk.pc.in
