SUBDIRS = lib core core/cdn_node core/cdn_source core/data_transfer core/voss cdc core/cdc_http core/cdc_so
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
	cd core; cp vfs_master /diska/vfs/bin; cp vfs_master.conf /diska/vfs/conf; cp fcs_list.txt /diska/vfs/bin
	cd core/tracker ; cp *.so /diska/vfs/bin
	cd core/cs; cp *.so /diska/vfs/bin
	cd core/data; cp *.so /diska/vfs/bin
	rm -rf /diska/vfs/csdir; cp -r csdir /diska/vfs/
	cp script/*.sh /diska/vfs/bin
	cd core/fcs; cp *.so /diska/vfs/bin
	cd core/voss; cp *.so /diska/vfs/bin

