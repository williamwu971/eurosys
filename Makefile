.PHONY: clean ralloc kv.out
LINKS = -I/mnt/nvme0/xiaoxiang/eurosys/ralloc/src /mnt/nvme0/xiaoxiang/eurosys/ralloc/test/libralloc.a -pthread
CC = g++

all: simple.out fs.out kv.out


simple.out: simple.c
	$(CC) simple.c -o build/simple.out $(LINKS)


fs.out: fs.c
	$(CC) fs.c -o build/fs.out $(LINKS)

clean:
	rm -rf /pmem0/* ./build/*

ralloc:
	cd ralloc/test && make clean && rm -f libralloc.a && make libralloc.a

kv.out:
	cd RECIPE/P-Masstree/build && make clean && rm -f example && cmake .. && make -j && cd - && mv RECIPE/P-Masstree/build/example ./build/kv.out
