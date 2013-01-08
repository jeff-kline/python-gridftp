from gridftpClient import *
from os import environ
from socket import getfqdn
from threading import Event
from time import sleep
import sys

SERVER=getfqdn()
PORT=int(sys.argv[1])
for key in environ:
    print key, environ[key]
try:
    op = OperationAttr()
    op.set_mode_extended_block()
    hattr = HandleAttr()
    # use the same control channel for all actions
    hattr.set_cache_all()
    cli = FTPClient(hattr)

    md5_event = Event()
    md5_err = None
    def md5_cb(cksm, arg, handle, error):
        if error is not None:
            md5_err = error
        arg[:] = cksm
        md5_event.set()

    md5hash = bytearray()
    dst = 'gsiftp://%s:%d/etc/issue' % (SERVER, PORT)
    for j in range(5):
        cli.cksm(dst, md5_cb, md5hash, op)
        md5_event.wait()
        md5_event.clear()
        print dst, md5hash
finally:
    op.destroy()
    hattr.destroy()
    cli.destroy()
    sleep(1)
