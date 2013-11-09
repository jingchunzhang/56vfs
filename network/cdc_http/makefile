CC=gcc
CXX=g++
INC+= -I../ -I../../lib  -I../../3rdlib -I../../cdc/inc -I../../cdc/redis/inc -I/usr/include/mysql
LIB+=-lpthread  -L../../lib -lvfs   -L../../3rdlib -L../../cdc/lib/ -lcdc -L../../cdc/redis/lib/ -lhiredis -lcdc_redis_api
LIB+= -L/usr/lib64/mysql/ -lmysqlclient
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -DVFS_SERVER=1 -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC -shared #-z defs
TARGET=cdc_http.so
ALL:$(TARGET)

OBJ=cdc_http.o 

$(TARGET):$(OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o *.so
	rm -f $(TARGET) 	
