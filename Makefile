#
# Makefile for building gridftpwrapper "by hand" during
# development
#
#
#
#


include ./makefile_header
TARGET=gridftpwrapper.so
MY_INC=-I/opt/local/Library/Frameworks/Python.framework/Versions/2.6/include/python2.6 

# For use on mac:
MY_CC_OPTS=-bundle -undefined suppress -flat_namespace
# For use on Linux:
# MY_CC_OPTS=-shared

MY_CFLAGS   = $(GLOBUS_INCLUDES) $(GLOBUS_CFLAGS)  $(MY_INC)  
MY_LDFLAGS  = $(GLOBUS_LDFLAGS)   $(GLOBUS_PKG_LDFLAGS) -fPIC
MY_LIBS     = $(GLOBUS_LIBS) $(GLOBUS_PKG_LIBS)
CC=/opt/local/bin/gcc-mp-4.4

%.o: %.c
	$(CC) $(MY_CFLAGS) -c $< -o $@

all: $(TARGET)

%.so: %.o
	$(CC) $(MY_CC_OPTS) -o $@ $< $(MY_LDFLAGS) $(MY_LIBS)

clean:
	$(RM) *.so *.o *.pyc 



