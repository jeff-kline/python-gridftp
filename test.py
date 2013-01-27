from gridftpClient import *
from random import randrange, random, seed, shuffle
from shutil import copy, rmtree
from socket import getfqdn
from string import split, Template
from subprocess import PIPE, Popen
from threading import Event
from time import sleep
from tempfile import mkdtemp
from os import chmod, environ, makedirs, symlink, unlink
from os.path import basename, getsize, isfile, join

TRUSTED_CERT_DIR = join(environ['HOME'], '.globus', 'certificates')

'''
the openssl commands is based on information found at
http://www.peereboom.us/assl/assl/html/openssl.html
'''
def ca_keycert(subject, basedir):
    '''
    create a ca certificate and key pair that lasts 10 years, write to
    basedir/ca/
    '''
    keydir = join(basedir, 'ca','private')
    certdir = join(basedir, 'ca')
    makedirs(keydir, 0700)
    cmd_dict = {'key': join(keydir, 'key.pem'),
                'cert': join(certdir, 'cert.pem'),
                'subject': subject}
    cmd_tmpl = Template("""openssl req -x509 -days 3650 -newkey rsa:1024 
                          -keyout ${key} -out ${cert} -nodes 
                          -subj ${subject}""")
    cmd = cmd_tmpl.substitute(cmd_dict)
    p=Popen(split(cmd, maxsplit=0xd), stdout=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate()
    if p.returncode:
        raise RuntimeError(stderr)

    return cmd_dict

def _keycert(subject, basedir, ca_cert, ca_key, _type):
    '''
    build request, sign it with the CA, write the files to disk under
    basedir/_type/
    
    return dict
    '''
    # build request
    keydir = join(basedir, _type, 'private')
    certdir = join(basedir, _type)
    makedirs(keydir, 0700)
    cmd_dict = {'key': join(keydir, 'key.pem'),
                'certreq': join(certdir, 'certreq.pem'),
                'cert': join(certdir, 'cert.pem'),
                'subject': subject,
                'ca_cert': ca_cert,
                'ca_key': ca_key}
    cmd_tmpl = Template("""openssl genrsa -out ${key} 1024""")
    cmd = cmd_tmpl.substitute(cmd_dict)
    p = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate()
    if p.returncode:
        raise RuntimeError(stderr)
    else:
        chmod(cmd_dict['key'], 0600)

    # create a key
    cmd_tmpl = Template("""openssl req -new -key ${key} -out ${certreq} 
                           -subj ${subject}""")
    cmd = cmd_tmpl.substitute(cmd_dict)
    p = Popen(split(cmd, maxsplit=8), stdout=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate()
    if p.returncode:
        raise RuntimeError(stderr)

    # sign the request
    cmd_tmpl = Template('''openssl x509 -req -in ${certreq} 
                           -CAcreateserial -CA ${ca_cert} -CAkey ${ca_key} 
                           -out ${cert}''')
    cmd = cmd_tmpl.substitute(cmd_dict)
    p=Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
    stdout,stderr = p.communicate()
    if p.returncode:
        raise RuntimeError(stderr)

    return cmd_dict

def server_keycert(subject, basedir, ca_cert, ca_key):
    return _keycert(subject, basedir, ca_cert, ca_key, 'server')

def client_keycert(subject, basedir, ca_cert, ca_key):
    return _keycert(subject, basedir, ca_cert, ca_key, 'client')

def cert_dir(ca_cert):
    '''
    make this certificate trusted, it lives under global variable
    TRUSTED_CERT_DIR. This is global because gridftp-server does not
    permit arbitrary customization.
    '''
    try:
        makedirs(TRUSTED_CERT_DIR)
    except OSError:
        # if the dir exists, ignore error
        pass

    # get the cert subject hash
    cmd_tmpl = Template("""openssl x509 -hash -noout -in ${ca_cert}""")
    cmd_dict = {'ca_cert': ca_cert}
    cmd = cmd_tmpl.substitute(cmd_dict)
    p = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate()
    if p.returncode:
        raise RuntimeError(stderr)
    cert_hash = stdout.strip()

    # remove any lingering files
    try:
        unlink(join(TRUSTED_CERT_DIR,'%s.0' % cert_hash ))
    except:
        pass
    try:
        unlink(join(TRUSTED_CERT_DIR,'%s.signing_policy' % cert_hash ))
    except:
        pass
    symlink(ca_cert, join(TRUSTED_CERT_DIR,'%s.0' % cert_hash ))

    # get subject
    cmd_tmpl = Template("""openssl x509 -subject -noout -in ${ca_cert}""")
    cmd = cmd_tmpl.substitute(cmd_dict)
    p = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate()
    if p.returncode:
        raise RuntimeError(stderr)

    # remove 'subject= ' from the output
    subject = stdout.strip()[len('subject= '):]

    # write signing policy
    with open(join(TRUSTED_CERT_DIR,'%s.signing_policy' % cert_hash ), 'w') as f:
        f.write("access_id_CA    X509    '%s'\n"  % subject )
        f.write("pos_rights      globus  CA:sign\n")
        
    return cert_hash


class RunningGridFTPServer(object):
    """
    create ca, server and client certs, start a gridftp server, create
    client and guarantee transfer occurs before returning

        # all file writing occurs here
        self.basedir 

        # credentials (dictionaries)
        self.ca_cred 
        self.server_cred
        self.client_cred

        # the openssl subject hash of the ca
        self.hashname

        # environmental vairable GLOBUS_LOCATION for the gridftp
        # server
        self.globusdir


    files that get written are subsequently deleted when object is
    garbage-collected (after all references to object go out of
    scope).
    """
    def __init__(self, try_connect=True, ntries=25, *args, **kwargs):
        basedir = mkdtemp()
        base_subj = '/C=US/ST=WI/O=Python Test CA Limited'

        ca_subj = '/'.join([base_subj, 'CN=Python CA Common Name'])
        server_subj = '/'.join([base_subj, 'CN=%s' % getfqdn()])
        client_subj = '/'.join([base_subj, 'CN=Client Foo Bar'])
        self.basedir = basedir

        # build the credentials for CA, server and client
        self.ca_cred = ca_keycert(ca_subj, basedir)
        self.server_cred = server_keycert(
            server_subj, basedir, self.ca_cred['cert'], self.ca_cred['key'])
        self.client_cred = client_keycert(
            client_subj, basedir, self.ca_cred['cert'], self.ca_cred['key'])
        self.hashname = cert_dir(self.ca_cred['cert'])

        # environmental variables used by server
        self.globusdir = basedir
        sysdir = join(self.globusdir, 'etc')
        makedirs(sysdir)
        hostcert = join(sysdir, 'hostcert.pem')
        hostkey = join(sysdir, 'hostkey.pem')
        copy(self.server_cred['cert'], hostcert)
        copy(self.server_cred['key'], hostkey)
        chmod(hostkey, 0600)

        # write a gridmap 
        self.gridmap = join(self.globusdir, 'etc', 'gridmap')
        with open(self.gridmap, 'w') as f:
            f.write('"%s" %s\n' % (self.client_cred['subject'], environ['USER']))

        # build server command
        server_env = {
            'GLOBUS_LOCATION': self.globusdir,
            'GRIDMAP': self.gridmap,
            'X509_USER_CERT': self.server_cred['cert'],
            'X509_USER_KEY':  self.server_cred['key']
            }
        self.port = randrange(10000, 50000)
        server_cmd = '/usr/sbin/globus-gridftp-server -p %d -debug' % self.port
        self.server_p = Popen(
            server_cmd.split(), env=server_env, stdout=PIPE, stderr=PIPE)
        # server start takes time; keep trying to contact server until
        # success
        
        client_env = {'X509_USER_CERT': self.client_cred['cert'],
                      'X509_USER_KEY':  self.client_cred['key'],}
        client_cmd = """globus-url-copy -vb 
                        gsiftp://%s:%d/etc/issue /dev/null""" % (getfqdn(), self.port)

        if try_connect:
            for n in range(ntries):
                p = Popen(client_cmd.split(), env=client_env, stdout=PIPE, stderr=PIPE)
                o = p.communicate()
                if p.returncode:
                    sleep(1)
                else:
                    break
            if n >= (ntries-1):
                msg = 'Unable to contact server. STDOUT: %s, STDERR: %s' % o
                raise RuntimeError(msg)

    def __del__(self):
        try:
            self.server_p.terminate()
            rmtree(self.basedir)
            unlink(join(TRUSTED_CERT_DIR,'%s.0' % self.hashname ))
            unlink(join(TRUSTED_CERT_DIR,'%s.signing_policy' % self.hashname ))
        except:
            pass

gridftp_server = RunningGridFTPServer(try_connect=True)
env = {'X509_USER_CERT': gridftp_server.client_cred['cert'],
       'X509_USER_KEY': gridftp_server.client_cred['key']}
environ.update(env)
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
    dst = 'gsiftp://%s:%d/etc/issue' % (getfqdn(), gridftp_server.port)
    for j in range(5):
        cli.cksm(dst, md5_cb, md5hash, op)
        md5_event.wait()
        md5_event.clear()
        print dst, md5hash

    exists_event = Event()
    exists_err = None
    exists_arg = bytearray(1)
    def exists_cb(arg, handle, error):
        arg[0] = bool(not error)
        exists_event.set()

    dst_list = [ 'gsiftp://%s:%d/etc/foo_issue' % (getfqdn(), gridftp_server.port),
                 'gsiftp://%s:%d/etc/issue' % (getfqdn(), gridftp_server.port)]
    for f in dst_list:
        cli.exists(f, exists_cb, exists_arg, op)
        exists_event.wait()
        exists_event.clear()
        print f, bool(exists_arg[0])
        
finally:
    op.destroy()
    hattr.destroy()
    cli.destroy()
    del gridftp_server
    sleep(1)
