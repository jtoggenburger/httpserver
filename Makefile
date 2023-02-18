CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wpedantic -Werror -Wextra 

all: httpserver

httpserver: httpserver.o http_helper.o
	$(CC) -o httpserver httpserver.o http_helper.o asgn2_helper_funcs.a -lm

httpserver.o : httpserver.c
	$(CC) $(CFLAGS) -c httpserver.c

http_helper.o : http_helper.c
	$(CC) $(CFLAGS) -c http_helper.c

clean:
	rm -f httpserver *.o

format:
	clang-format -i -style=file *.[ch]
