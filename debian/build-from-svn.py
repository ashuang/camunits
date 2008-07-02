#!/usr/bin/python

import sys
import os
import os.path
import commands
import shutil
from xml.dom import minidom

def do_or_die(cmd):
    print "Running [%s]" % cmd
    if 0 != os.system(cmd):
        raise RuntimeError ("damn.")

if len (sys.argv) < 2:
    print ("usage: %s <version>" % (os.path.basename(sys.argv[0])))
    sys.exit(1)

release_version = sys.argv[1]
package_name = "libcam"
svn_url = "https://libcam.googlecode.com/svn/trunk"

do_or_die ("svn update")
status, xml_str = commands.getstatusoutput("svn info --xml %s" % svn_url)

info = minidom.parseString (xml_str)
entry_node = info.documentElement.getElementsByTagName ("entry")[0]
svn_revision = int (entry_node.getAttribute("revision"))

# setup some useful variables
export_dir = "%s-%s.svn%d" % (package_name, release_version, svn_revision)
orig_tarball = "../tarballs/%s_%s.svn%d.orig.tar.gz" % \
        (package_name, release_version, svn_revision) 

# start the process...
print "Cleaning out build-area"
do_or_die ("rm -rf build-area/*")

print ("Reverting trunk")
do_or_die ("svn revert --recursive trunk")
os.chdir("trunk")

if os.path.exists (export_dir):
    print "Clearing out %s" % export_dir
    do_or_die ("rm -rf %s" % export_dir)

print ("Checking out a copy of source tree")
do_or_die ("svn export -r %d %s %s" % (svn_revision, svn_url, export_dir))

print ("Building orig.tar.gz")
os.chdir (export_dir)
do_or_die ("gtkdocize --copy")
do_or_die ("autoreconf -i")
do_or_die ("./configure --enable-gtk-doc")
os.system ("make")
do_or_die ("make")
do_or_die ("make")
do_or_die ("make distcheck")
os.chdir ("..")
if os.path.exists (orig_tarball):
    os.unlink (orig_tarball)
shutil.copyfile ("%s/%s-%s.tar.gz" % (export_dir, package_name, 
    release_version), orig_tarball)

print ("Updating debian/changelog")
do_or_die ("dch --newversion %s.svn%d-1 \"New subversion build\"" % \
        (release_version, svn_revision))

print "Building..."
do_or_die ("svn-buildpackage -us -uc -rfakeroot --svn-ignore")

os.system ("svn revert debian/changelog")
