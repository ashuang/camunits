AC_PREREQ(2.50)
AC_INIT(lcm/plugins/lcm_publish.c)
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE(camunits-extra, 0.3.0)
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC

dnl check for GLib
AM_PATH_GLIB_2_0(,,,gthread gobject gmodule)
PKG_PROG_PKG_CONFIG

dnl check for camunits
PKG_CHECK_MODULES(Camunits, [camunits >= 0.2.0],,
                  [AC_MSG_ERROR([Camunits is not found, but is required by camunits-extra.])])

dnl check for lcm
AC_ARG_WITH(lcm-plugins,
            [AS_HELP_STRING([--without-lcm-plugins],
             [Don't compile LCM plugins])],
            [], [with_lcm_plugins=yes])
if test x$with_lcm_plugins = xyes; then
    PKG_CHECK_MODULES(LCM, [lcm >= 0.2.1],,
           [AC_MSG_WARN([LCM not found, LCM plugins will not be built.])
            LCM_RSN="LCM not found"])
fi
AM_CONDITIONAL(HAVE_LCM, test "x$LCM_LIBS" != x)

dnl check for IPP
AC_ARG_WITH(ipp-plugins,
            [AS_HELP_STRING([--without-ipp-plugins],
             [Don't compile IPP plugins])],
            [], [with_ipp_plugins=yes])
if test x$with_ipp_plugins = xyes; then
    AM_PATH_IPP(,,
           [AC_MSG_WARN([IPP not found, IPP plugins will not be built.])
            IPP_RSN="IPP not found"])
fi
AM_CONDITIONAL(HAVE_IPP, test "x$IPP_LIBS" != x)

dnl check for Framewave
AC_ARG_WITH(framewave-plugins,
            [AS_HELP_STRING([--without-framewave-plugins],
             [Don't compile Framewave plugins])],
            [], [with_framewave_plugins=yes])
if test x$with_framewave_plugins = xyes; then
    AM_PATH_FRAMEWAVE(,,
           [AC_MSG_WARN([Framewave not found, Framewave plugins will not be built.])
            FRAMEWAVE_RSN="Framewave not found"])
fi
AM_CONDITIONAL(HAVE_FRAMEWAVE, test "x$FRAMEWAVE_LIBS" != x)

dnl check for libCVD
AC_ARG_WITH(cvd-plugins,
            [AS_HELP_STRING([--without-cvd-plugins],
             [Don't compile libCVD plugins])],
            [], [with_cvd_plugins=yes])
if test x$with_cvd_plugins = xyes; then
    AC_MSG_CHECKING(for libCVD)
    have_cvd=yes
    AC_LANG_PUSH(C++)
    AC_CHECK_HEADERS(cvd/image.h,[],[have_cvd=no
                                  CVD_RSN="CVD not found"])
    AC_CHECK_LIB(cvd, main,[],[have_cvd=no
                            CVD_RSN="CVD not found"])
    AC_LANG_POP(C++)
fi
AM_CONDITIONAL(HAVE_CVD, test x$have_cvd = xyes)

dnl opencv
AC_ARG_WITH(opencv-plugins,
            [AS_HELP_STRING([--without-opencv-plugins],
             [Don't compile OpenCV plugins])],
            [], [with_opencv_plugins=yes])
if test x$with_opencv_plugins = xyes; then
    PKG_CHECK_MODULES(OPENCV, [opencv >= 0.96],,
          [AC_MSG_WARN([OpenCV not found, OpenCV plugins will not be built.])
          OPENCV_RSN="OpenCV not found"])
fi
AM_CONDITIONAL(HAVE_OPENCV, test "x$OPENCV_LIBS" != x)

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

camunitsextradatadir="$datadir/camunits-extra"
AC_SUBST(camunitsextradatadir)
camunitsplugindir="$libdir/camunits"
AC_SUBST(camunitsplugindir)

dnl ---------------------------------------------------------------------------
dnl i18n support
dnl ---------------------------------------------------------------------------
ALL_LINGUAS=""

dnl The gettext domain of the library
GETTEXT_PACKAGE=${PACKAGE}
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[The gettext domain for the library])
AC_SUBST(GETTEXT_PACKAGE)
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.14.3)

AM_ICONV()

GTK_DOC_CHECK([1.4])

dnl ---------------------------------------------------------------------------
dnl Warnings
dnl ---------------------------------------------------------------------------
if test "x$GCC" = "xyes"; then
    AM_CFLAGS="$AM_CFLAGS -g -Wall -std=gnu99 -Wmissing-declarations -Wmissing-prototypes -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
    AM_LDFLAGS="$AM_LDFLAGS -g -Wall"
    AC_SUBST(AM_CFLAGS)
    AC_SUBST(AM_LDFLAGS)
fi

AC_OUTPUT([
  Makefile
  lcm/Makefile
  lcm/plugins/Makefile
  lcm/lcmtypes/Makefile
  ipp/Makefile
  ipp/plugins/Makefile
  ipp/jpeg-plugins/Makefile
  framewave/Makefile
  framewave/plugins/Makefile
  framewave/jpeg-plugins/Makefile
  klt-stb/Makefile
  klt-stb/klt/Makefile
  cvd/Makefile
  opencv/Makefile
  util/Makefile
  progs/Makefile
  docs/Makefile
  docs/build/Makefile
  po/Makefile.in
])

if test "x$LCM_LIBS" = x; then
    LCM_MSG="Disabled $LCM_RSN"
else
    LCM_MSG="Enabled"
fi

if test "x$IPP_LIBS" = x; then
    IPP_MSG="Disabled $IPP_RSN"
else
    IPP_MSG="Enabled"
fi

if test "x$FRAMEWAVE_LIBS" = x; then
    FRAMEWAVE_MSG="Disabled $FRAMEWAVE_RSN"
else
    FRAMEWAVE_MSG="Enabled"
fi

KLTSTB_MSG="Enabled"
if test x$have_cvd = xyes; then
    CVD_MSG="Enabled"
else
    CVD_MSG="Disabled $CVD_RSN"
fi

if test "x$OPENCV_LIBS" = x; then
    OPENCV_MSG="Disabled $OPENCV_RSN"
else
    OPENCV_MSG="Enabled"
fi

echo "

Configuration (camunits-extra):

	Source code location:              ${srcdir}
	Compiler:                          ${CC}
	LCM plugins:                       ${LCM_MSG}
	IPP plugins:                       ${IPP_MSG}
	Framewave plugins:                 ${FRAMEWAVE_MSG}
	KLT (Birchfield) plugin:           ${KLTSTB_MSG}
	libCVD (FAST features) plugins:    ${CVD_MSG}
	OpenCV plugins:                    ${OPENCV_MSG}
"
