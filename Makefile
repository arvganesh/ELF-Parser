compile:
	gcc -c -g -o apager.o parser.c
	gcc -static apager.o -o apager -Wl,-T,linker_script

test-static:
	gcc -c -g -o helloworld.o helloworld.c
	gcc -static helloworld.o -o helloworld_static -Wl,-T,linker_script_hello