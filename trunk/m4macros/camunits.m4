# Configure paths for libcamunits
# Albert Huang
# Large parts shamelessly stolen from GIMP

dnl AM_PATH_CAMUNITS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for camunits, and define CAMUNITS_CFLAGS and CAMUNITS_LIBS
dnl
AC_DEFUN([AM_PATH_CAMUNITS],
[dnl 
dnl Get the cflags and libraries from pkg-config
dnl

AC_ARG_ENABLE(camunitstest, [  --disable-camunitstest      do not try to compile and run a test camunits program],, enable_camunitstest=yes)

  pkg_name=camunits
  pkg_config_args="$pkg_name glib-2.0"

  no_camunits=""

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

  if test x$PKG_CONFIG != xno ; then
    if pkg-config --atleast-pkgconfig-version 0.7 ; then
      :
    else
      echo *** pkg-config too old; version 0.7 or better required.
      no_camunits=yes
      PKG_CONFIG=no
    fi
  else
    no_camunits=yes
  fi

  min_camunits_version=ifelse([$1], ,0.0.4,$1)
  AC_MSG_CHECKING(for camunits - version >= $min_camunits_version)

  if test x$PKG_CONFIG != xno ; then
    ## don't try to run the test against uninstalled libtool libs
    if $PKG_CONFIG --uninstalled $pkg_config_args; then
	  echo "Will use uninstalled version of camunits found in PKG_CONFIG_PATH"
	  enable_camunitstest=no
    fi

    if $PKG_CONFIG --atleast-version $min_camunits_version $pkg_config_args; then
	  :
    else
	  no_camunits=yes
    fi
  fi

  if test x"$no_camunits" = x ; then
    CAMUNITS_CFLAGS=`$PKG_CONFIG $pkg_config_args --cflags`
    CAMUNITS_LIBS=`$PKG_CONFIG $pkg_config_args --libs`
    CAMUNITS_DATA_DIR=`$PKG_CONFIG $pkg_name --variable=camunitsdatadir`
    CAMUNITS_PLUGIN_DIR=`$PKG_CONFIG $pkg_name --variable=camunitslibdir`

    camunits_pkg_major_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    camunits_pkg_minor_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    camunits_pkg_micro_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_camunitstest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $CAMUNITS_CFLAGS"
      LIBS="$CAMUNITS_LIBS $LIBS"

dnl
dnl Now check if the installed camunits is sufficiently new. (Also sanity
dnl checks the results of pkg-config to some extent
dnl
      rm -f conf.camunitstest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <camunits/cam.h>

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

  system ("touch conf.camunitstest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = strdup("$min_camunits_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_camunits_version");
     exit(1);
   }

    if (($camunits_pkg_major_version > major) ||
        (($camunits_pkg_major_version == major) && ($camunits_pkg_minor_version > minor)) ||
        (($camunits_pkg_major_version == major) && ($camunits_pkg_minor_version == minor) && ($camunits_pkg_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'pkg-config --modversion %s' returned %d.%d.%d, but the minimum version\n", "$pkg_name", $camunits_pkg_major_version, $camunits_pkg_minor_version, $camunits_pkg_micro_version);
      printf("*** of camunits required is %d.%d.%d. If pkg-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If pkg-config was wrong, set the environment variable PKG_CONFIG_PATH\n");
      printf("*** to point to the correct the correct configuration files\n");
      return 1;
    }
}

],, no_camunits=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_camunits" = x ; then
     AC_MSG_RESULT(yes (version $camunits_pkg_major_version.$camunits_pkg_minor_version.$camunits_pkg_micro_version))
     ifelse([$2], , :, [$2])     
  else
     if test "$PKG_CONFIG" = "no" ; then
       echo "*** A new enough version of pkg-config was not found."
       echo "*** See http://www.freedesktop.org/software/pkgconfig/"
     else
       if test -f conf.camunitstest ; then
        :
       else
          echo "*** Could not run camunits test program, checking why..."
          CFLAGS="$CFLAGS $CAMUNITS_CFLAGS"
          LIBS="$LIBS $CAMUNITS_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <camunits/cam.h>

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
          echo "*** that the run-time linker is not finding camunits or finding the wrong"
          echo "*** version of camunits. If it is not finding camunits, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occurred. This usually means camunits is incorrectly installed."])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     CAMUNITS_CFLAGS=""
     CAMUNITS_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(CAMUNITS_CFLAGS)
  AC_SUBST(CAMUNITS_LIBS)
  AC_SUBST(CAMUNITS_DATA_DIR)
  AC_SUBST(CAMUNITS_PLUGIN_DIR)
  rm -f conf.camunitstest
])
