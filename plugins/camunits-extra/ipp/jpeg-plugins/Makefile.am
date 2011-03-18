INCLUDES = $(GLIB_CFLAGS) $(Camunits_CFLAGS) $(IPP_CFLAGS)

camunitsplugin_LTLIBRARIES = ipp_jpeg_compress.la \
						   ipp_jpeg_decompress.la

jpeg_sources = jcapimin.c \
					jcapistd.c \
					jccoefct.c \
					jccolor.c \
					jcdctmgr.c \
					jchuff.c \
					jchuff.h \
					jcinit.c \
					jcmainct.c \
					jcmarker.c \
					jcmaster.c \
					jcomapi.c \
					jcparam.c \
					jcphuff.c \
					jcprepct.c \
					jcsample.c \
					jctrans.c \
					jdapimin.c \
					jdapistd.c \
					jdatadst.c \
					jdatasrc.c \
					jdcoefct.c \
					jdcolor.c \
					jddctmgr.c \
					jdct.h \
					jdhuff.c \
					jdhuff.h \
					jdinput.c \
					jdmainct.c \
					jdmarker.c \
					jdmaster.c \
					jdmerge.c \
					jdphuff.c \
					jdpostct.c \
					jdsample.c \
					jdtrans.c \
					jerror.c \
					jfdctflt.c \
					jfdctfst.c \
					jfdctint.c \
					jidctflt.c \
					jidctfst.c \
					jidctint.c \
					jidctred.c \
					jmemmgr.c \
					jmemnobs.c \
					jquant1.c \
					jquant2.c \
					jutils.c \
					jconfig.h \
					jerror.h \
					jmorecfg.h \
					jpeglib.h \
					jversion.h \
					jinclude.h \
					jmemsys.h \
					jpegint.h \
					jpegipp.h 

ipp_jpeg_compress_la_SOURCES = $(jpeg_sources) \
				   convert_ipp_jpeg_compress.c

ipp_jpeg_decompress_la_SOURCES = $(jpeg_sources) \
					 convert_ipp_jpeg_decompress.c

ipp_jpeg_compress_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_jpeg_compress_la_LIBADD = $(IPP_JPEG_LIBS)

ipp_jpeg_decompress_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
ipp_jpeg_decompress_la_LIBADD = $(IPP_JPEG_LIBS)
