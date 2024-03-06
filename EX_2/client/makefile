all: client_a client_b

client_a : client_a.o
	gcc -o client_a client_a.o -lssl -lcrypto
	
client_a.o: client_a.c
	gcc -c client_a.c

client_b : client_b.o
	gcc -g -o client_b client_b.o -lssl -lcrypto

client_b.o:client_b.c
	gcc -c client_b.c


clean:
	rm -f *.o client_a client_b
