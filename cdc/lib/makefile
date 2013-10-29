OBJ=bitops.o cdc_hash.o cdc_shm.o 
LIBNAME=libcdc
CXX=gcc

# Fallback to gcc when $CC is not in $PATH.
REAL_CFLAGS=$(OPTIMIZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG)
REAL_LDFLAGS=$(LDFLAGS)
INCLUDE= -I../inc -I../../lib
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -Wl,-Bdynamic -fPIC
LIB+=-L../lib/ -lcdc -L../../lib -lvfs 
CFLAGS+= $(INCLUDE)
TARGET = test_hash

STLIBSUFFIX=a
STLIBNAME=$(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD=ar rcs $(STLIBNAME)
ALL: $(STLIBNAME) $(TARGET)

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

test_hash: test_hash.o
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)

clean:
	rm -rf $(STLIBNAME) *.o $(TARGET)
