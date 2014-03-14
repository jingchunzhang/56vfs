SUBDIRS = lib 3rdlib network network/tracker network/cs network/fcs network/data network/voss network/http cdc network/cdc_http network/cdc_so
all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "Making all in $$list"; \
	(cd $$subdir && make); \
	done;

clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "Making all in $$list"; \
	(cd $$subdir && make clean); \
	done;

install:
	rm -rf /diska/vfs/*;
	mkdir /diska/vfs/bin -p;
	mkdir /diska/vfs/log -p;
	mkdir /diska/vfs/conf -p;
	mkdir /diska/vfs/data -p;
	mkdir /diska/vfs/path/tmpdir -p;
	mkdir /diska/vfs/path/outdir -p;
	mkdir /diska/vfs/path/datadir -p;
	mkdir /diska/vfs/path/workdir -p;
	mkdir /diska/vfs/path/indir -p;
	mkdir /diska/vfs/path/bkdir -p;
	mkdir /diska/vfs/path/delfile -p;
	cd network; cp vfs_master /diska/vfs/bin; cp vfs_master.conf /diska/vfs/conf; 
	cd network/tracker ; cp *.so /diska/vfs/bin
	cd network/cs; cp *.so /diska/vfs/bin
	cd network/data; cp *.so /diska/vfs/bin
	cp script/*.sh /diska/vfs/bin
	cd network/fcs; cp *.so /diska/vfs/bin
	cd network/voss; cp *.so /diska/vfs/bin

