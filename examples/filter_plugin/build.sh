#!/bin/bash

CFLAGS="-fPIC `pkg-config --cflags camunits glib-2.0` -Wall"
kernel=`uname -s`
if [ "$kernel" == "Darwin" ]; then
    ext=".dylib"
    ldsh="-dynamic"
else
    ext=".so"
    ldsh="-shared"
fi

if [ ! -d build ] ; then
    mkdir build
fi

echo gcc -c filter_plugin.c $CFLAGS
gcc -c filter_plugin.c $CFLAGS || exit 1

echo gcc $ldsh -o build/filter_plugin$ext filter_plugin.o
gcc $ldsh -o build/filter_plugin$ext filter_plugin.o || exit 1
