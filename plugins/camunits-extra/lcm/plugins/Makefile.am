if HAVE_LCM

# lcmtypes

camlcm_c_files= camlcm_image_sync_t.c \
			   camlcm_image_legacy_sync_t.c \
			   camlcm_image_metadata_t.c \
			   camlcm_image_t.c \
			   camlcm_image_legacy_t.c

camlcm_h_files= camlcm_image_sync_t.h \
			   camlcm_image_legacy_sync_t.h \
			   camlcm_image_metadata_t.h \
			   camlcm_image_t.h \
			   camlcm_image_legacy_t.h

$(camlcm_c_files): %.c: ../lcmtypes/%.lcm
	lcm-gen -c $< 
$(camlcm_h_files): %.h: ../lcmtypes/%.lcm
	lcm-gen -c $< 

CLEANFILES: $(camlcm_c_files) $(camlcm_h_files)

camunitsplugin_LTLIBRARIES = camlcm_publish.la \
						   camlcm_input.la \
						   camlcm_sync_publish.la \
						   camlcm_input_log_sync.la \
						   camlcm_input_legacy.la

INCLUDES = -I$(top_srcdir)/src $(GLIB_CFLAGS) $(Camunits_CFLAGS) $(LCM_CFLAGS)

# publish unit
camlcm_publish_la_SOURCES = \
	$(camlcm_c_files) $(camlcm_h_files) \
	lcm_publish.c

camlcm_publish_la_LDFLAGS = -avoid-version -module 
camlcm_publish_la_LIBADD = $(LCM_LIBS)

lcm_publish.c lcm_publish.h: $(camlcm_c_files) $(camlcm_h_files)

# input unit
camlcm_input_la_SOURCES = \
	$(camlcm_c_files) $(camlcm_h_files) \
	lcm_input.c

camlcm_input_la_LDFLAGS = -avoid-version -module 
camlcm_input_la_LIBADD = $(LCM_LIBS)

lcm_input.c lcm_input.h: $(camlcm_c_files) $(camlcm_h_files)

# input log sync unit
camlcm_input_log_sync_la_SOURCES = \
	$(camlcm_c_files) $(camlcm_h_files) \
	lcm_input_log_sync.c \
	lcm_input_log_sync.h 

camlcm_input_log_sync_la_LDFLAGS = -avoid-version -module 
camlcm_input_log_sync_la_LIBADD = $(LCM_LIBS)

lcm_input_log_sync.c lcm_input_log_sync.h: $(camlcm_c_files) $(camlcm_h_files)

# sync publish unit
camlcm_sync_publish_la_SOURCES = \
	$(camlcm_c_files) $(camlcm_h_files) \
	lcm_sync_publish.c \
	lcm_sync_publish.h 

camlcm_sync_publish_la_LDFLAGS = -avoid-version -module 
camlcm_sync_publish_la_LIBADD = $(LCM_LIBS)

lcm_sync_publish.c lcm_sync_publish.h: $(camlcm_c_files) $(camlcm_h_files)

# legacy input unit
camlcm_input_legacy_la_SOURCES = \
	$(camlcm_c_files) $(camlcm_h_files) \
	lcm_input_legacy.c \
	lcm_input_legacy.h 

camlcm_input_legacy_la_LDFLAGS = -avoid-version -module 
camlcm_input_legacy_la_LIBADD = $(LCM_LIBS)

lcm_input_legacy.c lcm_input_legacy.h: $(camlcm_c_files) $(camlcm_h_files)

endif
