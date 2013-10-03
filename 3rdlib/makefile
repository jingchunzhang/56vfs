CC=gcc
CXX=g++
INC+= -I./ 
LIB+= -lpthread -ldl -lrt -rdynamic -L./ -lnmshm  -lstdc++
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT 
TARGET= app_c 
ALL:$(TARGET)

app_c_OBJ = app_c.o 

app_c:$(app_c_OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o
	rm -f $(TARGET) 	
