INCLUDES = $(GLIB_CFLAGS) $(CAMUNITS_CFLAGS)

camunitsplugin_LTLIBRARIES = camextra_throttle.la \
						   camextra_image_files.la \
						   camextra_snapshot.la

# throttle unit
camextra_throttle_la_SOURCES = \
	throttle.c \
	throttle.h 

camextra_throttle_la_LDFLAGS = -avoid-version -module 
camextra_throttle_la_LIBADD = $(CAMUNITS_LIBS) $(LCM_LIBS)

# file output unit
camextra_image_files_la_SOURCES = \
	image_files.c \
	image_files.h 

camextra_image_files_la_LDFLAGS = -avoid-version -module 
camextra_image_files_la_LIBADD = $(CAMUNITS_LIBS) $(LCM_LIBS)

# file output unit
camextra_snapshot_la_SOURCES = \
	snapshot.c \
	snapshot.h 

camextra_snapshot_la_LDFLAGS = -avoid-version -module 
camextra_snapshot_la_LIBADD = $(CAMUNITS_LIBS) $(LCM_LIBS)
