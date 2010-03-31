AC_PREREQ(2.50)
AC_INIT(camunits/unit.h)
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE(camunits, 0.3.0)
AC_CONFIG_MACRO_DIR([m4])

case "$target_os" in
	linux*) arch=linux;;
	darwin*)
		AM_CFLAGS="$AM_CFLAGS -I/usr/X11R6/include -I/opt/local/include"
		LDFLAGS="$LDFLAGS -L/usr/X11R6/lib -L/opt/local/lib -Wl,-dylib_file,/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib"
		;;
	*) arch=unknown;;
esac

AC_PROG_CC
AM_PATH_GLIB_2_0(,,,gthread gobject gmodule)
AM_PATH_GTK_2_0
AC_CHECK_LIB(GL, glBegin, GL_LIBS='-lGL',
             [AC_MSG_ERROR([OpenGL not found, but is required by camunits.])])
AC_CHECK_LIB(jpeg, jpeg_destroy_decompress, JPEG_LIBS='-ljpeg',
             [AC_MSG_ERROR([libjpeg not found, but is required by camunits.])])

AC_SUBST(GL_LIBS)
AC_SUBST(JPEG_LIBS)
AM_CONDITIONAL(HAVE_GL, test "x$GL_LIBS" != x)
AM_CONDITIONAL(HAVE_JPEG, test "x$JPEG_LIBS" != x)

AC_ARG_WITH(dc1394-plugin,
            [AS_HELP_STRING([--with-dc1394-plugin],
             [Compile dc1394 plugin if available])],
             [], [with_dc1394_plugin=yes])
if test x$with_dc1394_plugin = xyes; then
    PKG_PROG_PKG_CONFIG
    PKG_CHECK_MODULES(DC1394, [libdc1394-2 >= 2.0.0],,
                      [AC_MSG_WARN([libdc1394 not found, firewire camera support will be disabled])])
    if test "x$DC1394_LIBS" = x; then
        DC1394MSG="Disabled (libdc1394 not found)"
    else
        DC1394MSG="Enabled"
    fi
else
    DC1394MSG="Disabled"
fi
AM_CONDITIONAL(HAVE_DC1394, test x$with_dc1394_plugin = xyes -a "x$DC1394_LIBS" != x)

AM_CONDITIONAL([LINUX], [test x$arch = xlinux])

if test x$target_cpu = xi386 -o x$target_cpu = xi486 -o x$target_cpu = xi586 -o x$target_cpu = xi686 -o x$target_cpu = xx86_64; then
    AC_DEFINE(HAVE_INTEL, [1], [x86 instructions are available])
    have_intel=yes
fi
AM_CONDITIONAL(INTEL, [test x$have_intel = xyes])

dnl compile the V4L 1 plugin?
AC_ARG_WITH(v4l1-plugin,
            [AS_HELP_STRING([--with-v4l1-plugin],
             [Compile V4L1 plugin])],
            [], [with_v4l1_plugin=no])
if test x$with_v4l1_plugin = xyes -a x$arch != xlinux; then
    AC_MSG_ERROR([Video 4 Linux 1 plugin requires Linux])
fi
AM_CONDITIONAL([WITH_V4L1_PLUGIN], [test x$with_v4l1_plugin = xyes])

dnl compile the V4L 2 plugin?
AC_ARG_WITH(v4l2-plugin,
            [AS_HELP_STRING([--with-v4l2-plugin],
             [Compile V4L2 plugin if available])],
            [], [with_v4l2_plugin=yes])
AM_CONDITIONAL([WITH_V4L2_PLUGIN], 
               [test x$with_v4l2_plugin = xyes -a x$arch = xlinux])

dnl ---------------------------------------------------------------------------
dnl When making a release:
dnl  1. If the library source code has changed at all since the last release,
dnl     then increment REVISION.
dnl  2. If the interface has been changed since the last release, increment
dnl     CURRENT, and set REVISION to 0.
dnl  3. If the interface changes consist solely of additions, increment AGE.
dnl  4. If the interface has removed elements, set AGE to 0.
dnl ---------------------------------------------------------------------------
CAMUNITS_AGE=0
CAMUNITS_REVISION=2
CAMUNITS_CURRENT=6
AC_SUBST(CAMUNITS_AGE)
AC_SUBST(CAMUNITS_REVISION)
AC_SUBST(CAMUNITS_CURRENT)
CAMUNITS_VERSION_INFO=$CAMUNITS_CURRENT:$CAMUNITS_REVISION:$CAMUNITS_AGE
AC_SUBST(CAMUNITS_VERSION_INFO)

CAMUNITS_GTK_AGE=0
CAMUNITS_GTK_REVISION=2
CAMUNITS_GTK_CURRENT=6
AC_SUBST(CAMUNITS_GTK_AGE)
AC_SUBST(CAMUNITS_GTK_REVISION)
AC_SUBST(CAMUNITS_GTK_CURRENT)
CAMUNITS_GTK_VERSION_INFO=$CAMUNITS_GTK_CURRENT:$CAMUNITS_GTK_REVISION:$CAMUNITS_GTK_AGE
AC_SUBST(CAMUNITS_GTK_VERSION_INFO)

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

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

camunitsplugindir="$libdir/$PACKAGE"
AC_SUBST(camunitsplugindir)
camunitspluginincludedir="$includedir/$PACKAGE/plugins"
AC_SUBST(camunitspluginincludedir)

camunitspixmapdir="$datadir/pixmaps/$PACKAGE"
AC_SUBST(camunitspixmapdir)

camunitsdatadir="$datadir/$PACKAGE"
AC_SUBST(camunitsdatadir)

dnl ---------------------------------------------------------------------------
dnl additional plugin paths can be specified as a configure argument
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(plugin-extra-path,
            [AS_HELP_STRING([--with-plugin-extra-path=<path>],
             [Colon-separated plugin search path])])
if ! test -z "$with_plugin_extra_path" ; then
    extra_plugin_path="$with_plugin_extra_path"
else
    extra_plugin_path=""
fi
CAMUNITS_PLUGIN_PATH="$camunitsplugindir:$with_plugin_extra_path"
AC_SUBST(CAMUNITS_PLUGIN_PATH)

dnl ---------------------------------------------------------------------------
dnl C99 support, Warnings, and Large file support
dnl ---------------------------------------------------------------------------
if test "x$GCC" = "xyes"; then
    AM_CFLAGS="$AM_CFLAGS -std=gnu99 -Wall -Wmissing-declarations -Wmissing-prototypes -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
    LDFLAGS="$LDFLAGS -g -Wall"
    AC_SUBST(AM_CFLAGS)
    AC_SUBST(LDFLAGS)
fi

AC_OUTPUT([
  Makefile
  camunits.spec
  camunits/Makefile
  camunits-gtk/Makefile
  camview/Makefile
  camlog/Makefile
  m4/Makefile
  m4macros/Makefile
  camunits/camunits.pc
  camunits-gtk/camunits-gtk.pc
  camunits-gtk/pixmaps/Makefile
  po/Makefile.in
  plugins/Makefile
  plugins/dc1394/Makefile
  plugins/v4l/Makefile
  plugins/v4l2/Makefile
  plugins/convert/Makefile
  plugins/other/Makefile
  examples/Makefile
  examples/basic/Makefile
  examples/qt4/Makefile
  docs/Makefile
  docs/reference/Makefile
  docs/reference/libcamunits/Makefile
  docs/reference/libcamunits-gtk/Makefile
  docs/tutorial/Makefile
  docs/plugins/Makefile
  docs/plugins/build/Makefile
])

if test x$have_intel = xyes; then
    INTELMSG="Enabled"
else
    INTELMSG="Disabled"
fi

echo "

Configuration (camunits):

	Source code location:  ${srcdir}
	Compiler:              ${CC}
	x86 optimizations:     ${INTELMSG}
	DC1394 plugin:         ${DC1394MSG}"
if test x$with_v4l1_plugin = xyes; then
    echo "	Video4Linux 1 plugin:  Enabled"
fi
