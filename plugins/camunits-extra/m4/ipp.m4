# Configure paths for Intel IPP
# Albert Huang

dnl AM_PATH_IPP([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for IPP, and define LIBIPP_CFLAGS and LIBIPP_LIBS
dnl
AC_DEFUN([AM_PATH_IPP],
[dnl 
dnl Get the cflags and libraries from pkg-config
dnl

AC_REQUIRE([AC_CANONICAL_SYSTEM])

min_ipp_version=ifelse([$1], ,5.1,$1)
AC_MSG_CHECKING(for IPP - version >= $min_ipp_version)

AC_ARG_ENABLE(ipptest, [  --disable-ipptest      do not try to compile and run a test IPP program],, enable_ipptest=yes)

case "$target_cpu" in
    i*86*) 
        IPP_JPEG_STATIC_LIBS="-lippjemerged -lippjmerged -lippiemerged -lippimerged -lippsemerged -lippsmerged -lippcore"
        IPP_JPEG_LIBS="-lippj -lippi -lipps -lippcore"
        IPP_ARCH="ia32"
        IPP_LIBS="-lguide -lippcore -lippi -lippcc -lippcv"
    ;;
    ia64*) 
        IPP_JPEG_STATIC_LIBS="-lippji7 -lippii7 -lippsi7 -lippcore64"
        IPP_JPEG_LIBS="-lippj64 -lippi64 -lipps64 -lippcore64"
        IPP_ARCH="ia64"
        IPP_LIBS="-lguide -lippcore64 -lippi64 -lippj64 -lippcc64 -lippcv64 -lpthread -lipps64"
    ;;
	x86_64*) 
        IPP_JPEG_STATIC_LIBS="-lippjemergedem64t -lippjmergedem64t -lippiemergedem64t -lippimergedem64t -lippsemergedem64t -lippsmergedem64t -lippcoreem64t"
        IPP_JPEG_LIBS="-lippjem64t -lippiem64t -lippsem64t -lippcoreem64t"
        IPP_ARCH="em64t"
        IPP_LIBS="-lguide -lippcoreem64t -lippiem64t -lippjem64t -lippccem64t -lippcvem64t -lpthread -lippsem64t"
    ;;
	*) 
        IPP_JPEG_STATIC_LIBS="-lippjemerged -lippjmerged -lippiemerged -lippimerged -lippsemerged -lippsmerged -lippcore"
        IPP_JPEG_LIBS="-lippj -lippi -lipps -lippcore"
        IPP_LIBS="-lguide -lippcore -lippi -lippcc -lippcv"
    ;;
esac

base_candidates=
case "$target_os" in
	linux*) 
        IPP_CDEFINES="-Dlinux -Dlinux64"
        base_candidates="/opt/intel/ipp /usr/local/intel/ipp"
        include_subdir="include"
        lib_subdir="sharedlib"
        ;;
	darwin*)
        IPP_CDEFINES=
        base_candidates="/Library/Frameworks/Intel_IPP.framework"
        include_subdir="Headers"
        lib_subdir="Libraries"
		;;
	*) 
        IPP_CDEFINES=
        ;;
esac

IPP_INCLUDEDIR=
for basedir in $base_candidates ; do
    if test -d $basedir ; then
        for d in $basedir/* ; do
            if test -d $d/${IPP_ARCH}/${include_subdir} ; then
                IPP_INCLUDEDIR=$d/${IPP_ARCH}/${include_subdir} 
                IPP_LIBDIR=$d/${IPP_ARCH}/${lib_subdir}
                break 2
            fi
        done
    fi
done

if test x${IPP_INCLUDEDIR} = x ; then
    IPP_JPEG_STATIC_LIBS=
    IPP_JPEG_LIBS=
    IPP_ARCH=
    IPP_LIBS=
    IPP_CFLAGS=
    IPP_CDEFINES=

    ifelse([$3], , :, [$3])
else
    IPP_CFLAGS="-I${IPP_INCLUDEDIR} $IPP_CDEFINES"

    case "$target_os" in
        linux*) 
            IPP_LDFLAGS="-L${IPP_LIBDIR} -Wl,-R${IPP_LIBDIR}"
        ;;
        darwin*)
            IPP_LDFLAGS="-L${IPP_LIBDIR} "
        ;;
    esac

    AC_SUBST(IPP_CFLAGS)
    AC_SUBST(IPP_LDFLAGS)
    AC_SUBST(IPP_JPEG_STATIC_LIBS)
    AC_SUBST(IPP_JPEG_LIBS)
    AC_SUBST(IPP_LIBS)
    AC_DEFINE([HAVE_IPP],[1],[IPP available])
    ifelse([$2], , :, [$2])
fi

])
