#
# Makefile for building gridftpwrapper "by hand" during
# development
#
#
#
#

include ./makefile_header

MY_CFLAGS   = $(GLOBUS_INCLUDES) $(GLOBUS_CFLAGS) -I/usr/include/python2.4 -fPIC
MY_LDFLAGS  = $(GLOBUS_LDFLAGS) $(GLOBUS_PKG_LDFLAGS) -fPIC
MY_LIBS     = $(GLOBUS_LIBS) $(GLOBUS_PKG_LIBS)

%.o: %.c
	$(CC) $(MY_CFLAGS) -c $<

all: gridftpwrapper

gridftpwrapper: gridftpwrapper.o 
	$(CC) -shared -o gridftpwrapper.so gridftpwrapper.o $(MY_LDFLAGS) $(MY_LIBS)

clean:
	$(RM) gridftpwrapper.so *.o *.pyc 



