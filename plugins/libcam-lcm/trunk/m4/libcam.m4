# Configure paths for libcam
# Albert Huang
# Large parts shamelessly stolen from GIMP

dnl AM_PATH_LIBCAM([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for libcam, and define LIBCAM_CFLAGS and LIBCAM_LIBS
dnl
AC_DEFUN([AM_PATH_LIBCAM],
[dnl 
dnl Get the cflags and libraries from pkg-config
dnl

AC_ARG_ENABLE(libcamtest, [  --disable-libcamtest      do not try to compile and run a test libcam program],, enable_libcamtest=yes)

  pkg_name=libcam
  pkg_config_args="$pkg_name glib-2.0"

  no_libcam=""

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

  if test x$PKG_CONFIG != xno ; then
    if pkg-config --atleast-pkgconfig-version 0.7 ; then
      :
    else
      echo *** pkg-config too old; version 0.7 or better required.
      no_libcam=yes
      PKG_CONFIG=no
    fi
  else
    no_libcam=yes
  fi

  min_libcam_version=ifelse([$1], ,0.0.4,$1)
  AC_MSG_CHECKING(for libcam - version >= $min_libcam_version)

  if test x$PKG_CONFIG != xno ; then
    ## don't try to run the test against uninstalled libtool libs
    if $PKG_CONFIG --uninstalled $pkg_config_args; then
	  echo "Will use uninstalled version of libcam found in PKG_CONFIG_PATH"
	  enable_libcamtest=no
    fi

    if $PKG_CONFIG --atleast-version $min_libcam_version $pkg_config_args; then
	  :
    else
	  no_libcam=yes
    fi
  fi

  if test x"$no_libcam" = x ; then
    LIBCAM_CFLAGS=`$PKG_CONFIG $pkg_config_args --cflags`
    LIBCAM_LIBS=`$PKG_CONFIG $pkg_config_args --libs`
    LIBCAM_DATA_DIR=`$PKG_CONFIG $pkg_name --variable=libcamdatadir`
    LIBCAM_PLUGIN_DIR=`$PKG_CONFIG $pkg_name --variable=libcamlibdir`

    libcam_pkg_major_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    libcam_pkg_minor_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    libcam_pkg_micro_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_libcamtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $LIBCAM_CFLAGS"
      LIBS="$LIBCAM_LIBS $LIBS"

dnl
dnl Now check if the installed libcam is sufficiently new. (Also sanity
dnl checks the results of pkg-config to some extent
dnl
      rm -f conf.libcamtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcam/cam.h>

typedef struct _CamTestDriver CamTestDriver;
typedef struct _CamTestDriverClass CamTestDriverClass;

struct _CamTestDriver {
    CamUnitDriver parent;
} testdriver;

struct _CamTestDriverClass {
    CamUnitDriverClass parent_class;
};

typedef struct _CamTest CamTest;
typedef struct _CamTestClass CamTestClass;

struct _CamTest {
    CamUnit parent;
} testunit;

struct _CamTestClass {
    CamUnitClass parent_class;
};

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.libcamtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = strdup("$min_libcam_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_libcam_version");
     exit(1);
   }

    if (($libcam_pkg_major_version > major) ||
        (($libcam_pkg_major_version == major) && ($libcam_pkg_minor_version > minor)) ||
        (($libcam_pkg_major_version == major) && ($libcam_pkg_minor_version == minor) && ($libcam_pkg_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'pkg-config --modversion %s' returned %d.%d.%d, but the minimum version\n", "$pkg_name", $libcam_pkg_major_version, $libcam_pkg_minor_version, $libcam_pkg_micro_version);
      printf("*** of libcam required is %d.%d.%d. If pkg-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If pkg-config was wrong, set the environment variable PKG_CONFIG_PATH\n");
      printf("*** to point to the correct the correct configuration files\n");
      return 1;
    }
}

],, no_libcam=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_libcam" = x ; then
     AC_MSG_RESULT(yes (version $libcam_pkg_major_version.$libcam_pkg_minor_version.$libcam_pkg_micro_version))
     ifelse([$2], , :, [$2])     
  else
     if test "$PKG_CONFIG" = "no" ; then
       echo "*** A new enough version of pkg-config was not found."
       echo "*** See http://www.freedesktop.org/software/pkgconfig/"
     else
       if test -f conf.libcamtest ; then
        :
       else
          echo "*** Could not run libcam test program, checking why..."
          CFLAGS="$CFLAGS $LIBCAM_CFLAGS"
          LIBS="$LIBS $LIBCAM_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <libcam/cam.h>

struct _CamTestDriver {
    CamUnitDriver parent;
} testdriver;

struct _CamTestDriverClass {
    CamUnitDriverClass parent_class;
};

typedef struct _CamTest CamTest;
typedef struct _CamTestClass CamTestClass;

struct _CamTest {
    CamUnit parent;
} testunit;

struct _CamTestClass {
    CamUnitClass parent_class;
};

],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding libcam or finding the wrong"
          echo "*** version of libcam. If it is not finding libcam, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occurred. This usually means libcam is incorrectly installed."])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     LIBCAM_CFLAGS=""
     LIBCAM_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(LIBCAM_CFLAGS)
  AC_SUBST(LIBCAM_LIBS)
  AC_SUBST(LIBCAM_DATA_DIR)
  AC_SUBST(LIBCAM_PLUGIN_DIR)
  rm -f conf.libcamtest
])
