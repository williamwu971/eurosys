.PHONY: clean ralloc kv.out
LINKS = -I/mnt/sdb/xiaoxiang/eurosys/ralloc/src /mnt/sdb/xiaoxiang/eurosys/ralloc/test/libralloc.a -pthread
CC = g++

all: ralloc.out kernel.out kv.out
#all: fs_expand.out fs_open.out

fs_open.out: fs_expand.c
	$(CC) fs_open.c -o build/fs_open.out $(LINKS)

fs_expand.out: fs_expand.c
	$(CC) fs_expand.c -o build/fs_expand.out $(LINKS)

ralloc.out: ralloc.c
	$(CC) ralloc.c -o build/ralloc.out $(LINKS)


kernel.out: kernel.c
	$(CC) kernel.c -o build/kernel.out $(LINKS)

clean:
	rm -rf /pmem0/* ./build/*

ralloc:
	cd ralloc/test && make clean && rm -f libralloc.a && make libralloc.a

kv.out:
	cd RECIPE/P-Masstree/build && make clean && rm -f example && cmake .. && make -j && cd - && mv RECIPE/P-Masstree/build/example ./build/kv.out

download:
	#scp 'xiaoxiang@labos2.cs.usyd.edu.au:/mnt/sdb/xiaoxiang/eurosys/*.pdf' ./pdfs/
	#scp 'xiaoxiang@labos2.cs.usyd.edu.au:/mnt/sdb/xiaoxiang/eurosys/build/*{csv,stat}' .
	scp 'xiaoxiang@labos0.cs.usyd.edu.au:/mnt/sdb/xiaoxiang/eurosys/build/*.csv' .
