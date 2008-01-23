# Configure paths for opencv
# Albert Huang
# Large parts shamelessly stolen from GIMP

dnl AM_PATH_OPENCV([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for opencv, and define OPENCV_CFLAGS and OPENCV_LIBS
dnl
AC_DEFUN([AM_PATH_OPENCV],
[dnl 
dnl Get the cflags and libraries from pkg-config
dnl

AC_ARG_ENABLE(opencvtest, [  --disable-opencvtest      do not try to compile and run a test opencv program],, enable_opencvtest=yes)

  pkg_name=opencv
  pkg_config_args="$pkg_name"

  no_opencv=""

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

  if test x$PKG_CONFIG != xno ; then
    if pkg-config --atleast-pkgconfig-version 0.7 ; then
      :
    else
      echo *** pkg-config too old; version 0.7 or better required.
      no_opencv=yes
      PKG_CONFIG=no
    fi
  else
    no_opencv=yes
  fi

  min_opencv_version=ifelse([$1], ,1.0.0,$1)
  AC_MSG_CHECKING(for opencv - version >= $min_opencv_version)

  if test x$PKG_CONFIG != xno ; then
    ## don't try to run the test against uninstalled libtool libs
    if $PKG_CONFIG --uninstalled $pkg_config_args; then
	  echo "Will use uninstalled version of opencv found in PKG_CONFIG_PATH"
	  enable_opencvtest=no
    fi

    if $PKG_CONFIG --atleast-version $min_opencv_version $pkg_config_args; then
	  :
    else
	  no_opencv=yes
    fi
  fi

  if test x"$no_opencv" = x ; then
    OPENCV_CFLAGS=`$PKG_CONFIG $pkg_config_args --cflags`
    OPENCV_LIBS=`$PKG_CONFIG $pkg_config_args --libs`

    opencv_pkg_major_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    opencv_pkg_minor_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    opencv_pkg_micro_version=`$PKG_CONFIG --modversion $pkg_name | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_opencvtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $OPENCV_CFLAGS"
      LIBS="$OPENCV_LIBS $LIBS"

dnl
dnl Now check if the installed opencv is sufficiently new. (Also sanity
dnl checks the results of pkg-config to some extent
dnl
      rm -f conf.opencvtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <opencv/cv.h>

CvPoint cvp = { 0, 0 };

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.opencvtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = strdup("$min_opencv_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_opencv_version");
     exit(1);
   }

    if (($opencv_pkg_major_version > major) ||
        (($opencv_pkg_major_version == major) && ($opencv_pkg_minor_version > minor)) ||
        (($opencv_pkg_major_version == major) && ($opencv_pkg_minor_version == minor) && ($opencv_pkg_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'pkg-config --modversion %s' returned %d.%d.%d, but the minimum version\n", "$pkg_name", $opencv_pkg_major_version, $opencv_pkg_minor_version, $opencv_pkg_micro_version);
      printf("*** of opencv required is %d.%d.%d. If pkg-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If pkg-config was wrong, set the environment variable PKG_CONFIG_PATH\n");
      printf("*** to point to the correct the correct configuration files\n");
      return 1;
    }
}

],, no_opencv=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_opencv" = x ; then
     AC_MSG_RESULT(yes (version $opencv_pkg_major_version.$opencv_pkg_minor_version.$opencv_pkg_micro_version))
     ifelse([$2], , :, [$2])     
  else
     if test "$PKG_CONFIG" = "no" ; then
       echo "*** A new enough version of pkg-config was not found."
       echo "*** See http://www.freedesktop.org/software/pkgconfig/"
     else
       if test -f conf.opencvtest ; then
        :
       else
          echo "*** Could not run opencv test program, checking why..."
          CFLAGS="$CFLAGS $OPENCV_CFLAGS"
          LIBS="$LIBS $OPENCV_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <opencv/cv.h>

CvPoint cvp = { 0, 0 };


],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding opencv or finding the wrong"
          echo "*** version of opencv. If it is not finding opencv, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occurred. This usually means opencv is incorrectly installed."])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     OPENCV_CFLAGS=""
     OPENCV_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(OPENCV_CFLAGS)
  AC_SUBST(OPENCV_LIBS)
  AC_SUBST(OPENCV_DATA_DIR)
  AC_SUBST(OPENCV_PLUGIN_DIR)
  rm -f conf.opencvtest
])
