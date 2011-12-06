#
# Makefile for building gridftpwrapper "by hand" during
# development
#
#
#
#

include ./makefile_header
TARGET=gridftpwrapper.so
RPMDIR=$(HOME)/rpmbuild
VERSION=1.3.0
SRCNAME=python-gridftp
MY_INC=-I/usr/include/python2.6 
OS=$(shell uname)

ifeq ($(OS),Darwin)
	MY_CC_OPTS=-bundle -undefined suppress -flat_namespace
endif
ifeq ($(OS),Linux)
	MY_CC_OPTS=-shared
endif

MY_CFLAGS   = $(GLOBUS_INCLUDES) $(GLOBUS_CFLAGS) $(MY_INC) -fPIC 
MY_LDFLAGS  = $(GLOBUS_LDFLAGS)   $(GLOBUS_PKG_LDFLAGS) -lpython2.6 -fPIC
MY_LIBS     = $(GLOBUS_LIBS) $(GLOBUS_PKG_LIBS)

%.o: %.c
	$(CC) $(MY_CFLAGS) -c $< -o $@

all: $(TARGET)

%.so: %.o
	$(CC) $(MY_CC_OPTS) -o $@ $< $(MY_LDFLAGS) $(MY_LIBS)

clean:
	$(RM) -r *.so *.o *.pyc dist $(RPMDIR)/*/$(SRCNAME)*

srpm: clean
	cp $(SRCNAME).spec $(RPMDIR)/SPECS/
	tar -cf $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar --transform=s/\./$(SRCNAME)-$(VERSION)/ . --exclude-vcs
	gzip $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar
