libtcp_listener.a: libtcp_listener.o
	ar rcs $@ $^

libtcp_listener.o: tcp_listener.c
	gcc -c -o $@ $<

clean:
	rm -f *.o *.a
