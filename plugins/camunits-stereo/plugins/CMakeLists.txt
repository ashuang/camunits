find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0 gobject-2.0)
pkg_check_modules(CAMUNITS REQUIRED camunits)

include_directories(${GLIB2_INCLUDE_DIRS} ${CAMUNITS_INCLUDE_DIRS})
add_definitions(-std=gnu99)

add_library(stereo_split_stereo MODULE split-stereo.c)

install(TARGETS stereo_split_stereo LIBRARY DESTINATION ${CAMUNITS_PLUGIN_DEST_PATH})
