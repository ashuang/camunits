INCLUDES = $(GLIB_CFLAGS) $(IPP_CFLAGS)

libcamplugin_LTLIBRARIES = convert_ipp_jpeg.la

convert_ipp_jpeg_la_SOURCES = jcapimin.c \
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
					jpegipp.h \
					convert_ipp_jpeg.c \
					convert_ipp_jpeg.h 

convert_ipp_jpeg_la_LDFLAGS = -avoid-version -module $(IPP_LDFLAGS)
convert_ipp_jpeg_la_LIBADD = $(LIBCAM_LIBS) $(IPP_JPEG_LIBS)
