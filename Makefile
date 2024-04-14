all: apager hpager dpager helloworld_static page_alloc_static

parser-apager.o: parser.c
	gcc -D APAGER -c -g -o parser-apager.o parser.c

apager.o: apager.c
	gcc -c -g -o apager.o apager.c

apager: apager.o parser-apager.o
	gcc -Wall -Werror -static apager.o parser-apager.o -o apager -Wl,-T,linker_script

parser-dpager.o: parser.c
	gcc -D DPAGER -c -g -o parser-dpager.o parser.c

dpager.o: dpager.c
	gcc -c -g -o dpager.o dpager.c

dpager: dpager.o parser-dpager.o
	gcc -static dpager.o parser-dpager.o -o dpager -Wl,-T,linker_script

parser-hpager.o: parser.c
	gcc -D HPAGER -c -g -o parser-hpager.o parser.c

hpager.o: hpager.c
	gcc -c -g -o hpager.o hpager.c

hpager: hpager.o parser-hpager.o
	gcc -static hpager.o parser-hpager.o -o hpager -Wl,-T,linker_script

helloworld_static: helloworld.c
	gcc -c -g -o helloworld.o helloworld.c
	gcc -static helloworld.o -o helloworld_static -Wl,-T,linker_script_test_prog

page_alloc_static: page_alloc.c
	gcc -c -g -o page_alloc.o page_alloc.c
	gcc -static page_alloc.o -o page_alloc_static -Wl,-T,linker_script_test_prog

clean:
	rm *.o
	rm *_static
	rm *pager


