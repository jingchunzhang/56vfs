NS=vfs_master
CC=gcc
CXX=g++
INC+= 
LIB+= -lpthread -ldl -lrt -rdynamic -L../lib -lvfs -lstdc++ -L../3rdlib -lnmshm 
TARGET=$(NS)
ALL:$(TARGET)

ifneq ($(NS), vfs_master)
SUBOBJS=vfs_so_r.o
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_NS_=\"$(NS)\" -DVFS_SERVER=1 -I../lib -I../3rdlib
else
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_NS_=\"$(NS)\" -DVFS_SERVER=2 -I../lib -I../3rdlib
SUBOBJS=vfs_so.o
endif

OBJ= main.o	\
	daemon.o \
	fdinfo.o	\
	thread.o	\
	version.o	\
	watchdog.o	\
	mybuff.o \
	vfs_init.o \
	vfs_task.o \
	vfs_time_stamp.o \
	vfs_tmp_status.o \
	vfs_localfile.o \
	vfs_agent.o \
	vfs_timer.o \
	vfs_del_file.o \
	vfs_file_filter.o \
	vfs_maintain.o \
	parsepara.o

OBJ += $(SUBOBJS)
		
$(TARGET):$(OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
#	install $(TARGET) ../bin
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f $(OBJ)
	rm -f $(TARGET) 	
