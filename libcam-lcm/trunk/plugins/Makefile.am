libcamplugin_LTLIBRARIES = unit_publish.la unit_input.la

INCLUDES = $(GLIB_CFLAGS) $(LIBCAM_CFLAGS) $(LCM_CFLAGS)

# publish unit
unit_publish_la_SOURCES = \
	$(lcmtypes_cfiles) $(lcmtypes_hfiles) \
	unit_publish.c \
	unit_publish.h 

unit_publish_la_LDFLAGS = -avoid-version -module 
unit_publish_la_LIBADD = $(LIBCAM_LIBS) $(LCM_LIBS)

unit_publish.c unit_publish.h: $(lcmtypes_cfiles) $(lcmtypes_hfiles)

# input unit
unit_input_la_SOURCES = \
	$(lcmtypes_cfiles) $(lcmtypes_hfiles) \
	unit_input.c \
	unit_input.h 

unit_input_la_LDFLAGS = -avoid-version -module 
unit_input_la_LIBADD = $(LIBCAM_LIBS) $(LCM_LIBS)

unit_input.c unit_input.h: $(lcmtypes_cfiles) $(lcmtypes_hfiles)


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
