#!/usr/bin/make -f
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

RPMDIR=$(HOME)/rpmbuild
VERSION=1.3.0
SRCNAME=python-gridftp

all: 
	@echo
	@echo
	@echo "Targets are:"
	@echo "    all, clean, install, srpm, rpm and deb"
	@echo
	@echo "all: print this message"
	@echo "clean: remove stuff"
	@echo "install: python setup.py install --root=debian/python-gridftp --prefix=/usr"
	@echo "srpm: clean and then build the source rpm"
	@echo "rpm: clean and then build the rpm and source rpm"
	@echo "deb: create an orig.tar.gz file and run debuild -uc -us"
	@echo
	@echo

clean:
	$(RM) -r\
		$(RPMDIR)/*/$(SRCNAME)*\
		$(RPMDIR)/*/*/$(SRCNAME)*\
		../$(SRCNAME)_*.deb\
		../$(SRCNAME)_*.dsc\
		../$(SRCNAME)_*.changes\
		../$(SRCNAME)_*.build\
		../$(SRCNAME)_*.debian.*\
		*.pyc\
		build\
		dist\
		MANIFEST\
		debian/python-gridftp\
		debian/python-gridftp.*.log\
		debian/python-gridftp.*.debhelper\
		debian/python-gridftp.substvars\
		debian/files

install:
	python setup.py install --root=debian/python-gridftp --prefix=/usr

# Scientific Linux source rpm
srpm: clean
	cp $(SRCNAME).spec $(RPMDIR)/SPECS/
	tar -cf\
		 $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar\
		 --transform=s/\./$(SRCNAME)-$(VERSION)/\
		 --exclude-vcs\
		 .
	gzip $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar
	rpmbuild -bs $(RPMDIR)/SPECS/$(SRCNAME).spec

rpm: srpm
	rpmbuild -ba $(RPMDIR)/SPECS/$(SRCNAME).spec

# debian commands to build deb files
deb: clean
	$(RM) ../$(SRCNAME)_$(VERSION).orig.tar.gz
	tar -cf\
		../$(SRCNAME)_$(VERSION).orig.tar\
		--transform=s/\./$(SRCNAME)_$(VERSION)/\
		--exclude-vcs\
		.
	gzip ../$(SRCNAME)_$(VERSION).orig.tar
	debuild -rfakeroot -D -uc -us
