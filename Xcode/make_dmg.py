#!/usr/bin/python
import sys
import os
import subprocess as sp
from string import Template

if len(sys.argv) != 2:
    print "ERROR: invalid parameters."
    print "make_dmg.py <built products dir>"
    quit()

wc_rev = "XXXX"
try:
    svn_proc = sp.Popen(["svn", "info"], stdout=sp.PIPE)
    svn_info = svn_proc.communicate()[0]

    for line in svn_info.splitlines():
        if line.startswith("Revision:"):
            wc_rev = line.partition(" ")[2]
except:
    print "WARNING: No properly installed SVN client found, can't determine SVN working copy revision"

built_product_dir = sys.argv[1]
app = os.path.join(built_product_dir, "FreeOrion.app")
dmg_file = "FreeOrion-%s-MacOSX-10.6.dmg" % wc_rev
out_path = os.path.join(built_product_dir, dmg_file)
if os.path.exists(out_path):
    os.remove(out_path)

print "Creating %s in %s" % (dmg_file, built_product_dir)

sp.check_call('hdiutil create -volname FreeOrion -srcfolder "%s" -ov -format UDZO "%s"' % (app, out_path), shell=True)
