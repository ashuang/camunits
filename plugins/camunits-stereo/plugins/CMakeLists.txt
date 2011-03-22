add_definitions(-std=gnu99)

#split stereo plugin
add_library(stereo_split_stereo MODULE split-stereo.c)
pods_use_pkg_config_packages    (stereo_split_stereo camunits)
install(TARGETS stereo_split_stereo LIBRARY DESTINATION ${CAMUNITS_PLUGIN_DEST_PATH})
