libcamplugin_LTLIBRARIES = lcm_publish.la lcm_input.la lcm_sync_publish.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) $(LCM_CFLAGS)

# publish unit
lcm_publish_la_SOURCES = \
	$(lcmtypes_cfiles) $(lcmtypes_hfiles) \
	lcm_publish.c \
	lcm_publish.h 

lcm_publish_la_LDFLAGS = -avoid-version -module 
lcm_publish_la_LIBADD = $(LIBCAM_LIBS) $(LCM_LIBS)

lcm_publish.c lcm_publish.h: $(lcmtypes_cfiles) $(lcmtypes_hfiles)

# input unit
lcm_input_la_SOURCES = \
	$(lcmtypes_cfiles) $(lcmtypes_hfiles) \
	lcm_input.c \
	lcm_input.h 

lcm_input_la_LDFLAGS = -avoid-version -module 
lcm_input_la_LIBADD = $(LIBCAM_LIBS) $(LCM_LIBS)

lcm_input.c lcm_input.h: $(lcmtypes_cfiles) $(lcmtypes_hfiles)

# sync publish unit
lcm_sync_publish_la_SOURCES = \
	$(lcmtypes_cfiles) $(lcmtypes_hfiles) \
	lcm_sync_publish.c \
	lcm_sync_publish.h 

lcm_sync_publish_la_LDFLAGS = -avoid-version -module 
lcm_sync_publish_la_LIBADD = $(LIBCAM_LIBS) $(LCM_LIBS)

lcm_sync_publish.c lcm_sync_publish.h: $(lcmtypes_cfiles) $(lcmtypes_hfiles)


# lcmtypes

lcmtypes_cfiles = camlcm_announce_t.c \
		   camlcm_sync_t.c \
		   camlcm_key_byte_t.c \
		   camlcm_key_string_t.c \
		   camlcm_image_t.c 

lcmtypes_hfiles = camlcm_announce_t.h \
		   camlcm_sync_t.h \
		   camlcm_key_byte_t.h \
		   camlcm_key_string_t.h \
		   camlcm_image_t.h

$(lcmtypes_cfiles): camlcm_%.c: ../lcmtypes/%.lcm
	lcmgen -c $< 
$(lcmtypes_hfiles): camlcm_%.h: ../lcmtypes/%.lcm
	lcmgen -c $< 

clean-local:
	rm -f $(lcmtypes_cfiles) $(lcmtypes_hfiles)
