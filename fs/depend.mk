testfs.o: testfs.c testfs.h common.h super.h inode.h list.h
mktestfs.o: mktestfs.c testfs.h common.h super.h dir.h inode.h list.h
bitmap.o: bitmap.c testfs.h common.h bitmap.h
block.o: block.c testfs.h common.h super.h
super.o: super.c testfs.h common.h bitmap.h super.h inode.h list.h \
 block.h
inode.o: inode.c testfs.h common.h super.h block.h inode.h list.h \
 read_write.h
read_write.o: read_write.c testfs.h common.h list.h super.h block.h \
 inode.h
dir.o: dir.c testfs.h common.h dir.h inode.h list.h read_write.h
file.o: file.c testfs.h common.h inode.h list.h dir.h read_write.h
common.o: common.c testfs.h common.h
