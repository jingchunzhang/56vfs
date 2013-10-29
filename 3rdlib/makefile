OBJ=TException.o TShareMem.o TNmShm.o c_api.o 
LIBNAME=libnmshm
CXX=g++

# Fallback to gcc when $CC is not in $PATH.
CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
OPTIMIZATION?=-O3
WARNINGS=-Wall -W -Wwrite-strings
DEBUG?= -g -ggdb
REAL_CFLAGS=$(OPTIMIZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG)
REAL_LDFLAGS=$(LDFLAGS)
LIB+= -lpthread -ldl -lrt -rdynamic -lz 
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -Wl,-Bdynamic -lssl -fPIC 
CFLAGS+= $(INCLUDE)

STLIBSUFFIX=a
STLIBNAME=$(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD=ar rcs $(STLIBNAME)
ALL: $(STLIBNAME) 

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

%.o: %.cpp
	$(CXX) -c $(REAL_CFLAGS) $<

clean:
	rm -rf $(STLIBNAME) *.o 
