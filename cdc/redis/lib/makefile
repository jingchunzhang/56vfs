OBJ=cdc_redis_api.o 
LIBNAME=libcdc_redis_api
CXX=gcc

# Fallback to gcc when $CC is not in $PATH.
REAL_CFLAGS=$(OPTIMIZATION) -fPIC $(CFLAGS) $(WARNINGS) $(DEBUG)
REAL_LDFLAGS=$(LDFLAGS)
INCLUDE= -I../inc -I../../lib -I../../../lib
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -Wl,-Bdynamic -fPIC
LIB+=-L../../lib/ -lcdc 
CFLAGS+= $(INCLUDE)

STLIBSUFFIX=a
STLIBNAME=$(LIBNAME).$(STLIBSUFFIX)
STLIB_MAKE_CMD=ar rcs $(STLIBNAME)
ALL: $(STLIBNAME) $(TARGET)

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

clean:
	rm -rf $(STLIBNAME) *.o 
