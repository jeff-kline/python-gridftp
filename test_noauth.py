from gridftpClient import *
from string import split, Template
from subprocess import PIPE, Popen
from threading import Event
from time import sleep

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
    dst = 'ftp://localhost:4444/etc/issue'
    for j in range(5):
        cli.cksm(dst, md5_cb, md5hash, op)
        md5_event.wait(1)
        md5_event.clear()
        print dst, md5hash

    exists_event = Event()
    exists_err = None
    exists_arg = bytearray(1)
    def exists_cb(arg, handle, error):
        arg[0] = bool(not error)
        exists_event.set()

    dst_list = [ 'ftp://localhost:4444/etc/issue',
                 'ftp://localhost:4444/etc/foo_issue']
    for f in dst_list:
        cli.exists(f, exists_cb, exists_arg, op)
        exists_event.wait()
        exists_event.clear()
        print f, bool(exists_arg[0])
except Exception as e:
    print e
    raise e
finally:
    op.destroy()
    hattr.destroy()
    cli.destroy()
    sleep(1)
