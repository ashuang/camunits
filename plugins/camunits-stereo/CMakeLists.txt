cmake_minimum_required(VERSION 2.6.0)

# pull in the pods macros. See cmake/pods.cmake for documentation
set(POD_NAME camunits-stereo)
include(cmake/pods.cmake)

set(CAMUNITS_PLUGIN_DEST_PATH lib/camunits)

add_subdirectory(plugins)
