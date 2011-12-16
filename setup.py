import os
import sys
import platform
from distutils.core import setup, Extension

GLOBUS_LOCATION = "/usr"

try:
    GLOBUS_LOCATION = os.environ["GLOBUS_LOCATION"]
except KeyError:
    print >> sys.stderr, "GLOBUS_LOCATION is not set; using default %s" %(GLOBUS_LOCATION)

if platform.dist()[0] == 'debian':
        my_include_dirs=[ "/usr/include/globus", "/usr/lib/globus/include" ]
else:
        my_include_dirs=[ "/usr/include/globus", "/usr/lib64/globus/include" ]
    
linkFlags = [
"-L%s/lib64" % GLOBUS_LOCATION,
"-lglobus_ftp_client",
"-lglobus_io",
"-lglobus_common",
]

if platform.dist()[0] == 'debian':
        my_include_dirs=[ "/usr/include/globus", "/usr/lib/globus/include" ]
else:
        my_include_dirs=[ "/usr/include/globus", "/usr/lib64/globus/include" ]
    

e = Extension(
        "gridftpwrapper",
        ["gridftpwrapper.c"],
        include_dirs=my_include_dirs,
        extra_compile_args=["-O0", "-Wno-strict-prototypes" ],
        extra_link_args=linkFlags
        )

extModList = [e]

setup(name="python-gridftp",
      version="1.3.0",
      description="Python GridFTP client bindings",
      author="Jeff Kline",
      author_email="kline@gravity.phys.uwm.edu",
      url="http://www.lsc-group.phys.uwm.edu/LDR",
      py_modules=["gridftpClient", "gridftpwrapper"],
      ext_modules=extModList
      )
