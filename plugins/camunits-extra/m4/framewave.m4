# Configure paths for Framewave
# Albert Huang

dnl AM_PATH_FRAMEWAVE([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for Framewave, and define FRAMEWAVE_CFLAGS and FRAMEWAVE_LIBS
dnl
AC_DEFUN([AM_PATH_FRAMEWAVE],
[dnl 
dnl setup cflags and libraries
dnl

AC_ARG_ENABLE(framewavetest, [  --disable-framewavetest      do not try to run a test framewave program],, enable_framewavetest=yes)

no_framewave=""

FRAMEWAVE_CFLAGS=""
FRAMEWAVE_IMAGE_LIBS="-lfwBase -lfwImage"
FRAMEWAVE_JPEG_LIBS="-lfwBase -lfwJPEG"
FRAMEWAVE_SIGNAL_LIBS="-lfwBase -lfwVideo"
FRAMEWAVE_VIDEO_LIBS="-lfwBase -lfwSignal"
FRAMEWAVE_LIBS="-lfwBase -lfwImage -lfwJPEG -lfwVideo -lfwSignal"

dnl 
dnl compile a test program
dnl
if test "x$enable_framewavetest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $FRAMEWAVE_CFLAGS"
    LIBS="$FRAMEWAVE_LIBS $LIBS"

    rm -f conf.framewavetest
    AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fwBase.h>

int main ()
{
  fwStaticInit();

  system ("touch conf.framewavetest");
}

],, no_framewave=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])

    CFLAGS="$ac_save_CFLAGS"
    LIBS="$ac_save_LIBS"
fi

if test "x$no_framewave" = x ; then
   AC_MSG_RESULT(yes)
   AC_DEFINE([HAVE_FRAMEWAVE],[1],[Framewave available])
   ifelse([$2], , :, [$2])     
else
   if test -f conf.framewavetest ; then
    :
   else
       echo "*** Could not run framewave test program, checking why..."
       CFLAGS="$CFLAGS $FRAMEWAVE_CFLAGS"
       LIBS="$LIBS $FRAMEWAVE_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
#include <fwBase.h>

FwiSize sz;

],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding framewave or finding the wrong"
          echo "*** version of framewave. If it is not finding framewave, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occurred. This usually means framewave is incorrectly installed."])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
    fi
    FRAMEWAVE_CFLAGS=""
    FRAMEWAVE_LIBS=""
    ifelse([$3], , :, [$3])
fi
AC_SUBST(FRAMEWAVE_CFLAGS)
AC_SUBST(FRAMEWAVE_LIBS)
AC_SUBST(FRAMEWAVE_IMAGE_LIBS)
AC_SUBST(FRAMEWAVE_JPEG_LIBS)
AC_SUBST(FRAMEWAVE_SIGNAL_LIBS)
AC_SUBST(FRAMEWAVE_VIDEO_LIBS)
rm -f conf.framewavetest
])
