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
	@echo "install: python setup.py install --root=debian/$(SRCNAME) --prefix=/usr"
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
		debian/$(SRCNAME)\
		debian/$(SRCNAME).*.log\
		debian/$(SRCNAME).*.debhelper\
		debian/$(SRCNAME).substvars\
		debian/files

install:
	python setup.py install --root=${DESTDIR} --prefix=/usr

# Scientific Linux source rpm
srpm: clean
	#
	# copy the local specfile to the necessary place
	#
	cp $(SRCNAME).spec $(RPMDIR)/SPECS/
	# 
	# create the tar.gz file 
	#
	tar -cf\
		$(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar\
		--transform=s/\./$(SRCNAME)-$(VERSION)/\
		--exclude-vcs\
		.
	gzip $(RPMDIR)/SOURCES/$(SRCNAME)-$(VERSION).tar
	#
	# build the source from the specfile
	# 
	rpmbuild -bs $(RPMDIR)/SPECS/$(SRCNAME).spec

rpm: srpm
	#
	# build all
	# 
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
