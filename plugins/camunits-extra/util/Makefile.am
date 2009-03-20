INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS)

camunitsplugin_LTLIBRARIES = camextra_throttle.la \
						   camextra_image_files.la \
						   camextra_snapshot.la \
						   camextra_convert_to_gray.la

# throttle unit
camextra_throttle_la_SOURCES = throttle.c
camextra_throttle_la_LDFLAGS = -avoid-version -module 

# file output unit
camextra_image_files_la_SOURCES = image_files.c
camextra_image_files_la_LDFLAGS = -avoid-version -module 

# snapshot unit
camextra_snapshot_la_SOURCES = snapshot.c 
camextra_snapshot_la_LDFLAGS = -avoid-version -module 

## single image file input unit
#camextra_input_single_file_la_SOURCES = input_single_file.c 
#camextra_input_single_file_la_LDFLAGS = -avoid-version -module 

# convert to grayscale unit
camextra_convert_to_gray_la_SOURCES = convert_to_gray.c 
camextra_convert_to_gray_la_LDFLAGS = -avoid-version -module 
