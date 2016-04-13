Camunits consists of an image acquisition library, algorithms, and tools designed specifically for machine vision researchers. It was originally designed and used by Team MIT for real-time vision-based lane estimation in the 2007 DARPA Urban Challenge, and is now being developed and released as an open source project.

Camunits is written in C and uses GLib.

## Features ##

Camunits is divided into several pieces.

| **libcamunits** | Provides the core functionality of Camunits, which includes classes for acquiring images from USB and Firewire cameras, logging and replaying image streams, performing colorspace conversions and image compression/decompression, and connecting image processing elements together |
|:----------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **libcamunits-gtk** | Provides GTK widgets for libcamunit objects for easily adding GUI elements.                                                                                                                                                                                                           |
| **camview**     | Debugging tool and general purpose Camunits application                                                                                                                                                                                                                               |
| **camunits extra** | Provides image processing and acquisition plugins that may contain nonstandard dependencies, or algorithms for special interest purposes.                                                                                                                                             |

## Requirements ##

Camunits currently works on the GNU/Linux and OS X operating systems.  libcamunits has a few main dependencies

  * GLib
  * OpenGL
  * libjpeg

IEEE 1394 Digital Camera support is provided as a plugin, and requires libdc1394 version 2.x:
  * http://sourceforge.net/project/showfiles.php?group_id=8157&package_id=154936

## Documentation ##

  * [DownloadingAndInstalling](DownloadingAndInstalling.md)
  * [Tutorial](http://camunits.googlecode.com/svn/www/tutorial/index.html)
  * API Reference (partially complete)
    * [libcamunits](http://camunits.googlecode.com/svn/www/reference/libcamunits/index.html)
    * [libcamunits-gtk](http://camunits.googlecode.com/svn/www/reference/libcamunits-gtk/index.html)
  * [Core plugins](http://camunits.googlecode.com/svn/www/reference/plugins-core/index.html)

  * [Frequently Asked Questions (FAQ)](FAQ.md)
  * To ask questions, submit patches, and contact the developers, use the camunits mailing list, accessible at http://groups.google.com/group/camunits

## News ##

**Jan 28, 2010** - Release 0.3.0

This is a maintenance release.

  * libcamunits:
    * fix camunits float control
    * fix return value bug in cam-unit\_control\_try\_set##type()
  * core plugins:
    * input.dc1394: set framerate for non format 7 modes
    * input.v4l2: enable V4L2 extended controls
  * camunits-extra:
    * bugfixes to snapshot unit
  * build system:
    * add --without-dc1394-plugin configure option

**Apr 20, 2009** - Release 0.2.1


This is a maintenance release.

  * libcamunits:
    * remove usage of libc math functions
    * unit manager unset singleton on finalize
  * core plugins:
    * input.dc1394: re-init dc1394 unit when packet size changes.
    * convert.fast\_debayer: fix regression (auto-setting of tiling control)
    * convert.to\_rgb8: add UYVY as acceptable input
  * camunits-extra:
    * add ipp.filter-sobel
  * build system:
    * set AM\_CFLAGS instead of CFLAGS

[OlderNews](OlderNews.md)
## Screenshots ##

Click on each image for a higher resolution version

### camview ###

Camview, running with a Logitech Quickcam 5000 Pro

![![](http://camunits.googlecode.com/svn/www/images/camview-qc5000-screenshot-small.png)](http://camunits.googlecode.com/svn/www/images/camview-qc5000-screenshot.png)

### lane\_finder ###

lane\_finder, a real-time lane-detection application developed with libcamunits and libcamunits-gtk for the 2007 DARPA Urban Challenge.

![![](http://camunits.googlecode.com/svn/www/images/camview-lanefinder-screenshot-small.png)](http://camunits.googlecode.com/svn/www/images/camview-lanefinder-screenshot.png)