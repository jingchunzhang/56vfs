OBJ=log.o  util_ip.o myconfig.o md5.o util_hex.o util_misc.o util_str.o myepoll.o protocol.o acl.o common.o pro_voss.o GeneralHashFunctions.o
LIBNAME=libvfs
CXX=g++

# Fallback to gcc when $CC is not in $PATH.
CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
OPTIMIZATION?=-O3
WARNINGS=-Wall -W -Wstrict-prototypes -Wwrite-strings
DEBUG?= -g -ggdb
REAL_CFLAGS=$(OPTIMIZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG)
REAL_LDFLAGS=$(LDFLAGS)
LIB+= -lpthread -ldl -lrt -rdynamic -lz -L./ -lmnm
MLIB=$(LIB) libmnm.a
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -Wl,-Bdynamic -fPIC
#INCLUDE = -I/home/jingchun.zhang/st/mysql/include/mysql
CFLAGS+= $(INCLUDE)
TARGET = testmd5

STLIBSUFFIX=a
STLIBNAME=$(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD=ar rcs $(STLIBNAME)
ALL: $(STLIBNAME) $(TARGET)

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

testmd5: testmd5.o
	$(CXX)	$(CFLAGS) -o $@ $^ -L./ -lvfs

%.o: %.cpp
	$(CC) -std=c99 -pedantic -c $(REAL_CFLAGS) $<

clean:
	rm -rf $(STLIBNAME) *.o testdb
