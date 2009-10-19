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

    if processorString == 'x86_64':
        WORD_SIZE = 64
    else:
        WORD_SIZE = 32
except Exception, e:
    print >> sys.stderr, "Unable to determine if using x86_64 processor"
    sys.exit(-1)

linkFlags32 = [
"-L%s/lib" % GLOBUS_LOCATION,
"-lglobus_ftp_client_gcc32dbgpthr",
"-lglobus_rls_client_gcc32dbgpthr",
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
"-L%s/lib" % GLOBUS_LOCATION,
"-lglobus_ftp_client_gcc64dbgpthr",
"-lglobus_rls_client_gcc64dbgpthr",
"-lglobus_gass_transfer_gcc64dbgpthr",
"-lglobus_ftp_control_gcc64dbgpthr",
"-lglobus_usage_gcc64dbgpthr",
"-lglobus_io_gcc64dbgpthr",
"-lglobus_xio_gcc64dbgpthr",
"-lgssapi_error_gcc64dbgpthr",
"-lglobus_gss_assist_gcc64dbgpthr",
"-lglobus_gssapi_gsi_gcc64dbgpthr",
"-lglobus_gsi_proxy_core_gcc64dbgpthr",
"-lglobus_gsi_credential_gcc64dbgpthr",
"-lglobus_gsi_callback_gcc64dbgpthr",
"-lglobus_oldgaa_gcc64dbgpthr",
"-lglobus_gsi_sysconfig_gcc64dbgpthr",
"-lglobus_gsi_cert_utils_gcc64dbgpthr",
"-lglobus_openssl_gcc64dbgpthr",
"-lglobus_openssl_error_gcc64dbgpthr",
"-lglobus_callout_gcc64dbgpthr",
"-lglobus_proxy_ssl_gcc64dbgpthr",
"-lglobus_common_gcc64dbgpthr",
"-lssl",
"-lcrypto",
"-lltdl_gcc64dbgpthr",
]

if WORD_SIZE == 64:
    linkFlags = linkFlags64
else:
    linkFlags = linkFlags32

e = Extension(
        "gridftpwrapper",
        ["gridftpwrapper.c"],
        include_dirs=[ "%s/include/gcc%ddbgpthr" % (GLOBUS_LOCATION, WORD_SIZE) ],
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
