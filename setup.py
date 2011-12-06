import os
import sys
import platform
from distutils.core import setup, Extension

GLOBUS_LOCATION = None
WORD_SIZE = None

try:
    GLOBUS_LOCATION = os.environ["GLOBUS_LOCATION"]
except KeyError:
    print >> sys.stderr, "GLOBUS_LOCATION must be set before building gridftpClient"
    sys.exit(-1)

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

if platform.dist()[0] == 'debian':
        my_include_dirs=[ "/usr/include/globus", "/usr/lib/globus/include" ]
else:
        my_include_dirs=[ "/usr/include/globus", "/usr/lib64/globus/include" ]
    
linkFlags32 = [
"-L%s/lib" % GLOBUS_LOCATION,
"-lglobus_ftp_client_gcc32dbgpthr",
"-lglobus_ftp_control_gcc32dbgpthr",
"-lglobus_usage_gcc32dbgpthr",
"-lglobus_io_gcc32dbgpthr",
"-lglobus_xio_gcc32dbgpthr",
"-lgssapi_error_gcc32dbgpthr",
"-lglobus_gss_assist_gcc32dbgpthr",
"-lglobus_gssapi_gsi_gcc32dbgpthr",
"-lglobus_gsi_proxy_core_gcc32dbgpthr",
"-lglobus_gsi_credential_gcc32dbgpthr",
"-lglobus_gsi_callback_gcc32dbgpthr",
"-lglobus_oldgaa_gcc32dbgpthr",
"-lglobus_gsi_sysconfig_gcc32dbgpthr",
"-lglobus_gsi_cert_utils_gcc32dbgpthr",
"-lglobus_openssl_gcc32dbgpthr",
"-lglobus_openssl_error_gcc32dbgpthr",
"-lglobus_callout_gcc32dbgpthr",
"-lglobus_proxy_ssl_gcc32dbgpthr",
"-lglobus_common_gcc32dbgpthr",
"-lssl",
"-lcrypto",
"-lltdl_gcc32dbgpthr",
]

linkFlags64 = [
"-L%s/lib64" % GLOBUS_LOCATION,
"-lglobus_ftp_client",
"-lglobus_gass_transfer",
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
        include_dirs=my_include_dirs,
        extra_compile_args=["-O0", "-Wno-strict-prototypes" ],
        extra_link_args=linkFlags
        )

extModList = [e]

setup(name="python-gridftp",
      version="1.1.0",
      description="Python GridFTP client bindings",
      author="Scott Koranda",
      author_email="ldr-lsc@gravity.phys.uwm.edu",
      url="http://www.lsc-group.phys.uwm.edu/LDR",
      py_modules=["gridftpClient", "gridftpwrapper"],
      ext_modules=extModList
      )
