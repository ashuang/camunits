SUBDIRS = klt 

camunitsplugin_LTLIBRARIES = demo_klt_stb.la

INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS) 

# KLT demo
demo_klt_stb_la_SOURCES = demo_klt_stb.c color_util.h
demo_klt_stb_la_LDFLAGS = -avoid-version -module 
demo_klt_stb_la_LIBADD = klt/libklt.la
