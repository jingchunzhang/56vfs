CC=gcc
CXX=g++
INC+= -I./ -I../inc -I../../lib -I../../3rdlib -I/usr/include/mysql
LIB+= -lpthread -ldl -lrt -rdynamic 
LIB+=-L../lib/ -lcdc -L../../lib -lvfs -L../../3rdlib -lnmshm -L/usr/lib64/mysql/ -lmysqlclient

CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGE_FILES=1 -D_XOPEN_SOURCE=600
TARGET= cdc cdctool cdc_db cdc_db_tool cdc_redis_tool cdc_db_merge cdc_migrate cdc_migrate_normal cdc_db_del
ALL:$(TARGET)

cdc_OBJ = cdc.o 
cdctool_OBJ = cdctool.o 
cdc_db_OBJ = cdc_db.o cdc_db_oper.o
cdc_db_tool_OBJ = cdc_db_tool.o 
cdc_db_del_OBJ = cdc_db_del.o 
cdc_db_merge_OBJ = cdc_db_merge.o 
cdc_redis_tool_OBJ = cdc_redis_tool.o 
cdc_migrate_OBJ = cdc_migrate.o 
cdc_migrate_normal_OBJ = cdc_migrate_normal.o 

cdc:$(cdc_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdctool:$(cdctool_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_db:$(cdc_db_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_db_merge:$(cdc_db_merge_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_db_tool:$(cdc_db_tool_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_db_del:$(cdc_db_del_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_redis_tool:$(cdc_redis_tool_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_migrate:$(cdc_migrate_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
cdc_migrate_normal:$(cdc_migrate_normal_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o
	rm -f $(TARGET) 	
