SUBDIRS = klt 

libcamplugin_LTLIBRARIES = demo_klt_stb.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) 

# KLT demo
demo_klt_stb_la_SOURCES = demo_klt_stb.c 
demo_klt_stb_la_LDFLAGS = -avoid-version -module 
demo_klt_stb_la_LIBADD = $(LIBCAM_LIBS) klt/libklt.la
