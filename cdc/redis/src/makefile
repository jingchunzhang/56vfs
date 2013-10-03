CC=gcc
CXX=g++
INC+= -I./ -I../inc -I../../../lib -I../../../3rdlib 
LIB+= -lpthread -ldl -lrt -rdynamic 
LIB+=-L../../lib/ -lcdc -L../../../lib/ -lvfs -L../../../3rdlib -lnmshm -L../lib/ -lhiredis -lcdc_redis_api

CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGE_FILES=1 -D_XOPEN_SOURCE=600
TARGET= cdc_redis cdc_redis_test cdc_redis_del
ALL:$(TARGET)

cdc_redis_OBJ = cdc_redis.o 
cdc_redis_test_OBJ = cdc_redis_test.o 
cdc_redis_del_OBJ = cdc_redis_del.o 

cdc_redis:$(cdc_redis_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_redis_test:$(cdc_redis_test_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_redis_del:$(cdc_redis_del_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o
	rm -f $(TARGET) 	
