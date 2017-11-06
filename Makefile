CFLAGS?= -O3

all: fastcgi_parser.o

%.o: %.c
	$(CC) ${CFLAGS} -fPIC -c $^ -o $@
libfastcig_parser.so: kv_parser.o
	$(CC) -shared -o $@ $^
libfastcig_parser.a: kv_parser.o
	$(AR) rcs $@ $^

clean:
	rm -f *.o *.a *.so
fastcig_parser_test: fastcig_parser.o fastcig_parser_test.o
	$(CC) $^ -o $@
test: CFLAGS= -O0 -g
test: fastcig_parser_test
	./fastcig_parser_test
