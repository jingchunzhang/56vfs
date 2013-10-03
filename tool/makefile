CC=gcc
CXX=g++
INC+= -I../lib -I/home/jingchun.zhang/st/mysql/include/mysql
LIB+= -lpthread -ldl -lrt -rdynamic 
LIB+=-L../lib -lvfs -L/home/jingchun.zhang/st/mysql/lib/mysql/ -lmysqlclient

CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGE_FILES=1 -D_XOPEN_SOURCE=600
TARGET= fcs_cmp
ALL:$(TARGET)

fcs_cmp_OBJ = fcs_cmp.o 

fcs_cmp:$(fcs_cmp_OBJ)
	$(CXX)	$(CFLAGS) -o $@ $^ $(LIB)
#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o
	rm -f $(TARGET) 	
