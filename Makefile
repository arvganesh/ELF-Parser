parser.o:
	gcc -c -g -o parser.o parser.c

apager.o:
	gcc -c -g -o apager.o apager.c

apager: apager.o parser.o
	gcc -static apager.o parser.o -o apager -Wl,-T,linker_script

dpager.o:
	gcc -c -g -o dpager.o dpager.c

dpager: dpager.o parser.o
	gcc -static dpager.o parser.o -o dpager -Wl,-T,linker_script

helloworld-static:
	gcc -c -g -o helloworld.o helloworld.c
	gcc -static helloworld.o -o helloworld_static -Wl,-T,linker_script_test_prog

page_alloc-static:
	gcc -c -g -o page_alloc.o page_alloc.c
	gcc -static page_alloc.o -o page_alloc_static -Wl,-T,linker_script_test_prog

clean:
	rm *.o


