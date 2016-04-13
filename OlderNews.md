**Mar 11, 2009** - Release 0.2.0

This release features a wide-ranging API overhaul, and breaks the old API in
many ways.

  * Most classes have had all of their fields moved away from the public structs
> and into private structs.  This is so that future changes do not result in any
> API or ABI incompatibilities, and is in keeping with GTK+3 plans.  Currently,
> the only classes that still have publicly exposed fields in their structs are:
> CamUnitFormat, CamFrameBuffer
  * CamUnitManager has been converted to follow a singleton pattern
  * CamUnitControl has been modified so that enumerated controls no longer have
> integer values implicitly assigned by ordering.  integer values associated with
> enumerated controls are now explicitly assigned, using arrays of
> CamUnitControlEnumValue structs.
  * All core units have been converted to plugins.  All functions specific to
> core units have been removed from the API.
  * when adding an output format, max\_data\_size is no longer specified.
  * convert.to\_rgb now uses IPP and Framewave JPEG decompression plugins from
> camunits-extra if they are available

**Camunits Core**

  * new functions:
    * `cam_unit_manager_get_and_ref`
    * `cam_unit_add_output_format`
    * `cam_unit_control_get_enum_values`
    * `cam_unit_control_get_name`
    * `cam_unit_control_get_id`
    * `cam_unit_control_get_control_type`
    * `cam_unit_control_get_control_type_str`
  * new structs:
    * `CamUnitControlEnumValue`
  * removed functions:
    * `cam_unit_manager_new`
    * `cam_unit_chain_new_with_manager`
    * `cam_unit_chain_get_manager`
    * `cam_unit_add_output_format_full` (changed, renamed: `cam_unit_add_output_format`)
    * `cam_unit_set_id`
    * `cam_unit_set_name`
    * `cam_unit_control_get_type_str` (renamed: `cam_unit_control_get_control_type_str`)
    * all functions related to core units have been removed.
  * modified functions:  (parameter list has changed)
    * `cam_unit_add_control_enum`
    * `cam_unit_control_new_enum`
    * `cam_unit_control_modify_enum`
    * `cam_unit_set_preferred_format`   (new  parameter added: `const char *name`)

**Camunits GTK**

  * removed functions:
    * `cam_unit_manager_set_manager`
  * modified functions:  (parameter list has changed)
    * `cam_unit_manager_widget`

**Feb 15, 2009** - Release 0.1.2

Camunits changes:
  * add plugin example
  * add jpeg decompress to grayscale
  * add --plugin-path command line option to camview and camlog
  * modify convert:fast\_debayer to handle data that's not already 16-byte aligned
  * convert CamInputV4L to plugin.
  * disable V4L plugin by default.  V4L2 is still enabled by default.

This also marks the first release of Camunits-extra, which has the following plugins:
  * Intel IPP
    * JPEG Decompress
    * JPEG Compress
    * Resize
    * Pyramid Downsample Gaussian
    * Gaussian Fixed Filter
  * AMD Framewave
    * JPEG Decompress
    * JPEG Compress
    * Resize
  * OpenCV
    * Canny Edge demo
    * GoodFeaturesToTrack demo
    * Undistort
  * Utilities
    * image throttling
    * snapshot
    * convert to grayscale
    * dump images as files
  * LibCVD
    * FAST features demo
  * KLT (Birchfield)
    * KLT feature tracker demo
  * Lightweight Communications and Marshalling (LCM)
    * image publish
    * image receive
    * synchronize Camunits log file input with LCM

**Dec 17, 2008** - Release 0.1.1

  * camview:
    * add FPS display, other minor UI tweaks
    * change "-f" option to "-c"
  * libcamunits:
    * CamInputLog: add looping controls
    * add CAM\_PIXEL\_FORMAT\_LE\_GRAY16, opengl support
    * CamLoggerUnit: add "auto-suffix-enable" control, option to disable auto suffix on filename
    * rename IS\_ALIGNEDxx macros to CAM\_IS\_ALIGNEDxx
    * add better type safety to CamUnitControl
    * CamUnitManager bugfixes
  * libcamunits-gtk:
    * bugfixes in UnitControlWidget
  * camlog
    * add "-c" option to load chain from XML description file
    * add "-o" option to specify output file
    * add "-f" option to force overwrite of existing files

**Nov 11, 2008** - New project name, release 0.1.0

The [libcam](http://libcam.googlecode.com) project is renamed to Camunits.  Reasons for the renaming are:
  1. Avoid conflicting name with BSD libcam, a completely unrelated project.
  1. Reduce confusion between the core library (libcamunit) and the whole project (libcamunit, libcamunit-gtk, camview, camlog, plugins, etc.)
  1. Choose a new name that more closely reflects the core design.

To mark the new project name, version 0.1.0 is also being released.  Notable changes:

  * better support for 16-bit pixel formats
  * input\_dc1394: include format7 mode in output format string
  * unit\_manager: on failure to open plugin dir, warn in dbg instead of stderr
  * unit\_chain:   store output format name in saved XML.
  * unit\_control\_widget: slightly smarter about format changing
  * minor bugfixes all around.

This release breaks the old API.  To port libcam code to Camunits code,
replace all instances of "libcam" and "LIBCAM" with "camunits" and "CAMUNITS",
respectively.  This applies to source code, include paths, library names,
linker flags, environment variables, pkg-config names, etc.

The following two perl commands accomplish the search/replace:
```
   $ perl -pi -e "s/LIBCAM/CAMUNITS/g;" <files>
   $ perl -pi -e "s/libcam/camunits/g;" <files>
```

**July 16, 2008** - Released libcam 0.0.9

The changes from version 0.0.8 are:
  * unit manager widget UI enhancements
    * sort unit descriptions by unit name
    * toggle expansion of packages when package is double-clicked/activated
  * enable spinbutton for float unit control widget
  * warn when units produce framebuffers with zero bytesused and timestamp fields
  * fix some docs
  * fix accidental API change in 0.0.8

**June 13, 2008** - Released libcam 0.0.8

The changes from version 0.0.7 are:
  * add --no-gui option to camview
  * input\_example: fix timestamps
  * camlog: fix thrashing for inexact log seeking (e.g. timestamps)
  * update tutorial
  * add qt4 examples
  * input\_v4l: was not populating timestamp, bytesused.  fixed.

**April 17, 2008** - Released libcam 0.0.7

**April 9, 2008** - Released libcam 0.0.6

**February 14, 2008** - Released libcam 0.0.5

**January 8, 2008** - Released libcam 0.0.4

The changes from version 0.0.3 are:
  * add CAM\_PIXEL\_FORMAT\_ANY
  * implement advance mode control in input\_log
  * implement speed control in input\_log
  * log - change file offsets in CamLogFrameInfo from uint64\_t to in64\_t
  * CamUnitChainGLWidget - add "gl-draw-finished" signal

**December 20, 2007** - Released libcam 0.0.3

The changes from version 0.0.2 are:

  * Fixed the installation directory of libcam-gtk headers and renamed cam\_gtk.h to cam-gtk.h.

**December 18, 2007** - Released libcam 0.0.2

The changes from version 0.0.1 are:

  * Now builds under Mac OS X (tested on Leopard)
  * Tutorial and examples improvements
  * Documentation improvements
  * Update DC1394 plugin to use libdc1394-2.0.0-rc9
  * Allow DC1394 modes other than Format 7
  * Added additional colorspace conversions
  * Allow bayer demosaic code to work on non-SSE3 CPUs (still requires SSE2)