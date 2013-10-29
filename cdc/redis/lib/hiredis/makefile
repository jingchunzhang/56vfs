OBJ=net.o hiredis.o sds.o async.o
LIBNAME=../libhiredis

CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
OPTIMIZATION?=-O3
WARNINGS=-Wall -W -Wstrict-prototypes -Wwrite-strings
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -Wl,-Bdynamic -fPIC
DEBUG?= -g -ggdb
REAL_CFLAGS=$(OPTIMIZATION) $(CFLAGS) $(WARNINGS) $(DEBUG) $(ARCH)
REAL_LDFLAGS=$(LDFLAGS) $(ARCH)

STLIBSUFFIX=a
STLIBNAME=$(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD=ar rcs $(STLIBNAME)

ALL: $(STLIBNAME) $(TARGET)

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

clean:
	rm -rf $(STLIBNAME) *.o 

