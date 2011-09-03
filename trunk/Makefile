all: lz4hcdemo64 lz4hcdemo32

lz4hcdemo64: mmc.c mmc.h lz4.c lz4.h lz4hc.c lz4hc.h main.c
	gcc -g -O3 -I. -Wall -W mmc.c lz4.c lz4hc.c main.c -o lz4hcdemo64.exe

lz4hcdemo32: mmc.c mmc.h lz4.c lz4.h lz4hc.c lz4hc.h main.c
	gcc -m32 -g -O3 -I. -Wall -W mmc.c lz4.c lz4hc.c main.c -o lz4hcdemo32.exe

clean:
	rm -f core *.o lz4hcdemo32.exe lz4hcdemo64.exe
