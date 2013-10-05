CC=gcc
CXX=g++
INC+= -I../ -I../../lib  -I../../3rdlib 
LIB+=-lpthread  -L../../lib -lvfs   -L../../3rdlib -lnmshm 
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC -shared #-z defs
TARGET=vfs_voss_master_sig.so
ALL:$(TARGET)

OBJ=vfs_voss.o

$(TARGET):$(OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o *.so
	rm -f $(TARGET) 	
