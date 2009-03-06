#!/usr/bin/python

import sys
import os
import os.path
import commands
import shutil
import re
from xml.dom import minidom

def do_or_die(cmd):
    print "Running [%s]" % cmd
    if 0 != os.system(cmd):
        raise RuntimeError("damn.")

if len(sys.argv) > 1:
    print("usage: %s" % (os.path.basename(sys.argv[0])))
    sys.exit(1)

dont_use_cache = False

package_name = "camunits-extra"
svn_url = "https://camunits.googlecode.com/svn/plugins/camunits-extra"

do_or_die("svn update")
status, xml_str = commands.getstatusoutput("svn info --xml %s" % svn_url)

info = minidom.parseString(xml_str)
entry_node = info.documentElement.getElementsByTagName("entry")[0]
svn_revision = int(entry_node.getAttribute("revision"))

# start the process...
print "Cleaning out build-area"
do_or_die("rm -rf build-area/*")

print("Reverting changelog")
do_or_die("svn revert trunk/debian/changelog")
os.chdir("trunk")

export_dir = "%s.svn%d" % (package_name, svn_revision)
if os.path.exists(export_dir) and dont_use_cache:
    print "Clearing out %s" % export_dir
    do_or_die("rm -rf %s" % export_dir)

if not os.path.exists(export_dir):
    print("Checking out a copy of source tree")
    do_or_die("svn export -r %d %s %s" % (svn_revision, svn_url, export_dir))

os.chdir(export_dir)

print("Extracting version number")
regex = re.compile("AM_INIT_AUTOMAKE\s*\(\s*%s\s*,\s*([^\s)]+)\s*\)" % \
        package_name)
release_version = None
for line in file("configure.in"):
    match = regex.match(line)
    if match:
        release_version = match.group(1)
        break
if not release_version:
    print "unable to determine release version!"
    sys.exit(1)
print "release version: %s" % release_version

print("Building orig.tar.gz")
orig_tarball = "../../tarballs/%s_%s.svn%d.orig.tar.gz" % \
        (package_name, release_version, svn_revision) 
if os.path.exists(orig_tarball) and dont_use_cache:
    os.unlink(orig_tarball)

if not os.path.exists(orig_tarball):
    print os.listdir("../")
    do_or_die("gtkdocize --copy")
    do_or_die("autoreconf -i")
    do_or_die("./configure --enable-gtk-doc")
    do_or_die("make")
    do_or_die("make distcheck")
    os.chdir ("..")
    orig_tarball = "../tarballs/%s_%s.svn%d.orig.tar.gz" % \
            (package_name, release_version, svn_revision) 
    if os.path.exists (orig_tarball):
        os.unlink (orig_tarball)
    shutil.copyfile ("%s/%s-%s.tar.gz" % (export_dir, package_name, 
        release_version), orig_tarball)
else:
    os.chdir ("..")

print("Updating debian/changelog")
do_or_die("dch --newversion %s.svn%d-1 \"New subversion build\"" % \
        (release_version, svn_revision))

print "Building..."
do_or_die("svn-buildpackage -us -uc -rfakeroot --svn-ignore")

os.system("svn revert debian/changelog")
