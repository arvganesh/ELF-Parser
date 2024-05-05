LINK_SCRIPT_PATH = link_scripts/
TEST_FILE_PATH = test_files/

all: apager hpager dpager helloworld_static page_alloc_static simple_static mem_access_static

### LOADERS

## APAGER

parser-apager.o: parser.c
	gcc -D APAGER -c -g -o parser-apager.o parser.c

apager.o: apager.c
	gcc -c -g -o apager.o apager.c

apager: apager.o parser-apager.o
	gcc -Wall -Werror -static apager.o parser-apager.o -o apager -Wl,-T,$(LINK_SCRIPT_PATH)linker_script

## DPAGER

parser-dpager.o: parser.c
	gcc -D DPAGER -c -g -o parser-dpager.o parser.c

dpager.o: dpager.c
	gcc -c -g -o dpager.o dpager.c 

dpager: dpager.o parser-dpager.o
	gcc -static dpager.o parser-dpager.o -o dpager -Wl,-T,$(LINK_SCRIPT_PATH)linker_script -ggdb3 -Og

## HPAGER

parser-hpager.o: parser.c
	gcc -D HPAGER -c -g -o parser-hpager.o parser.c

hpager.o: hpager.c
	gcc -c -g -o hpager.o hpager.c

hpager: hpager.o parser-hpager.o
	gcc -static hpager.o parser-hpager.o -o hpager -Wl,-T,$(LINK_SCRIPT_PATH)linker_script

### TEST FILES

helloworld_static: $(TEST_FILE_PATH)helloworld.c
	gcc -c -g -o $(TEST_FILE_PATH)helloworld.o $(TEST_FILE_PATH)helloworld.c
	gcc -static $(TEST_FILE_PATH)helloworld.o -o helloworld_static -Wl,-T,$(LINK_SCRIPT_PATH)linker_script_test_prog -ggdb3 -Og

simple_static: $(TEST_FILE_PATH)simple_file.c
	gcc -c -g -o $(TEST_FILE_PATH)simple.o $(TEST_FILE_PATH)simple_file.c
	gcc -static $(TEST_FILE_PATH)simple.o -o $(TEST_FILE_PATH)simple_static -Wl,-T,$(LINK_SCRIPT_PATH)linker_script_test_prog -ggdb3 -Og

mem_access_static: $(TEST_FILE_PATH)mem_access_err.c
	gcc -c -g -o $(TEST_FILE_PATH)mem_access_err.o $(TEST_FILE_PATH)mem_access_err.c
	gcc -static $(TEST_FILE_PATH)mem_access_err.o -o $(TEST_FILE_PATH)mem_access_err -Wl,-T,$(LINK_SCRIPT_PATH)linker_script_test_prog -ggdb3 -Og

page_alloc_static: $(TEST_FILE_PATH)page_alloc.c
	gcc -c -g -o $(TEST_FILE_PATH)page_alloc.o $(TEST_FILE_PATH)page_alloc.c
	gcc -static $(TEST_FILE_PATH)page_alloc.o -o $(TEST_FILE_PATH)page_alloc_static -Wl,-T,$(LINK_SCRIPT_PATH)linker_script_test_prog

## CLEANING

clean:
	rm $(TEST_FILE_PATH)*.o
	rm $(TEST_FILE_PATH)*_static
	rm *pager


