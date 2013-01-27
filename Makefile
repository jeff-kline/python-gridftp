# Use this Makefile as a template for 
#   debian "native" packages
#   SL packages with corresponding version
# 
# Customize it by modifying 2 variables: PKGNAME and VERSION
# 
# 'make chk_ver' will run checks on debian package version and 
# SL package version.  It will error out if the versions do not
# agree.
#
# 
# 
# (C) Jeffery Kline 2011
# GNU/GPL v3
PKGNAME=python-gridftp
VERSION=1.3.2

# RPMDIR is used by Scientific Linux
#        causes no harm on Debian.
RPMDIR=$(HOME)/rpmbuild

help: 
	@echo
	@echo
	@echo "Targets are:"
	@echo "    help, clean, install, srpm, rpm and deb"
	@echo
	@echo "help: print this message"
	@echo "clean: remove stuff"
	@echo "install: run the command" 
	@echo "    python setup.py install --root=DESTDIR --prefix=/usr"
	@echo "srpm: clean and then build the source rpm"
	@echo "rpm: clean and then build the rpm and source rpm"
	@echo "deb: create an orig.tar.gz file and run debuild -uc -us"
	@echo
	@echo

clean:
	$(RM) -r\
		$(RPMDIR)/*/$(PKGNAME)*\
		$(RPMDIR)/*/*/$(PKGNAME)*\
		../$(PKGNAME)_*\
		*.pyc\
		build\
		dist\
		MANIFEST\
		debian/$(PKGNAME)\
		debian/$(PKGNAME).*.log\
		debian/$(PKGNAME).*.debhelper\
		debian/$(PKGNAME).substvars\
		debian/files

# debhelper notes:
#   since this Makefile exists, debhelper will
#     - ignore setup.py files. setup.py must be called manually.
#     - run make clean, make, make install.
install: chk_ver
	python setup.py install --root=${DESTDIR} --prefix=/usr

# Scientific Linux source rpm
srpm: chk_ver clean
	# copy the local specfile to the necessary place
	install $(PKGNAME).spec $(RPMDIR)/SPECS/
	# create the tar.gz file 
	tar -zcf\
		$(RPMDIR)/SOURCES/$(PKGNAME)-$(VERSION).tar.gz\
		--transform=s/\./$(PKGNAME)-$(VERSION)/\
		--exclude-vcs\
		.
	# build the source from the specfile
	rpmbuild -bs $(RPMDIR)/SPECS/$(PKGNAME).spec

rpm: srpm
	# build all
	rpmbuild -ba $(RPMDIR)/SPECS/$(PKGNAME).spec

chk_ver:
	@echo "Expect version number to match $(VERSION)"
	@echo "  VERSION in Makefile, most recent version" 
	@echo "  in debian/changelog must match"
	@echo "  in $(PKGNAME).spec must match"
	@echo 
	@echo "  Checking Debian Version"
	@head -n1 debian/changelog | grep '($(VERSION))' > /dev/null
	@echo "  Debian OK."
	@echo 
	@echo "  Checking Scientific Linux Version"
	@if [ -f $(PKGNAME).spec ]; then\
	  grep -E '%define version +$(VERSION)' $(PKGNAME).spec > /dev/null;\
	else\
	  echo "** No file $(PKGNAME).spec. Silently continuing.";\
	fi
	@echo "  SL OK."

# debian commands to build deb files
deb: chk_ver
	debuild -rfakeroot -D -uc -us
