# to build python-gridftp by hand in this dir, do
#   make all
#
# to clean the dir do
#   make clean
#
# to build python-gridftp rpms for Scientific Linux, do
#   make rpm
#
# to build python-gridftp srpms for Scientific Linux, do
#   make srpm
#
TARGET=gridftpwrapper.so
RPMDIR=$(HOME)/rpmbuild
VERSION=1.3.0
SRCNAME=python-gridftp
MY_INC=-I/usr/include/python2.6 
OS=$(shell uname)
MFH=$(wildcard makefile_header)

ifeq ($(OS),Darwin)
	MY_CC_OPTS=-bundle -undefined suppress -flat_namespace
endif
ifeq ($(OS),Linux)
	MY_CC_OPTS=-shared
endif

all: $(TARGET) 

makefile_header:
	globus-makefile-header --flavor=gcc64pthr > $@

clean:
	$(RM) -r *.so\
		 *.o\
		 *.pyc\
		 $(RPMDIR)/*/$(SRCNAME)*\
		 $(RPMDIR)/*/*/$(SRCNAME)*\
		 build\
		 dist\
		 MANIFEST\

sinclude makefile_header
MY_CFLAGS   = $(GLOBUS_INCLUDES) $(GLOBUS_CFLAGS) $(MY_INC) -fPIC 
MY_LDFLAGS  = $(GLOBUS_LDFLAGS)   $(GLOBUS_PKG_LDFLAGS) -lpython2.6 -fPIC
MY_LIBS     = $(GLOBUS_LIBS) $(GLOBUS_PKG_LIBS)

%.o: %.c
	$(CC) $(MY_CFLAGS) -c $< -o $@

%.so: %.o
	$(CC) $(MY_CC_OPTS) -o $@ $< $(MY_LDFLAGS) $(MY_LIBS)


# Scientific Linux source rpm
srpm: clean
	cp $(SRCNAME).spec $(RPMDIR)/SPECS/
	tar -cf\
		 $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar\
		 --transform=s/\./$(SRCNAME)-$(VERSION)/\
		 --exclude-vcs\
		 .
	gzip $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar

# Scientific Linux rpm build
rpm: srpm
	GLOBUS_LOCATION=$(GLOBUS_LOCATION) rpmbuild -ba $(RPMDIR)/SPECS/$(SRCNAME).spec
