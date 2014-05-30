all: malloc_profile.so malloc_profile_test
.PHONY : all

malloc_profile.so: malloc_profile.c
	gcc -O2 -Wall -fPIC -std=gnu99 -o malloc_profile.so -shared malloc_profile.c -ldl -lrt -lunwind

clean:
	rm malloc_profile.so 2>/dev/null || /bin/true
	rm malloc_profile_test.o 2>/dev/null || /bin/true
	rm malloc_profile_wrapper.o 2>/dev/null || /bin/true
	rm malloc_profile_test 2>/dev/null || /bin/true

install: malloc_profile.so
	install -m 755 -p malloc_profile.so /usr/lib

malloc_profile_wrapper.o: malloc_profile_wrapper.c
	gcc -O2 -Wall -fPIC -std=gnu99 -c malloc_profile_wrapper.c

malloc_profile_test.o: malloc_profile_test.c
	gcc -O2 -Wall -fPIC -std=gnu99 -c malloc_profile_test.c

malloc_profile_test: malloc_profile.so malloc_profile_wrapper.o malloc_profile_test.o
	gcc -O2 -Wall -fPIC -std=gnu99 -o malloc_profile_test malloc_profile_wrapper.o malloc_profile_test.o -ldl -lrt


run: clean malloc_profile_test
	LD_PRELOAD="$$(pwd)/malloc_profile.so" ./malloc_profile_test
