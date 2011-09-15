import os
import sys
import platform
from distutils.core import setup, Extension

WORD_SIZE = None

try:
    processorString = platform.machine()
    platformString = platform.platform()

    if processorString == 'x86_64' or '64bit' in platformString:
        WORD_SIZE = 64
    else:
        WORD_SIZE = 32
except Exception, e:
    print >> sys.stderr, "Unable to determine if using x86_64 processor"
    sys.exit(-1)

linkFlags32 = [
"-lglobus_ftp_client",
"-lglobus_ftp_control",
"-lglobus_usage",
"-lglobus_io",
"-lglobus_xio",
"-lgssapi_error",
"-lglobus_gss_assist",
"-lglobus_gssapi_gsi",
"-lglobus_gsi_proxy_core",
"-lglobus_gsi_credential",
"-lglobus_gsi_callback",
"-lglobus_oldgaa",
"-lglobus_gsi_sysconfig",
"-lglobus_gsi_cert_utils",
"-lglobus_openssl",
"-lglobus_openssl_error",
"-lglobus_callout",
"-lglobus_proxy_ssl",
"-lglobus_common",
"-lssl",
"-lcrypto",
"-lltdl",
]

linkFlags64 = [
"-lglobus_ftp_client",
"-lglobus_ftp_control",
"-lglobus_usage",
"-lglobus_io",
"-lglobus_xio",
"-lglobus_gssapi_gsi",
"-lglobus_gsi_proxy_core",
"-lglobus_gsi_credential",
"-lglobus_gsi_callback",
"-lglobus_oldgaa",
"-lglobus_gsi_sysconfig",
"-lglobus_gsi_cert_utils",
"-lglobus_openssl",
"-lglobus_openssl_error",
"-lglobus_callout",
"-lglobus_proxy_ssl",
"-lglobus_common",
"-lssl",
"-lcrypto",
"-lltdl",
]

if WORD_SIZE == 64:
    linkFlags = linkFlags64
else:
    linkFlags = linkFlags32

e = Extension(
        "gridftpwrapper",
        ["gridftpwrapper.c"],
        include_dirs=[ "/usr/include/globus", "/usr/lib/globus/include" ],
        extra_compile_args=["-O0", "-Wno-strict-prototypes" ],
        extra_link_args=linkFlags
        )

extModList = [e]

setup(name="python-gridftp",
      version="1.2.0",
      description="Python GridFTP client bindings",
      author="Scott Koranda",
      author_email="ldr-lsc@gravity.phys.uwm.edu",
      url="http://www.lsc-group.phys.uwm.edu/LDR",
      py_modules=["gridftpClient", "gridftpwrapper"],
      ext_modules=extModList
      )
